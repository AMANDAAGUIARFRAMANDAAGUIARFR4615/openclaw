#pragma once

#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QUuid>
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
        QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_pendingAcks[id] = cb;
        sendJson({{"type", "event"}, {"name", event}, {"data", data}, {"id", id}});
    }

private:
    QMap<QString, EventHandler> m_handlers;
    QMap<QString, AckCallback> m_pendingAcks;

    void sendJson(const QJsonObject &obj) {
        sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }

    void handleMessage(const QString &msg) {
        QJsonObject obj = QJsonDocument::fromJson(msg.toUtf8()).object();
        QString type = obj["type"].toString();
        QString id = obj["id"].toString();
        QJsonValue data = obj["data"];

        if (type == "ack") {
            if (m_pendingAcks.contains(id)) {
                auto cb = m_pendingAcks.take(id);
                if (cb) cb(data);
            }
        } else if (type == "event") {
            QString name = obj["name"].toString();
            if (m_handlers.contains(name)) {
                m_handlers[name](data, [this, id](const QJsonValue &respData) {
                    if (!id.isEmpty()) sendJson({{"type", "ack"}, {"id", id}, {"data", respData}});
                });
            }
        }
    }
};
