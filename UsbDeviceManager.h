#pragma once

#include "UsbDeviceContext.h"
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QHostInfo>
#include <QJsonObject>
#include <libimobiledevice/libimobiledevice.h>
#include <QtConcurrent>
#include <QFutureWatcher>

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    void start();
    void stop();

    DeviceConnection* connectDevice(const QString& udid, uint16_t port, std::function<void(DeviceConnection*, const QByteArray&)> rawDataCallback = nullptr);
    void disconnectDevice(const QString& key);

    UsbDeviceContext* getContext(DeviceConnection* conn) const;

signals:
    void deviceConnected(DeviceConnection* conn);
    void deviceDisconnected(DeviceConnection* conn);
    void dataReceived(DeviceConnection* conn, const QJsonObject& json);
    void errorOccurred(DeviceConnection* conn, const QString& message);

public:
    explicit UsbDeviceManager(QObject* parent = nullptr);
    ~UsbDeviceManager() override = default;

    // 禁止拷贝和移动（保持单例安全）
    UsbDeviceManager(const UsbDeviceManager&) = delete;
    UsbDeviceManager& operator=(const UsbDeviceManager&) = delete;
    UsbDeviceManager(UsbDeviceManager&&) = delete;
    UsbDeviceManager& operator=(UsbDeviceManager&&) = delete;

private slots:
    void handlePollFinished();

private:
    void pollDevices();
    void processBufferedData(const QString& key, DeviceConnection* handler);

    QTimer* timer = nullptr;
    QHash<QString, UsbDeviceContext*> devices;
    QSet<QString> previousDevices;
    QHash<QString, QByteArray> deviceBuffers;

    QHash<DeviceConnection*, UsbDeviceContext*> connToContext;

    QFutureWatcher<QSet<QString>>* watcher = nullptr;
};

extern UsbDeviceManager* g_usbDeviceManager;
