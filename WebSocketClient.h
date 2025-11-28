#pragma once

#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHash>
#include <QPointer>
#include <functional>

using AckCallback = std::function<void(const QJsonValue &)>;
using EventHandler = std::function<void(const QJsonValue &, AckCallback)>;

class WebSocketClient : public QWebSocket
{
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr) : QWebSocket(QString(), QWebSocketProtocol::VersionLatest, parent) {
        connect(this, &QWebSocket::textMessageReceived, this, &WebSocketClient::handleMessage);
    }

    template <typename Func>
    void on(const QString &event, Func handler) {
        if constexpr (std::is_invocable_v<Func, QJsonValue, AckCallback>) {
            m_handlers[event] = handler;
        } else {
            m_handlers[event] = [handler](const QJsonValue &data, AckCallback) {
                handler(data);
            };
        }
    }

    void emitEvent(const QString &event, const QJsonValue &data) {
        sendJson({{"type", "event"}, {"event", event}, {"data", data}});
    }

    void emitEvent(const QString &event, const QJsonValue &data, AckCallback cb) {
        int id = ++m_nextId;
        m_pendingAcks[id] = cb;
        sendJson({{"type", "event"}, {"event", event}, {"data", data}, {"id", id}});
    }

private:
    QHash<QString, EventHandler> m_handlers;
    QHash<int, AckCallback> m_pendingAcks;
    int m_nextId = 0;

    void sendJson(const QJsonObject &obj) {
        sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }

    void handleMessage(const QString &msg) {
        QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
        if (!doc.isObject()) return;

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();
        QJsonValue data = obj["data"];

        if (type == "ack") {
            int id = obj["id"].toInt();
            if (m_pendingAcks.contains(id)) {
                auto cb = m_pendingAcks.take(id);
                if (cb) cb(data);
            }
        } else if (type == "event") {
            QString event = obj["event"].toString();
            if (m_handlers.contains(event)) {
                QJsonValue idVal = obj["id"];
                QPointer<WebSocketClient> self = this;

                m_handlers[event](data, [self, idVal](const QJsonValue &respData) {
                    if (self && !idVal.isUndefined() && !idVal.isNull()) {
                        self->sendJson({{"type", "ack"}, {"id", idVal}, {"data", respData}});
                    }
                });
            }
        }
    }
};