#pragma once

#include "Account.h"
#include <QWebSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>
#include <QHash>

// ==========================================
// DataBridge：TCP 对撞实现
// ==========================================
class DataBridge : public QObject {
    Q_OBJECT
public:
    // 高水位：单方向 Qt 应用层堆积超过这个值就暂停从对端读取，等下游消化完再恢复。
    // 防止屏幕流/大文件传输时客户端内存膨胀以及越来越大的延迟。
    static constexpr qint64 kHighWater = 1 << 20;        // 1 MiB
    static constexpr qint64 kLowWater  = kHighWater / 2; // 512 KiB
    // 小于此阈值的本次写入才主动 flush，避免大块流量上每个 chunk 都进一次系统调用。
    static constexpr int kFlushSmallThreshold = 1500;

    DataBridge(const QString &sessionId, const QString &lanIp, quint16 lanPort,
               const QString &serverIp, quint16 transferPort, QObject *parent = nullptr)
        : QObject(parent), m_sessionId(sessionId), m_serverIp(serverIp), m_transferPort(transferPort)
    {
        m_lanSocket = new QTcpSocket(this);
        m_serverSocket = new QTcpSocket(this);

        // 限制 Qt 内部读缓冲，间接对内核接收窗口形成约束，避免在被暂停期间无限囤积。
        m_lanSocket->setReadBufferSize(kHighWater);
        m_serverSocket->setReadBufferSize(kHighWater);

        connect(m_lanSocket, &QTcpSocket::disconnected, this, &DataBridge::cleanup);
        connect(m_serverSocket, &QTcpSocket::disconnected, this, &DataBridge::cleanup);
        connect(m_lanSocket, &QTcpSocket::errorOccurred, this, &DataBridge::cleanup);
        connect(m_serverSocket, &QTcpSocket::errorOccurred, this, &DataBridge::cleanup);

        // 🟢 第一步：只监听 LAN 的连接成功
        connect(m_lanSocket, &QTcpSocket::connected, this, &DataBridge::onLanConnected);

        // 关闭 Nagle 算法：投屏鼠标/触摸事件多为高频小包，
        // 一旦在广域网下被 Nagle 合并，会出现"必须大幅滑动才能触发系统手势"的问题。
        m_lanSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        m_serverSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        // 发起 LAN 连接
        m_lanSocket->connectToHost(lanIp, lanPort);
    }

private slots:
    void onLanConnected() {
        // 🟢 第二步：LAN 连上了，现在去连 Server。
        // 注意：此时故意 *不绑定* m_lanSocket 的 readyRead 信号！
        // 如果 LAN 发来了 SSH 欢迎语，它会安安静静地躺在 m_lanSocket 的接收缓冲区里。
        connect(m_serverSocket, &QTcpSocket::connected, this, &DataBridge::onServerConnected);
        m_serverSocket->connectToHost(m_serverIp, m_transferPort);
    }

    void onServerConnected() { 
        // 🟢 第三步：Server 连上了！这是最关键的时序控制点！

        // 1. 绝对优先：先把 36 字节的 UUID 暗号发给 Node.js
        m_serverSocket->write(m_sessionId.toUtf8()); 
        
        // 2. 检查 LAN 在等待期间是不是已经“抢跑”发了数据（比如 SSH Banner）
        // 如果有，紧跟在 UUID 后面发过去。Node.js 那边只要前 36 字节对上，后面这些会完美 pipe 过去。
        if (m_lanSocket->bytesAvailable() > 0) {
            m_serverSocket->write(m_lanSocket->readAll());
        }

        // 3. 暗号对接完毕，历史数据清理完毕，正式打通双向实时数据通道！
        connect(m_lanSocket, &QTcpSocket::readyRead, this, &DataBridge::onLanReadyRead);
        connect(m_serverSocket, &QTcpSocket::readyRead, this, &DataBridge::onServerReadyRead);

        // 背压恢复信号：下游写出后检查是否可以解除上游暂停
        connect(m_serverSocket, &QTcpSocket::bytesWritten, this, &DataBridge::onServerBytesWritten);
        connect(m_lanSocket,    &QTcpSocket::bytesWritten, this, &DataBridge::onLanBytesWritten);
    }

    void onLanReadyRead() {
        if (m_lanPaused) return; // 已暂停，等下游消化
        QByteArray data = m_lanSocket->readAll();
        if (data.isEmpty()) return;
        m_serverSocket->write(data);
        // 仅交互小包付 flush 代价；大块流量交给 TCP/系统决定，节省 syscall
        if (data.size() < kFlushSmallThreshold) m_serverSocket->flush();
        if (m_serverSocket->bytesToWrite() > kHighWater) m_lanPaused = true;
    }

    void onServerReadyRead() {
        if (m_serverPaused) return;
        QByteArray data = m_serverSocket->readAll();
        if (data.isEmpty()) return;
        m_lanSocket->write(data);
        if (data.size() < kFlushSmallThreshold) m_lanSocket->flush();
        if (m_lanSocket->bytesToWrite() > kHighWater) m_serverPaused = true;
    }

