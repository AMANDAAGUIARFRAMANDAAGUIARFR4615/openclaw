#include "UsbDeviceManager.h"
#include <QJsonDocument>
#include <QDebug>

Q_GLOBAL_STATIC(UsbDeviceManager, s_usbDeviceManager)

UsbDeviceManager* UsbDeviceManager::instance() {
    return s_usbDeviceManager;
}

UsbDeviceManager::UsbDeviceManager(QObject* parent)
    : QObject(parent)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &UsbDeviceManager::pollDevices);

    watcher = new QFutureWatcher<QSet<QString>>(this);
    connect(watcher, &QFutureWatcher<QSet<QString>>::finished, this, &UsbDeviceManager::handlePollFinished);
}

void UsbDeviceManager::start() {
    qDebug() << "🚀 启动设备管理器...";
    pollDevices();
    timer->start(2000);
}

void UsbDeviceManager::stop() {
    qDebug() << "🛑 停止设备管理器...";
    for (const QString& key : devices.keys())
        disconnectDevice(key);
}

DeviceConnection* UsbDeviceManager::connectDevice(const QString& udid, uint16_t port, std::function<void(DeviceConnection*, const QByteArray&)> rawDataCallback) {
    QString key = udid + ":" + QString::number(port);

    UsbDeviceContext* ctx = new UsbDeviceContext();
    ctx->udid = udid;
    ctx->port = port;

    if (IDEVICE_E_SUCCESS != idevice_new(&ctx->device, udid.toUtf8().constData())) {
        emitError(nullptr, QString("无法创建 idevice: %1").arg(udid));
        delete ctx;
        return nullptr;
    }

    if (idevice_connect(ctx->device, port, &ctx->connection) != IDEVICE_E_SUCCESS) {
        emitError(nullptr, QString("无法连接端口 %1 的设备: %2").arg(port).arg(udid));
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
        ctx->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(ctx->notifier, &QSocketNotifier::activated, this, [=](int) {
            char buffer[1024 * 1024];
            uint32_t bytes = 0;
            idevice_error_t err = idevice_connection_receive(ctx->connection, buffer, sizeof(buffer), &bytes);
            if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                QByteArray data(buffer, bytes);

                if (rawDataCallback)
                {
                    rawDataCallback(ctx->handler, data);
                    return;
                }

                deviceBuffers[key].append(data);
                processBufferedData(key, ctx->handler);
            } else if (err != IDEVICE_E_SUCCESS) {
                // emitError(ctx->handler, QString("设备通信错误: %1").arg(err));
            }
        });
    }

    devices.insert(key, ctx);
    connToContext.insert(ctx->handler, ctx);
    qDebug() << "✅ 已连接设备:" << key;
    emit deviceConnected(ctx->handler);
    return ctx->handler;
}

void UsbDeviceManager::disconnectDevice(const QString& key) {
    if (!devices.contains(key)) return;

    UsbDeviceContext* ctx = devices[key];
    qDebug() << "❌ 断开设备:" << key;

    if (ctx->notifier) ctx->notifier->deleteLater();
    if (ctx->handler) {
        emit deviceDisconnected(ctx->handler);
        connToContext.remove(ctx->handler);
        delete ctx->handler;
    }
    if (ctx->connection) idevice_disconnect(ctx->connection);
    if (ctx->device) idevice_free(ctx->device);

    delete ctx;
    devices.remove(key);
    deviceBuffers.remove(key);
}

void UsbDeviceManager::pollDevices() {
    if (watcher->isRunning()) {
        return; // 避免多次并发轮询
    }

    auto future = QtConcurrent::run([=]() -> QSet<QString> {
        char** deviceList = nullptr;
        int count = 0;

        if (idevice_get_device_list(&deviceList, &count) != IDEVICE_E_SUCCESS) {
            qWarning() << "⚠️ 获取设备列表失败";
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
            qDebug() << "📱 检测到新设备:" << udid;
            connectDevice(udid, 32839);
        }
    }

    // 检测设备拔出
    for (const QString& udid : previousDevices) {
        if (!currentDevices.contains(udid)) {
            qDebug() << "❌ 检测到设备拔出:" << udid;
            auto keys = devices.keys();
            for (const QString& key : keys) {
                if (key.startsWith(udid + ":"))
                    disconnectDevice(key);
            }
        }
    }

    previousDevices = currentDevices;
}

void UsbDeviceManager::emitError(DeviceConnection* conn, const QString& msg) {
    qWarning() << "⚠️ UsbDeviceManager 错误:" << msg;
    emit errorOccurred(conn, msg);
}

void UsbDeviceManager::processBufferedData(const QString& key, DeviceConnection* handler) {
    auto &buffer = deviceBuffers[key];

    while (buffer.size() >= static_cast<int>(sizeof(quint64) + sizeof(quint32))) {
        auto identifier = *reinterpret_cast<const quint64*>(buffer.constData());
        if (identifier != 0xb7c2e0f542a39a3e) {
            qCritical() << "识别码不匹配，清空缓冲区";
            buffer.clear();
            return;
        }

        auto jsonDataLength = *reinterpret_cast<const quint32*>(buffer.constData() + sizeof(quint64));
        if (buffer.size() < static_cast<int>(sizeof(quint64) + sizeof(quint32) + jsonDataLength))
            return;

        QByteArray jsonData = buffer.mid(sizeof(quint64) + sizeof(quint32), jsonDataLength);
        buffer.remove(0, sizeof(quint64) + sizeof(quint32) + jsonDataLength);
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);

        if (!doc.isNull()) {
            emit dataReceived(handler, doc.object());
        } else {
            qCritical() << "JSON 解析失败，丢弃数据";
        }
    }
}

UsbDeviceContext* UsbDeviceManager::getContext(DeviceConnection* conn) const {
    return connToContext.value(conn, nullptr);
}