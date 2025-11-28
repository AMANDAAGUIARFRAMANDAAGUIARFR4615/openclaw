#pragma once

#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHash>
#include <functional>

using AckCallback = std::function<void(const QJsonValue &)>;
using EventHandler = std::function<void(const QJsonValue &, AckCallback)>;

class SocketClient : public QWebSocket
{
    Q_OBJECT
public:
    explicit SocketClient(QObject *parent = nullptr) : QWebSocket(QString(), QWebSocketProtocol::VersionLatest, parent) {
        connect(this, &QWebSocket::textMessageReceived, this, &SocketClient::handleMessage);
    }

    void on(const QString &event, EventHandler handler) {
        m_handlers[event] = handler;
    }

    void emitEvent(const QString &event, const QJsonValue &data) {
        sendJson({{"type", "event"}, {"name", event}, {"data", data}});
    }

    void emitEvent(const QString &event, const QJsonValue &data, AckCallback cb) {
        int id = ++m_nextId;
        m_pendingAcks[id] = cb;
        sendJson({{"type", "event"}, {"name", event}, {"data", data}, {"id", id}});
    }

private:
    QHash<QString, EventHandler> m_handlers;
    QHash<int, AckCallback> m_pendingAcks;
    int m_nextId = 0;

    void sendJson(const QJsonObject &obj) {
        sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }

    void handleMessage(const QString &msg) {
        QJsonObject obj = QJsonDocument::fromJson(msg.toUtf8()).object();
        QString type = obj["type"].toString();
        QJsonValue data = obj["data"];

        if (type == "ack") {
            int id = obj["id"].toInt();
            if (m_pendingAcks.contains(id)) {
                auto cb = m_pendingAcks.take(id);
                if (cb) cb(data);
            }
        } else if (type == "event") {
            QString name = obj["name"].toString();
            if (m_handlers.contains(name)) {
                // 如果对方发来了id，说明需要回执
                QJsonValue idVal = obj["id"];
                m_handlers[name](data, [this, idVal](const QJsonValue &respData) {
                    if (!idVal.isUndefined()) {
                        sendJson({{"type", "ack"}, {"id", idVal}, {"data", respData}});
                    }
                });
            }
        }
    }
};