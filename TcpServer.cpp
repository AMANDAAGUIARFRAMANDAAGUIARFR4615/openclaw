#include "TcpServer.h"
#include "Logger.h"
#include <QDebug>
#include <QJsonDocument>

TcpServer::TcpServer(const std::function<void(QTcpSocket*)> &onClientConnected,
                     const std::function<void(QTcpSocket*, const QJsonObject&)> &onDataReceived,
                     const std::function<void(QTcpSocket*)> &onClientDisconnected,
                     const std::function<void(QTcpSocket*, QAbstractSocket::SocketError)> &onError,
                     quint16 port)
    : onDataReceivedCallback(onDataReceived),
      onClientConnectedCallback(onClientConnected),
      onClientDisconnectedCallback(onClientDisconnected),
      onErrorCallback(onError)
{
    connect(this, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

    if (!this->listen(QHostAddress::AnyIPv4, port)) {
        qCriticalEx() << "无法启动服务器" << port;
        return;
    }

    qDebugEx() << "服务器已启动,监听端口:" << this->serverPort();
}

void TcpServer::onNewConnection() {
    auto socket = this->nextPendingConnection();

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

void TcpServer::onReadyRead()
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    auto data = socket->readAll();
    clientBuffers[socket].append(data);
    processBufferedData(socket);
}

void TcpServer::onDisconnected()
{
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

void TcpServer::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    auto socket = qobject_cast<QTcpSocket*>(sender());
    qCriticalEx() << "Socket error:" << socketError;
    if (onErrorCallback && socket) {
        onErrorCallback(socket, socketError);
    }
}

void TcpServer::processBufferedData(QTcpSocket* socket)
{
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