    void onServerBytesWritten() {
        // server 下游消化到低水位，恢复从 LAN 端读取
        if (m_lanPaused && m_serverSocket->bytesToWrite() < kLowWater) {
            m_lanPaused = false;
            if (m_lanSocket->bytesAvailable() > 0) onLanReadyRead();
        }
    }

    void onLanBytesWritten() {
        if (m_serverPaused && m_lanSocket->bytesToWrite() < kLowWater) {
            m_serverPaused = false;
            if (m_serverSocket->bytesAvailable() > 0) onServerReadyRead();
        }
    }

    void cleanup() {
        disconnect(m_lanSocket, nullptr, this, nullptr);
        disconnect(m_serverSocket, nullptr, this, nullptr);
        if (m_lanSocket->isOpen()) m_lanSocket->close();
        if (m_serverSocket->isOpen()) m_serverSocket->close();
        this->deleteLater();
    }

private:
    QTcpSocket *m_lanSocket;
    QTcpSocket *m_serverSocket;
    QString m_sessionId;
    QString m_serverIp;
    quint16 m_transferPort;
    bool m_lanPaused = false;     // 暂停从 LAN 读：server 下游堆积过多
    bool m_serverPaused = false;  // 暂停从 Server 读：LAN 下游堆积过多
};

// ==========================================
// TunnelClient：控制层实现
// ==========================================
class TunnelClient : public QObject {
    Q_OBJECT
public:
    explicit TunnelClient(const QString &serverIp, quint16 wsPort = 10000, quint16 transferPort = 10001, QObject *parent = nullptr)
        : QObject(parent), m_serverIp(serverIp), m_wsPort(wsPort), m_transferPort(transferPort)
    {
        m_reconnectTimer.setSingleShot(true);
        m_reconnectTimer.setInterval(5000); 

        connect(&m_ws, &QWebSocket::connected, this, &TunnelClient::onWsConnected);
        connect(&m_ws, &QWebSocket::disconnected, this, &TunnelClient::onWsDisconnected);
        connect(&m_ws, &QWebSocket::textMessageReceived, this, &TunnelClient::onWsTextMessageReceived);
        connect(&m_ws, &QWebSocket::errorOccurred, this, &TunnelClient::onWsError);
        connect(&m_reconnectTimer, &QTimer::timeout, this, &TunnelClient::connectToServer);
    }

    void connectToServer() {
        if (m_ws.state() == QAbstractSocket::ConnectedState || m_ws.state() == QAbstractSocket::ConnectingState) return;
        
        QString wsUrl = QString("ws://%1:%2").arg(m_serverIp).arg(m_wsPort);
        qInfo() << "[Client] Connecting to" << wsUrl << "...";
        m_ws.open(QUrl(wsUrl));
    }

    void requestAdd(const QString &lanIp, quint16 lanPort, quint16 remotePort = 0, const QString &protocol = QStringLiteral("tcp")) {
        if (m_ws.state() != QAbstractSocket::ConnectedState) {
            for (const QJsonObject &p : m_pendingMessages) {
                if (p["type"].toString() == "ADD" && p["lanIp"].toString() == lanIp && p["lanPort"].toInt() == lanPort
                    && p["protocol"].toString() == protocol) {
                    return; // 已经在等待发送队列中
                }
            }
        }

        QJsonObject msg;
        msg["type"] = "ADD";
        msg["lanIp"] = lanIp;
        msg["lanPort"] = lanPort;
        msg["udid"] = Account::getInstance()->id;
        msg["protocol"] = protocol;
        if (remotePort > 0) {
            msg["remotePort"] = remotePort;
        }
        sendWsMsg(msg);
    }

