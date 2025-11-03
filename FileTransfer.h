#pragma once

#include "Logger.h"
#include "EventHub.h"
#include "DeviceConnection.h"
#include "UsbDeviceManager.h"
#include <QTcpServer>
#include <QFile>
#include <QDataStream>
#include <QtConcurrent>
#include <QUuid>

// type 1收2发

class FileTransfer : public QObject
{
    Q_OBJECT
public:
    FileTransfer(DeviceConnection* connection, int type, const QString &path, quint64 size) : id(QUuid::createUuid().toString()), connection(connection), type(type), path(path), size(size)
    {
        qDebugEx() << "FileTransfer" << path << type;

        timer.start();

        if (connection->type != DeviceConnection::Usb)
        {
            tcpServer = new QTcpServer(this);

            connect(tcpServer, &QTcpServer::newConnection, this, &FileTransfer::onNewConnection);

            if (!tcpServer->listen(QHostAddress::Any, 0))
            {
                qWarningEx() << "Server failed to start";
            }
            else
            {
                qDebugEx() << "Server started, waiting for connections...";
            }
        }
        else
        {
            EventHub::StartListening("transferPort", [=](const QJsonValue &data, DeviceConnection* connection) {
                if (this->connection != connection)
                    return;

                if (data["id"] != id)
                    return;

                auto manager = UsbDeviceManager::instance();
                auto ctx = manager->getContext(connection);

                transferConnection = manager->connectDevice(ctx->udid, data["port"].toInt(), [=](DeviceConnection* conn, const QByteArray& data){
                    handleDataRead(data);
                });

                handleNewConnection();
            });
        }
    }

    ~FileTransfer() {
        transferConnection->close();

        if (tcpServer) {
            delete tcpServer;
            delete transferConnection;
        }
    }

    const QString id;

    quint16 serverPort() {
        return tcpServer ? tcpServer->serverPort() : 0;
    }

    double elapsedTime() const {
        return timer.elapsed() / 1000.0;
    }

signals:
    void progressUpdated(quint64 transferred, quint64 total);

protected:
    void onNewConnection()
    {
        auto socket = tcpServer->nextPendingConnection();
        transferConnection = new DeviceConnection(socket);

        tcpServer->close();
        qDebugEx() << "已接受第一个连接，服务器停止监听新连接。";

        connect(socket, &QTcpSocket::readyRead, this, &FileTransfer::onReadyRead);
        // connect(socket, &QTcpSocket::bytesWritten, this, &FileTransfer::onBytesWritten);

        handleNewConnection();
    }

    void onReadyRead()
    {
        auto socket = qobject_cast<QTcpSocket *>(sender());
        auto data = socket->readAll();
        handleDataRead(data);
    }

    void handleNewConnection()
    {
        if (type == 1)
        {
            QString dirPath = QFileInfo(path).absolutePath();

            static QDir dir;
            if (!dir.exists(dirPath))
                dir.mkpath(dirPath);

            recvFile.setFileName(path);

            if (!recvFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                qCritical() << "文件保存失败:" << recvFile.errorString();
                deleteLater();
            }

            return;
        }

        QtConcurrent::run([=]() {
            QFile sendFile(path);

            if (!sendFile.open(QIODevice::ReadOnly))
            {
                qCriticalEx() << "Failed to open file for sending.";
                return;
            }

            while (!sendFile.atEnd())
            {
                auto buffer = sendFile.read(4096);
                if (connection->type == DeviceConnection::Usb) {
                    transferConnection->write(buffer);
                }
                else {
                    QMetaObject::invokeMethod(this, [=]() {
                        transferConnection->write(buffer);
                    });
                }
            }

            sendFile.close();
            qInfoEx() << "文件关闭";
        });
    }

    void handleDataRead(const QByteArray& data)
    {
        buffer.append(data);

        if (type == 1) {
            if (size == 0) {
                if (buffer.size() < 8) return;
                size = *reinterpret_cast<quint64 *>(buffer.data());
                buffer.remove(0, 8);

                qDebugEx() << "文件大小" << size;

                if (buffer.size() == 0)
                    return;
            }

            recvFile.write(buffer);
            transferredBytes += buffer.size();
            buffer.clear();
            emit progressUpdated(transferredBytes, size);

            if (recvFile.size() == size) {
                recvFile.close();
                deleteLater();
                qDebugEx() << path << "接收完成断开连接" << size;
            }
        } else {
            while (buffer.size() >= 8) {
                auto bytesSent = *reinterpret_cast<quint64 *>(buffer.data());
                buffer.remove(0, 8);

                transferredBytes = bytesSent;
                emit progressUpdated(transferredBytes, size);

                if (bytesSent == size) {
                    deleteLater();
                    qDebugEx() << path << "发送完成断开连接" << size;
                }
            }
        }
    }

    DeviceConnection* const connection;
    const int type;
    const QString path;
    quint64 size;

    QByteArray buffer;
    QFile recvFile;

    QTcpServer *tcpServer = nullptr;
    DeviceConnection* transferConnection = nullptr;

    QElapsedTimer timer;
    quint64 transferredBytes = 0;
};
