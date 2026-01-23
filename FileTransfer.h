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
    FileTransfer(DeviceConnection* connection, int type, const QString &path, quint64 size, QObject* parent) : id(QUuid::createUuid().toString(QUuid::WithoutBraces)), connection(connection), type(type), path(path), size(size), QObject(parent)
    {
        qDebugEx() << "FileTransfer" << path << type;

        timer.start();

        if (connection->type != DeviceConnection::Usb)
        {
            tcpServer = new QTcpServer(this);

            connect(tcpServer, &QTcpServer::newConnection, this, &FileTransfer::onNewConnection);

            if (!tcpServer->listen(QHostAddress::Any, 0))
            {
                qCriticalEx() << "Server failed to start";
            }
            else
            {
                qDebugEx() << "Server started, waiting for connections...";
            }
        }
        else
        {
            EventHub::on(this, "transferPort", [=](const QJsonValue &data, DeviceConnection* connection) {
                if (this->connection != connection)
                    return;

                if (data["id"] != id)
                    return;

                transferConnection = UsbDeviceManager::getInstance()->connectDevice(connection->deviceInfo->deviceId, data["port"].toInt(), true);

                if (!transferConnection) {
                    qCriticalEx() << "连接设备失败" << connection->deviceInfo->deviceId << data["port"];
                    return;
                }

                connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::rawDataReceived, this, [=](DeviceConnection* sender, const QByteArray& data){
                    if (sender != transferConnection)
                        return;

                    handleDataRead(data);
                });

                handleNewConnection();
            });
        }
    }

    ~FileTransfer() {
        EventHub::off(this, "transferPort");

        if (tcpServer)
            tcpServer->deleteLater();
        
        if (connection->type == DeviceConnection::Usb)
            UsbDeviceManager::getInstance()->disconnectDevice(transferConnection);

        connection = nullptr;
    }

    const QString id;

    quint16 serverPort() {
        return tcpServer ? tcpServer->serverPort() : 0;
    }

    float elapsedTime() const {
        return timer.elapsed() / 1000.0f;
    }

signals:
    void progressUpdated(quint64 transferred, quint64 total);

protected:
    void onNewConnection()
    {
        auto socket = tcpServer->nextPendingConnection();
        transferConnection = new DeviceConnection(socket);

        tcpServer->close();
        qDebugEx() << "已接受第一个连接，停止监听新连接。";

        connect(socket, &QTcpSocket::readyRead, this, &FileTransfer::onReadyRead);
        // connect(socket, &QTcpSocket::bytesWritten, this, &FileTransfer::onBytesWritten);

        handleNewConnection();
    }

    void onReadyRead()
    {
        auto socket = qobject_cast<QTcpSocket *>(sender());
        if (!socket)
            return;
            
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
                qCriticalEx() << "文件保存失败:" << recvFile.errorString();
                deleteLater();
            }

            return;
        }

        QtConcurrent::run([this]() {
            QFile sendFile(path);

            if (!sendFile.open(QIODevice::ReadOnly))
            {
                qCriticalEx() << "Failed to open file for sending.";
                return;
            }

            const qint64 chunkSize = 512 * 1024; 

            while (!sendFile.atEnd())
            {
                const auto& buffer = sendFile.read(chunkSize);

                // 使用 Qt::BlockingQueuedConnection，这会阻塞当前子线程，直到主线程完成 write 调用。
                // 这起到了两个作用：
                // 1. 防止子线程读取速度过快，导致主线程事件队列堆积成千上万个事件（卡死UI的主因）。
                // 2. 自动实现了“背压”，即读取速度自动适配网络/主线程处理速度。
                QMetaObject::invokeMethod(this, [=]() {
                    transferConnection->write(buffer);
                }, Qt::BlockingQueuedConnection);
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

            // 只有当 buffer 积累到 512KB 以上，或者文件接收完毕时，才执行写盘操作。
            if (buffer.size() < 512 * 1024 && transferredBytes + buffer.size() < size)
                return;

            recvFile.write(buffer);
            transferredBytes += buffer.size();
            buffer.clear();
            
            // 完成时或每隔100ms才发送一次信号
            qint64 currentTime = timer.elapsed();
            if (transferredBytes == size || currentTime - lastNotifyTime > 500) {
                emit progressUpdated(transferredBytes, size);
                lastNotifyTime = currentTime;
            }

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

                // 完成时或每隔100ms才发送一次信号
                qint64 currentTime = timer.elapsed();
                if (transferredBytes == size || currentTime - lastNotifyTime > 500) {
                    emit progressUpdated(transferredBytes, size);
                    lastNotifyTime = currentTime;
                }

                if (bytesSent == size) {
                    deleteLater();
                    qDebugEx() << path << "发送完成断开连接" << size;
                }
            }
        }
    }

    DeviceConnection* connection;
    const int type;
    const QString path;
    quint64 size;

    QByteArray buffer;
    QFile recvFile;

    QTcpServer *tcpServer = nullptr;
    DeviceConnection* transferConnection = nullptr;

    QElapsedTimer timer;
    quint64 transferredBytes = 0;
    
    qint64 lastNotifyTime = 0;
};