    void requestRemove(const QString &lanIp, quint16 lanPort) {
        QString targetMappingId;
        for (auto it = m_activeMappings.begin(); it != m_activeMappings.end(); ++it) {
            QJsonObject m = it.value();
            if (m["lanIp"].toString() == lanIp && m["lanPort"].toInt() == lanPort) {
                targetMappingId = it.key();
                break;
            }
        }

        if (!targetMappingId.isEmpty()) {
            if (m_ws.state() != QAbstractSocket::ConnectedState) {
                // 断网时直接在本地清理掉，不要进等待队列。
                // 否则重连时它会被当成 previousMappings 再次复活！
                m_activeMappings.remove(targetMappingId);
                qInfo().noquote() << "🚫 Mapping Removed locally (offline):" << lanIp << ":" << lanPort;
                return;
            }

            QJsonObject msg;
            msg["type"] = "REMOVE";
            msg["mappingId"] = targetMappingId;
            sendWsMsg(msg);
        } else {
            // 如果 active 里找不到，可能是在断网期间刚刚请求 ADD，还在 pending 队列里
            for (int i = 0; i < m_pendingMessages.size(); ++i) {
                QJsonObject m = m_pendingMessages[i];
                if (m["type"].toString() == "ADD" && m["lanIp"].toString() == lanIp && m["lanPort"].toInt() == lanPort) {
                    m_pendingMessages.removeAt(i);
                    qInfo().noquote() << "🚫 Pending ADD Removed locally:" << lanIp << ":" << lanPort;
                    break;
                }
            }
        }
    }

signals:
    void remotePortChanged(const QString &lanIp, quint16 lanPort, quint16 remotePort);

private slots:
    void onWsConnected() {
        qInfo() << "🚀 Connected to Server";
        m_reconnectTimer.stop();

        // 恢复断线前的映射，并请求原有的 remotePort
        QList<QJsonObject> previousMappings = m_activeMappings.values();
        m_activeMappings.clear(); // 清空旧 ID，等待服务端下发新的

        if (!previousMappings.isEmpty()) {
            qInfo() << "[Client] Restoring" << previousMappings.size() << "mappings...";
            const QString accountId = Account::getInstance()->id;
            for (const QJsonObject &m : previousMappings) {
                if (!m["udid"].toString().isEmpty() && m["udid"].toString() != accountId)
                    continue;
                const QString protocol = m["protocol"].toString(QStringLiteral("tcp"));
                requestAdd(m["lanIp"].toString(), m["lanPort"].toInt(), m["remotePort"].toInt(), protocol);
            }
        }

        // 发送断线期间新增的积压请求
        if (!m_pendingMessages.isEmpty()) {
            qInfo() << "[Client] Sending" << m_pendingMessages.size() << "queued messages...";
            for (const QJsonObject &msg : m_pendingMessages) {
                QJsonDocument doc(msg);
                m_ws.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
            }
            m_pendingMessages.clear();
        }
    }

    void onWsDisconnected() { scheduleReconnect(); }

    void scheduleReconnect() {
        if (!m_reconnectTimer.isActive()) {
            qWarning() << "⚠️ Connection lost. Retrying in 5s...";
            m_reconnectTimer.start();
        }
    }

    void onWsError(QAbstractSocket::SocketError error) {
        Q_UNUSED(error); 
        qCritical() << "[WS Error]:" << m_ws.errorString();
        m_ws.close();
        // 连接失败时可能不会触发 disconnected，必须在这里强制调起重连
        scheduleReconnect(); 
    }

    void onWsTextMessageReceived(const QString &message) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            handleCommand(doc.object());
        } else {
            qWarning() << "[Client] Failed to parse message:" << err.errorString();
        }
    }

private:
    void handleCommand(const QJsonObject &msg) {
        QString type = msg["type"].toString();

        if (type == "ADD_DONE") {
            QString mappingId = msg["mappingId"].toString();
            QString lanIp = msg["lanIp"].toString();
            int lanPort = msg["lanPort"].toInt();
            int remotePort = msg["remotePort"].toInt();
            const QString udid = msg["udid"].toString();
            if (!udid.isEmpty() && udid != Account::getInstance()->id)
                return;

            m_activeMappings[mappingId] = msg;
            qInfo().noquote() << QString("✨ Live: %1:%2 <-> %3:%4").arg(lanIp).arg(lanPort).arg(m_serverIp).arg(remotePort);
            emit remotePortChanged(lanIp, static_cast<quint16>(lanPort), static_cast<quint16>(remotePort));
        } 
        else if (type == "REQ_TUNNEL") {
            QString sessionId = msg["sessionId"].toString();
            QString lanIp = msg["lanIp"].toString();
            int lanPort = msg["lanPort"].toInt();
            if (!sessionId.isEmpty() && !lanIp.isEmpty() && lanPort > 0) {
                new DataBridge(sessionId, lanIp, lanPort, m_serverIp, m_transferPort, this);
            }
        } 
        else if (type == "REMOVE_DONE") {
            QString mappingId = msg["mappingId"].toString();
            m_activeMappings.remove(mappingId);
            qInfo().noquote() << "🚫 Mapping Removed:" << mappingId;
        } 
        else if (type == "ERROR") {
            qCritical().noquote() << "❌ Server Error:" << msg["message"].toString();
        }
    }

    void sendWsMsg(const QJsonObject &msg) {
        if (m_ws.state() == QAbstractSocket::ConnectedState) {
            QJsonDocument doc(msg);
            m_ws.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        } else {
            qDebug() << "[Client] WebSocket not ready. Message queued.";
            m_pendingMessages.append(msg);
            connectToServer(); // 尝试触发连接
        }
    }

    QString m_serverIp;
    quint16 m_wsPort;
    quint16 m_transferPort;
    
    QWebSocket m_ws;
    QTimer m_reconnectTimer;
    
    QList<QJsonObject> m_pendingMessages; 
    QHash<QString, QJsonObject> m_activeMappings; 
};
