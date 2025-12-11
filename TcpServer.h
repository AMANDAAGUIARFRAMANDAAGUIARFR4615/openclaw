#pragma once

#include "NetworkUtils.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <functional>
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
    TcpServer(const std::function<void(QTcpSocket*)> &onClientConnected,
              const std::function<void(QTcpSocket*, const QJsonObject&)> &onDataReceived,
              const std::function<void(QTcpSocket*)> &onClientDisconnected,
              const std::function<void(QTcpSocket*, QAbstractSocket::SocketError)> &onError,
              quint16 port = 0)
    : onDataReceivedCallback(onDataReceived),
      onClientConnectedCallback(onClientConnected),
      onClientDisconnectedCallback(onClientDisconnected),
      onErrorCallback(onError)
    {
        instance = this;

        connect(this, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

        if (!this->listen(QHostAddress::Any, port)) {
            qCriticalEx() << "无法启动服务器" << port;
            return;
        }

        qDebugEx() << "服务器已启动,监听端口:" << serverPort();
    }

    static TcpServer* getInstance() {return instance;}

    QJsonObject getHostInfo(const QString& ip) {
        return QJsonObject{{"ip", ip}, {"port", serverPort()}, {"remoteDeviceName", QHostInfo::localHostName()}};
    }

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
        if (onClientConnectedCallback) {
            onClientConnectedCallback(socket);
        }
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
        if (onClientDisconnectedCallback) {
            onClientDisconnectedCallback(socket);
        }
        socket->deleteLater();
    }

    void onErrorOccurred(QAbstractSocket::SocketError socketError) {
        auto socket = qobject_cast<QTcpSocket*>(sender());
        qCriticalEx() << "Socket error:" << socketError;
        if (onErrorCallback && socket) {
            onErrorCallback(socket, socketError);
        }
    }

private:
    inline static TcpServer* instance;

    QMap<QTcpSocket*, QByteArray> clientBuffers; // 保存每个客户端的缓冲区
    std::function<void(QTcpSocket*)> onClientConnectedCallback;
    std::function<void(QTcpSocket*, const QJsonObject&)> onDataReceivedCallback;
    std::function<void(QTcpSocket*)> onClientDisconnectedCallback;
    std::function<void(QTcpSocket*, QAbstractSocket::SocketError)> onErrorCallback;

    void processBufferedData(QTcpSocket* socket) {
        auto &buffer = clientBuffers[socket];

        while (buffer.size() >= sizeof(quint64) + sizeof(quint32)) {
            auto identifier = *reinterpret_cast<quint64*>(buffer.data());
            if (identifier != 0xb7c2e0f542a39a3e) {
                qCriticalEx() << "识别码不匹配，丢弃数据" << QString("0x%1").arg(identifier, 0, 16);
                buffer.clear(); // 清空缓冲区
                return;
            }

            auto jsonDataLength = *reinterpret_cast<quint32*>(buffer.data() + sizeof(quint64));

            if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + jsonDataLength)) {
                // qDebugEx() << "数据不完整，等待更多数据" << buffer.size() << sizeof(quint64) + sizeof(quint32) + jsonDataLength;
                return;
            }

            QByteArray jsonData = buffer.mid(sizeof(quint64) + sizeof(quint32), jsonDataLength);
            // 移除已处理的数据包
            buffer.remove(0, sizeof(quint64) + sizeof(quint32) + jsonDataLength);
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            
            if (!doc.isNull()) {
                if (onDataReceivedCallback) {
                    onDataReceivedCallback(socket, doc.object());
                }
            } else {
                qCriticalEx() << "JSON 解析失败，丢弃数据";
            }
        }
    }
};
