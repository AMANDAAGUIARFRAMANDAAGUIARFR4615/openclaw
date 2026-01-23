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
        if (type == 1) {
            if (pathLocked.contains(path)) {
                qCriticalEx() << "拒绝传输，文件正在处理中" << path;
                
                QTimer::singleShot(0, this, [this]() {
                    emit progressUpdated(-1, 0);
                    deleteLater();
                });
                return;
            }

            pathLocked.insert(path);
        }

        pendingList.append(this);
        scheduleNext();
    }

    ~FileTransfer() {
        if (runningList.contains(this)) {
            runningList.removeOne(this);
            scheduleNext(); // 有空位了，调度下一个
        } else {
            pendingList.removeOne(this); // 还在排队就销毁了
        }

        EventHub::off(this, "transferPort");
        EventHub::off(this, "transferStatus");

        if (tcpServer)
            tcpServer->deleteLater();
        
        if (connection->type == DeviceConnection::Usb)
            UsbDeviceManager::getInstance()->disconnectDevice(transferConnection);

        if (type == 1)
            pathLocked.remove(path);
    }

    const QString id;

    quint16 serverPort() {
        return tcpServer ? tcpServer->serverPort() : 0;
    }

    float elapsedTime() const {
        // 如果还在排队（timer没开始），返回0
        if (!timer.isValid()) return 0.0f;
        return timer.elapsed() / 1000.0f;
    }

signals:
    void progressUpdated(qint64 transferred, qint64 total);

protected:
    static void scheduleNext() {
        // 允许同时运行5个
        while (runningList.size() < 5 && !pendingList.isEmpty()) {
            FileTransfer* task = pendingList.takeFirst();
            runningList.append(task);
            task->startTransfer();
        }
    }

    void startTransfer() {
        qDebugEx() << "FileTransfer Start" << path << type;

        timer.start();

        if (connection->type != DeviceConnection::Usb)
        {
            tcpServer = new QTcpServer(this);

            connect(tcpServer, &QTcpServer::newConnection, this, &FileTransfer::onNewConnection);

            tcpServer->listen(QHostAddress::Any, 0);
            qDebugEx() << "文件传输服务已启动，监听端口" << tcpServer->serverPort();
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

        EventHub::on(this, "transferStatus", [this](QJsonValue data, DeviceConnection* connection) {
            if (this->connection != connection)
                return;

            if (data["code"].toInt() != 0 && data["id"].toString() == id) {
                emit progressUpdated(-1, 0);
                qDebugEx() << "传输失败断开连接" << data;
                deleteLater();
            }
        });
    }

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
                // 确保在主线程析构
                QMetaObject::invokeMethod(this, [this](){ deleteLater(); });
                return;
            }

            const qint64 chunkSize = 512 * 1024; 

            while (!sendFile.atEnd())
            {
                // 如果任务已被移出运行队列（被外部删除了），停止发送
                if (!runningList.contains(this)) break;

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

    const DeviceConnection* connection;
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

    inline static QSet<QString> pathLocked;
    
    inline static QList<FileTransfer*> pendingList;
    inline static QList<FileTransfer*> runningList;
};
