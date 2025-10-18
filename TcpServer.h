#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <functional>

class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    TcpServer(const std::function<void(QTcpSocket*)> &onClientConnected,
              const std::function<void(QTcpSocket*, const QJsonObject&)> &onDataReceived,
              const std::function<void(QTcpSocket*)> &onClientDisconnected,
              const std::function<void(QTcpSocket*, QAbstractSocket::SocketError)> &onError,
              quint16 port = 0);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    QMap<QTcpSocket*, QByteArray> clientBuffers; // 保存每个客户端的缓冲区
    std::function<void(QTcpSocket*)> onClientConnectedCallback;
    std::function<void(QTcpSocket*, const QJsonObject&)> onDataReceivedCallback;
    std::function<void(QTcpSocket*)> onClientDisconnectedCallback;
    std::function<void(QTcpSocket*, QAbstractSocket::SocketError)> onErrorCallback;

    void processBufferedData(QTcpSocket* socket);
};
