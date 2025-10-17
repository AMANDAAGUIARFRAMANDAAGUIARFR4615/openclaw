#pragma once

#include "DeviceConnection.h"
#include <QSocketNotifier>
#include <QHash>
#include <QTimer>
#include <QHostInfo>
#include <QJsonObject>
#include <functional>
#include <libimobiledevice/libimobiledevice.h>

struct DeviceContext {
    idevice_t device = nullptr;
    idevice_connection_t connection = nullptr;
    QSocketNotifier* notifier = nullptr;
    DeviceConnection* handler = nullptr;
};

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    explicit UsbDeviceManager(
        const std::function<void(DeviceConnection*)> &onDeviceConnected = nullptr,
        const std::function<void(DeviceConnection*)> &onDeviceDisconnected = nullptr,
        const std::function<void(DeviceConnection*, const QJsonObject&)> &onDataReceived = nullptr,
        const std::function<void(DeviceConnection*, const QString&)> &onError = nullptr,
        QObject* parent = nullptr)
        : QObject(parent),
          onDeviceConnectedCallback(onDeviceConnected),
          onDeviceDisconnectedCallback(onDeviceDisconnected),
          onDataReceivedCallback(onDataReceived),
          onErrorCallback(onError)
    {
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &UsbDeviceManager::pollDevices);
    }

    void start() {
        qDebugEx() << "🚀 启动设备管理器...";
        pollDevices();
        timer->start(2000);
    }

    void stop() {
        qDebugEx() << "🛑 停止设备管理器，清理所有设备...";
        for (const QString& udid : devices.keys()) {
            disconnectDevice(udid);
        }
        devices.clear();
    }

private:
    QTimer* timer;
    QHash<QString, DeviceContext*> devices;
    QSet<QString> previousDevices;
    QHash<QString, QByteArray> deviceBuffers;

    std::function<void(DeviceConnection*)> onDeviceConnectedCallback;
    std::function<void(DeviceConnection*)> onDeviceDisconnectedCallback;
    std::function<void(DeviceConnection*, const QJsonObject&)> onDataReceivedCallback;
    std::function<void(DeviceConnection*, const QString&)> onErrorCallback;

    void pollDevices() {
        char** deviceList = nullptr;
        int count = 0;

        if (idevice_get_device_list(&deviceList, &count) != IDEVICE_E_SUCCESS) {
            qWarning() << "⚠️ 获取设备列表失败";
            return;
        }

        QSet<QString> currentDevices;
        for (int i = 0; i < count; i++) {
            QString udid = QString::fromUtf8(deviceList[i]);
            currentDevices.insert(udid);
            if (!previousDevices.contains(udid)) {
                qDebugEx() << "📱 检测到新设备:" << udid;
                DeviceConnection* conn = connectDevice(udid);
                if (conn && onDeviceConnectedCallback)
                    onDeviceConnectedCallback(conn);
            }
        }

        // 检测断开设备
        for (const QString& udid : previousDevices) {
            if (!currentDevices.contains(udid)) {
                qDebugEx() << "❌ 检测到设备拔出:" << udid;
                DeviceConnection* conn = getConnection(udid);
                disconnectDevice(udid);
                if (conn && onDeviceDisconnectedCallback)
                    onDeviceDisconnectedCallback(conn);
            }
        }

        previousDevices = currentDevices;
        idevice_device_list_free(deviceList);
    }

    DeviceConnection* getConnection(const QString& udid) {
        if (!devices.contains(udid)) return nullptr;
        return devices.value(udid)->handler;
    }

    DeviceConnection* connectDevice(const QString& qUdid) {
        if (devices.contains(qUdid)) {
            qDebugEx() << "⚠️ 设备已连接:" << qUdid;
            return devices.value(qUdid)->handler;
        }

        DeviceContext* ctx = new DeviceContext();

        if (IDEVICE_E_SUCCESS != idevice_new(&ctx->device, qUdid.toUtf8().constData())) {
            emitError(nullptr, QString("无法创建 idevice: %1").arg(qUdid));
            delete ctx;
            return nullptr;
        }

        uint16_t port = 32839;
        if (idevice_connect(ctx->device, port, &ctx->connection) != IDEVICE_E_SUCCESS) {
            emitError(nullptr, QString("无法连接端口 %1 设备: %2").arg(port).arg(qUdid));
            idevice_free(ctx->device);
            delete ctx;
            return nullptr;
        }

        ctx->handler = new DeviceConnection(ctx->connection);
        ctx->handler->send("deviceInfo", QJsonObject{{"remoteDeviceName", QHostInfo::localHostName()}});

        int fd = -1;
        if (idevice_connection_get_fd(ctx->connection, &fd) == IDEVICE_E_SUCCESS && fd >= 0) {
            ctx->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(ctx->notifier, &QSocketNotifier::activated, this, [=](int) {
                char buffer[512];
                uint32_t bytes = 0;
                idevice_error_t err = idevice_connection_receive(ctx->connection, buffer, sizeof(buffer), &bytes);
                if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                    QByteArray data(buffer, bytes);
                    deviceBuffers[qUdid].append(data);
                    processBufferedData(qUdid, ctx->handler);
                } else if (err != IDEVICE_E_SUCCESS) {
                    emitError(ctx->handler, QString("设备通信错误: %1").arg(err));
                }
            });
        }

        devices.insert(qUdid, ctx);
        previousDevices.insert(qUdid);

        return ctx->handler;
    }

    void disconnectDevice(const QString& udid) {
        if (!devices.contains(udid))
            return;

        DeviceContext* ctx = devices.value(udid);
        qDebugEx() << "❌ 断开设备:" << udid;

        if (ctx->notifier) ctx->notifier->deleteLater();
        if (ctx->handler) delete ctx->handler;
        if (ctx->connection) idevice_disconnect(ctx->connection);
        if (ctx->device) idevice_free(ctx->device);

        delete ctx;
        devices.remove(udid);
        previousDevices.remove(udid);
    }

    void emitError(DeviceConnection* conn, const QString& msg) {
        qWarning() << "⚠️ UsbDeviceManager 错误:" << msg;
        if (onErrorCallback)
            onErrorCallback(conn, msg);
    }

    void processBufferedData(const QString& udid, DeviceConnection* handler) {
        auto &buffer = deviceBuffers[udid];

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
                    onDataReceivedCallback(handler, doc.object());
                }
            } else {
                qCriticalEx() << "JSON 解析失败，丢弃数据";
            }
        }
    }
};
