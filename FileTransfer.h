#pragma once

#include "Logger.h"
#include <QTcpSocket>
#include <QTcpServer>
#include <QFile>
#include <QDataStream>
#include <QtConcurrent>

class FileTransfer : public QTcpServer
{
    Q_OBJECT
public:
    FileTransfer(int type, const QString &path, quint64 size) : type(type), path(path), size(size)
    {
        connect(this, &QTcpServer::newConnection, this, &FileTransfer::onNewConnection);

        if (!listen(QHostAddress::Any, 0))
        {
            qWarning() << "Server failed to start";
        }
        else
        {
            qDebugEx() << "Server started, waiting for connections...";
        }
    }

protected:
    void onNewConnection()
    {
        auto socket = nextPendingConnection();

        close();
        qDebugEx() << "已接受第一个连接，服务器停止监听新连接。";

        connect(socket, &QTcpSocket::readyRead, this, &FileTransfer::onReadyRead);
        // connect(socket, &QTcpSocket::bytesWritten, this, &FileTransfer::onBytesWritten);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        connect(socket, &QTcpSocket::disconnected, this, &FileTransfer::deleteLater);

        qDebugEx() << path << type;

        if (type == 1)
        {
            recvFile.setFileName(path);

            if (!recvFile.open(QIODevice::WriteOnly))
            {
                qCritical() << "文件保存失败:" << recvFile.errorString();
                socket->close();
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
                QMetaObject::invokeMethod(this, [=, &buffer]() {
                    socket->write(buffer);
                });
            }

            sendFile.close();
            qInfoEx() << "文件关闭";
        });
    }

    void onReadyRead()
    {
        auto socket = qobject_cast<QTcpSocket *>(sender());
        auto data = socket->readAll();
        buffer.append(data);
        
        if (type == 1) {
            if (size == 0) {
                if (buffer.size() < 8)
                    return;

                size = *reinterpret_cast<quint64 *>(buffer.data());
                buffer.remove(0, 8);
                qDebugEx() << "文件大小" << size;

                if (buffer.size() == 0)
                    return;
            }

            recvFile.write(buffer);
            buffer.clear();

            if (recvFile.size() == size) {
                recvFile.close();
                socket->close();
                qDebugEx() << path << "接收完成断开连接";
            }
        }
        else {
            while (buffer.size() >= 8)
            {
                auto bytesSent = *reinterpret_cast<quint64 *>(buffer.data());
                buffer.remove(0, 8);

                if (bytesSent == size) {
                    socket->close();
                    qDebugEx() << path << "发送完成断开连接";
                }
            }
        }
    }

private:
    int type;
    QString path;
    quint64 size;
    QByteArray buffer;
    QFile recvFile;
};
