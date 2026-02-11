#pragma once

#include "AesCrypto.h"
#include "Safe.h"
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHash>
#include <QPointer>
#include <QTimer>

using AckCallback = std::function<void(const QJsonValue &)>;
using EventHandler = std::function<void(const QJsonValue &, AckCallback)>;

class WebSocketClient : public QWebSocket
{
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr) : QWebSocket(QString(), QWebSocketProtocol::VersionLatest, parent) {
        connect(this, &QWebSocket::binaryMessageReceived, this, &WebSocketClient::handleMessage, Qt::QueuedConnection);
        connect(this, &QWebSocket::disconnected, [this]() {
            qDebugEx() << "QWebSocket::disconnected";

            QTimer::singleShot(2000, [=]() {
                open(requestUrl());
            });
        });
    }

    template <typename Func>
    void on(const DataGuard::StrObfuscator<>& event, Func handler) {
        if constexpr (std::is_invocable_v<Func, QJsonValue, AckCallback>) {
            m_handlers[event.decrypt()] = handler;
        } else {
            m_handlers[event.decrypt()] = [handler](const QJsonValue &data, AckCallback) {
                handler(data);
            };
        }
    }

    void emitEvent(const DataGuard::StrObfuscator<>& event, const QJsonValue &data) {
        sendJson({{"type", "event"}, {"event", event.decrypt()}, {"data", data}});
    }

    void emitEvent(const DataGuard::StrObfuscator<>& event, const QJsonValue &data, AckCallback cb) {
        int id = ++m_nextId;
        m_pendingAcks[id] = cb;
        sendJson({{"type", "event"}, {"event", event.decrypt()}, {"data", data}, {"id", id}});
    }

private:
    QHash<QString, EventHandler> m_handlers;
    QHash<int, AckCallback> m_pendingAcks;
    int m_nextId = 0;

    void sendJson(const QJsonObject &obj) {
        if (state() != QAbstractSocket::ConnectedState) {
            qCriticalEx() << "不是连接状态，无法发送数据";
            return;
        }

        qDebugEx() << this << obj;
        const auto& jsonData = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        sendBinaryMessage(AesCrypto::encrypt(jsonData));
    }

    void handleMessage(const QByteArray &message) {
        const auto& jsonData = AesCrypto::decrypt(message);
        if (jsonData.size() == 0) {
            qCriticalEx() << "解密失败";
            return;
        }

        const auto& doc = QJsonDocument::fromJson(jsonData);
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
