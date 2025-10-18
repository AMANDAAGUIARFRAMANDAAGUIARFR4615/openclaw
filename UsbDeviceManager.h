#pragma once

#include "UsbDeviceContext.h"
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QHostInfo>
#include <QJsonObject>
#include <functional>
#include <libimobiledevice/libimobiledevice.h>

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    explicit UsbDeviceManager(
        const std::function<void(DeviceConnection*)> &onDeviceConnected = nullptr,
        const std::function<void(DeviceConnection*)> &onDeviceDisconnected = nullptr,
        const std::function<void(DeviceConnection*, const QJsonObject&)> &onDataReceived = nullptr,
        const std::function<void(DeviceConnection*, const QString&)> &onError = nullptr,
        QObject* parent = nullptr);

    void start();
    void stop();

    UsbDeviceContext* connectDevice(const QString& udid, uint16_t port);
    void disconnectDevice(const QString& key);

private:
    void pollDevices();
    void emitError(DeviceConnection* conn, const QString& msg);
    void processBufferedData(const QString& key, DeviceConnection* handler);

    QTimer* timer;
    QHash<QString, UsbDeviceContext*> devices;
    QSet<QString> previousDevices;
    QHash<QString, QByteArray> deviceBuffers;

    std::function<void(DeviceConnection*)> onDeviceConnectedCallback;
    std::function<void(DeviceConnection*)> onDeviceDisconnectedCallback;
    std::function<void(DeviceConnection*, const QJsonObject&)> onDataReceivedCallback;
    std::function<void(DeviceConnection*, const QString&)> onErrorCallback;
};
