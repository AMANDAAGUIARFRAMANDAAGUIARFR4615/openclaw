#include "UsbDeviceManager.h"
#include "MainWindow.h"
#include <QJsonDocument>
#include <magic_enum/magic_enum.hpp>

UsbDeviceManager::UsbDeviceManager(QObject* parent)
    : QObject(parent)
{
    timer = new QTimer(this);
    timer->callOnTimeout(this, &UsbDeviceManager::pollDevices);

    watcher = new QFutureWatcher<QSet<QString>>(this);
    connect(watcher, &QFutureWatcher<QSet<QString>>::finished, this, &UsbDeviceManager::handlePollFinished);
}

void UsbDeviceManager::start() {
    qDebugEx() << "🚀 启动设备管理器...";
    pollDevices();
    timer->start(2000);
}

void UsbDeviceManager::stop() {
    qDebugEx() << "🛑 停止设备管理器...";

    for (const auto& it : connToContext) {
        disconnectDevice(it->handler);
    }
}

DeviceConnection* UsbDeviceManager::connectDevice(const QString& udid, uint16_t port, std::function<void(DeviceConnection*, const QByteArray&)> rawDataCallback) {
    UsbDeviceContext* ctx = new UsbDeviceContext();
    ctx->udid = udid;
    ctx->port = port;

    if (IDEVICE_E_SUCCESS != idevice_new(&ctx->device, udid.toUtf8().constData())) {
        emit errorOccurred(nullptr, QString("无法创建 idevice: %1").arg(udid));
        delete ctx;
        return nullptr;
    }

    if (idevice_connect(ctx->device, port, &ctx->connection) != IDEVICE_E_SUCCESS) {
        emit errorOccurred(nullptr, QString("无法连接端口 %1 的设备: %2").arg(port).arg(udid));
        idevice_free(ctx->device);
        delete ctx;
        return nullptr;
    }

    ctx->handler = new DeviceConnection(ctx->connection);
    
    if (!rawDataCallback)
    {
        ctx->handler->send("deviceInfo", QJsonObject{
            {"remoteDeviceName", QHostInfo::localHostName()}
        });
    }

    int fd = -1;
    if (idevice_connection_get_fd(ctx->connection, &fd) == IDEVICE_E_SUCCESS && fd >= 0) {
        ctx->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, ctx->handler);
        connect(ctx->notifier, &QSocketNotifier::activated, [=](int) {   
            char buffer[1024 * 1024];
            quint32 bytes = 0;
            idevice_error_t err = idevice_connection_receive(ctx->connection, buffer, sizeof(buffer), &bytes);
            if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                QByteArray data(buffer, bytes);
                // qDebugEx() << "接收到数据字节数" << bytes;

                if (rawDataCallback)
                {
                    rawDataCallback(ctx->handler, data);
                    return;
                }

                deviceBuffers[ctx].append(data);
                processBufferedData(ctx);
            } else if (err != IDEVICE_E_SUCCESS) {
                emit errorOccurred(ctx->handler, QString("%1端口通信错误: %2").arg(port).arg(magic_enum::enum_name(err)));
                disconnectDevice(ctx->handler);
            }
        });
    }

    connToContext.insert(ctx->handler, ctx);
    qDebugEx() << "✅ 连接设备:" << ctx << ctx->udid + ":" + QString::number(ctx->port);
    emit deviceConnected(ctx->handler);
    return ctx->handler;
}

void UsbDeviceManager::disconnectDevice(DeviceConnection* conn) {
    if (!conn) return;

    UsbDeviceContext* ctx = connToContext.value(conn, nullptr);
    if (!ctx) return;

    if (ctx->handler != conn) {
        qCriticalEx() << "disconnectDevice" << conn;
        return;
    }

    connToContext.remove(conn);

    if (ctx->port == 32839)
        emit deviceDisconnected(conn);

    qDebugEx() << "❌ 断开设备:" << ctx << ctx->udid + ":" + QString::number(ctx->port);

    ctx->handler->deleteLater();

    if (ctx->connection) idevice_disconnect(ctx->connection);
    if (ctx->device) idevice_free(ctx->device);

    deviceBuffers.remove(ctx);
    delete ctx;
}

void UsbDeviceManager::pollDevices() {
    if (MainWindow::getInstance()->getTab().getAutoConnectUSBDevices() == 0)
        return;

    if (watcher->isRunning())
        return; // 避免多次并发轮询

    auto future = QtConcurrent::run([=]() -> QSet<QString> {
        char** deviceList = nullptr;
        int count = 0;

        if (idevice_get_device_list(&deviceList, &count) != IDEVICE_E_SUCCESS) {
            qCriticalEx() << "⚠️ 获取设备列表失败";
            return {};
        }

        QSet<QString> currentDevices;
        for (int i = 0; i < count; i++) {
            QString udid = QString::fromUtf8(deviceList[i]);
            currentDevices.insert(udid);
        }

        idevice_device_list_free(deviceList);
        return currentDevices;
    });

    watcher->setFuture(future);
}

void UsbDeviceManager::handlePollFinished() {
    QSet<QString> currentDevices = watcher->result();

    for (const QString& udid : currentDevices) {
        if (!previousDevices.contains(udid)) {
            qDebugEx() << "📱 检测到新设备:" << udid;
            connectDevice(udid, 32839);
        }
    }

    // 检测设备拔出
    for (const QString& udid : previousDevices) {
        if (!currentDevices.contains(udid)) {
            qDebugEx() << "❌ 检测到设备拔出:" << udid;
            for (const auto& it : connToContext) {
                if (it->udid == udid)
                    disconnectDevice(it->handler);
            }
        }
    }

    previousDevices = currentDevices;
}

void UsbDeviceManager::processBufferedData(UsbDeviceContext* ctx) {
    auto &buffer = deviceBuffers[ctx];

    while (buffer.size() >= static_cast<int>(sizeof(quint64) + sizeof(quint32))) {
        auto identifier = *reinterpret_cast<const quint64*>(buffer.constData());
        if (identifier != 0xb7c2e0f542a39a3e) {
            qCriticalEx() << "识别码不匹配，清空缓冲区";
            buffer.clear();
            return;
        }

        auto size = *reinterpret_cast<const quint32*>(buffer.constData() + sizeof(quint64));
        if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + size))
            return;

        const auto& data = buffer.mid(sizeof(quint64) + sizeof(quint32), size);
        buffer.remove(0, sizeof(quint64) + sizeof(quint32) + size);

        const auto& jsonData = AesCrypto::decrypt(data);
        if (jsonData.size() == 0) {
            // qCriticalEx() << "解密失败";
            return;
        }

        const auto& doc = QJsonDocument::fromJson(jsonData);

        if (!doc.isNull()) {
            emit dataReceived(ctx->handler, doc.object());
        } else {
            qCriticalEx() << "JSON 解析失败，丢弃数据";
        }
    }
}

UsbDeviceContext* UsbDeviceManager::getContext(DeviceConnection* conn) const {
    return connToContext.value(conn, nullptr);
}
