#pragma once

#include "UsbDeviceContext.h"
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QHostInfo>
#include <QJsonObject>
#include <libimobiledevice/libimobiledevice.h>

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    // 单例获取方法
    static UsbDeviceManager* instance();

    // 启动/停止管理器
    void start();
    void stop();

    // 设备连接管理
    UsbDeviceContext* connectDevice(const QString& udid, uint16_t port);
    void disconnectDevice(const QString& key);

signals:
    void deviceConnected(DeviceConnection* conn);
    void deviceDisconnected(DeviceConnection* conn);
    void dataReceived(DeviceConnection* conn, const QJsonObject& json);
    void errorOccurred(DeviceConnection* conn, const QString& message);

public:
    // 公开构造给 Q_GLOBAL_STATIC 使用
    explicit UsbDeviceManager(QObject* parent = nullptr);
    ~UsbDeviceManager() override = default;

    // 禁止拷贝和移动（保持单例安全）
    UsbDeviceManager(const UsbDeviceManager&) = delete;
    UsbDeviceManager& operator=(const UsbDeviceManager&) = delete;
    UsbDeviceManager(UsbDeviceManager&&) = delete;
    UsbDeviceManager& operator=(UsbDeviceManager&&) = delete;

private:
    void pollDevices();
    void emitError(DeviceConnection* conn, const QString& msg);
    void processBufferedData(const QString& key, DeviceConnection* handler);

    QTimer* timer = nullptr;
    QHash<QString, UsbDeviceContext*> devices;
    QSet<QString> previousDevices;
    QHash<QString, QByteArray> deviceBuffers;
};
