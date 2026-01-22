#pragma once

#include "NetworkUtils.h"
#include "AesCrypto.h"
#include "DeviceConnection.h"
#include "Safe.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QHash>
#include <QJsonDocument>
#include <QHostInfo>

#if defined(_WIN32)
#include <winsock2.h>
// #include <ws2tcpip.h>
#include <mstcpip.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#endif

class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr) : QTcpServer(parent)
    {
        connect(this, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

        if (!this->listen(QHostAddress::Any, 0)) {
            qCriticalEx() << "无法启动服务器";
            return;
        }

        qInfoEx() << "服务器已启动,监听端口:" << serverPort();
    }

    static TcpServer* getInstance() { static TcpServer instance; return &instance; }

    QSet<QString> getConnectedIps() const {
        QSet<QString> ips;
        for (auto socket : clientBuffers.keys()) {
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                auto ip = socket->peerAddress().toString();

                if (ip.startsWith("::ffff:"))
                    ip = ip.mid(7);

                ips.insert(ip);
            }
        }
        return ips;
    }

    QJsonObject getHostInfo(const QString& ip) {
        return QJsonObject{{"version", Config::VERSION}, {"ip", ip}, {"port", serverPort()}, {"remoteDeviceName", QHostInfo::localHostName()}};
    }

signals:
    void clientConnected(DeviceConnection* connection);
    void dataReceived(DeviceConnection* connection, const QJsonObject& jsonObject);
    void clientDisconnected(DeviceConnection* connection);
    void clientError(DeviceConnection* connection, QAbstractSocket::SocketError error);

private slots:
    void onNewConnection() {
        while (this->hasPendingConnections()) {
            auto socket = this->nextPendingConnection();
            if (!socket) continue;

            qintptr fd = socket->socketDescriptor();

            int opt = 1;
            setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&opt, sizeof(opt));

#ifdef _WIN32
            struct tcp_keepalive alive;
            DWORD bytesReturned;
            alive.onoff = 1;
            alive.keepalivetime = 3000;
            alive.keepaliveinterval = 200;
            WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive),
                     nullptr, 0, &bytesReturned, nullptr, nullptr);
#elif defined(__APPLE__)
            int idle = 3;
            int interval = 1;
            int count = 3;
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE,  &idle,     sizeof(idle));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL,  &interval, sizeof(interval));
            setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,    &count,    sizeof(count));
#endif

            auto ip = socket->peerAddress().toString();
            auto port = socket->peerPort();

            qDebugEx() << "连接成功" << ip + ":" + QString::number(port);

            connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
            connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
            connect(socket, &QTcpSocket::errorOccurred, this, &TcpServer::onErrorOccurred);

            clientBuffers.insert(socket, QByteArray());
            
            auto connection = new DeviceConnection(socket);
            connections.insert(socket, connection);
            
            emit clientConnected(connection);
        }
    }

    void onReadyRead() {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        if (!socket) return;

        if (!clientBuffers.contains(socket)) return;

        auto data = socket->readAll();
        if (data.isEmpty()) return;

        clientBuffers[socket].append(data);
        processBufferedData(socket);
    }

    void onDisconnected() {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        if (!socket) return;
    
        auto ip = socket->peerAddress().toString();
        auto port = socket->peerPort();

        qDebugEx() << "连接断开" << ip + ":" + QString::number(port);

        auto connection = connections.value(socket);

        clientBuffers.remove(socket);
        connections.remove(socket);
        
        if (connection)
            emit clientDisconnected(connection);
        
        socket->deleteLater();
    }

    void onErrorOccurred(QAbstractSocket::SocketError error) {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        if (!socket) return;

        qCriticalEx() << "onErrorOccurred" << error << socket->errorString();
        
        auto connection = connections.value(socket);
        if (connection)
            emit clientError(connection, error);
    }

private:
    QHash<QTcpSocket*, QByteArray> clientBuffers;
    QHash<QTcpSocket*, DeviceConnection*> connections;
 
    void processBufferedData(QTcpSocket* socket) {
        while (clientBuffers.contains(socket)) {
            auto &buffer = clientBuffers[socket];

            if (buffer.size() < sizeof(quint64) + sizeof(quint32))
                return;

            auto identifier = *reinterpret_cast<const quint64*>(buffer.constData());
            if (identifier != 0xb7c2e0f542a39a3e) {
                qCriticalEx() << HIDE("识别码不匹配，丢弃数据");
                buffer.clear();
                return;
            }

            auto size = *reinterpret_cast<const quint32*>(buffer.constData() + sizeof(quint64));

            if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + size)) {
                // qDebugEx() << "数据不完整，等待更多数据";
                return;
            }

            const auto& data = buffer.mid(sizeof(quint64) + sizeof(quint32), size);
            buffer.remove(0, sizeof(quint64) + sizeof(quint32) + size);

            const auto& jsonData = AesCrypto::decrypt(data);
            if (jsonData.size() == 0) {
                qDebugEx() << HIDE("解密失败");
                continue;
            }

            const auto& doc = QJsonDocument::fromJson(jsonData);
            
            if (!doc.isNull()) {
                auto connection = connections.value(socket);
                if (connection)
                    emit dataReceived(connection, doc.object());
            } else {
                qCriticalEx() << HIDE("JSON 解析失败，丢弃数据");
            }
        }
    }
};
