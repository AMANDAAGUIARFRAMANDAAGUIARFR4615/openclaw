#pragma once

#include "NetworkUtils.h"
#include "AesCrypto.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QJsonDocument>
#include <QHostInfo>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
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

        qDebugEx() << "服务器已启动,监听端口:" << serverPort();
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
        return QJsonObject{{"version", "1.0"}, {"ip", ip}, {"port", serverPort()}, {"remoteDeviceName", QHostInfo::localHostName()}};
    }

signals:
    void clientConnected(QTcpSocket* socket);
    void dataReceived(QTcpSocket* socket, const QJsonObject& jsonObject);
    void clientDisconnected(QTcpSocket* socket);
    void clientError(QTcpSocket* socket, QAbstractSocket::SocketError error);

private slots:
    void onNewConnection() {
        auto socket = this->nextPendingConnection();

        qintptr fd = socket->socketDescriptor();

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&opt, sizeof(opt));

#ifdef _WIN32
        struct tcp_keepalive alive;
        DWORD bytesReturned;
        alive.onoff = 1;
        alive.keepalivetime = 3000;    // 空闲多久后首次发送 KeepAlive 探针
        alive.keepaliveinterval = 200; // 每次探针间隔（Windows 固定重试 10 次）

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

        clientBuffers[socket] = QByteArray();
        
        emit clientConnected(socket);
    }

    void onReadyRead() {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        auto data = socket->readAll();
        clientBuffers[socket].append(data);
        processBufferedData(socket);
    }

    void onDisconnected() {
        auto socket = qobject_cast<QTcpSocket*>(sender());
    
        auto ip = socket->peerAddress().toString();
        auto port = socket->peerPort();

        qDebugEx() << "连接断开" << ip + ":" + QString::number(port);

        clientBuffers.remove(socket);
        
        emit clientDisconnected(socket);
        
        socket->deleteLater();
    }

    void onErrorOccurred(QAbstractSocket::SocketError error) {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        qCriticalEx() << "onErrorOccurred" << error << socket->errorString();
        
        emit clientError(socket, error);
    }

private:
    QMap<QTcpSocket*, QByteArray> clientBuffers; // 保存每个客户端的缓冲区
 
    void processBufferedData(QTcpSocket* socket) {
        auto &buffer = clientBuffers[socket];

        while (buffer.size() >= sizeof(quint64) + sizeof(quint32)) {
            auto identifier = *reinterpret_cast<quint64*>(buffer.data());
            if (identifier != 0xb7c2e0f542a39a3e) {
                qCriticalEx() << "识别码不匹配，丢弃数据" << QString("0x%1").arg(identifier, 0, 16);
                buffer.clear(); // 清空缓冲区
                return;
            }

            auto size = *reinterpret_cast<quint32*>(buffer.data() + sizeof(quint64));

            if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + size)) {
                // qDebugEx() << "数据不完整，等待更多数据";
                return;
            }

            const auto& data = buffer.mid(sizeof(quint64) + sizeof(quint32), size);
            // 移除已处理的数据包
            buffer.remove(0, sizeof(quint64) + sizeof(quint32) + size);

            const auto& jsonData = AesCrypto::decrypt(data);
            if (jsonData.size() == 0) {
                // qCriticalEx() << "解密失败";
                return;
            }

            const auto& doc = QJsonDocument::fromJson(jsonData);
            
            if (!doc.isNull())
                emit dataReceived(socket, doc.object());
            else
                qCriticalEx() << "JSON 解析失败，丢弃数据";
        }
    }
};
