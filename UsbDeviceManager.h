#pragma once

#include "UsbDeviceContext.h"
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QHostInfo>
#include <QJsonObject>
#include <QtConcurrent>
#include <QFutureWatcher>

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include <libimobiledevice/libimobiledevice.h>
#endif

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    static UsbDeviceManager* getInstance() { static UsbDeviceManager instance; return &instance; }

    void start();
    void stop();

    DeviceConnection* connectDevice(const QString& udid, uint16_t port, bool rawMode);
    void disconnectDevice(DeviceConnection* conn);

signals:
    void deviceConnected(DeviceConnection* conn);
    void deviceDisconnected(DeviceConnection* conn);
    void dataReceived(DeviceConnection* conn, const QJsonObject& json);
    void errorOccurred(DeviceConnection* conn, const QString& message);
    void rawDataReceived(DeviceConnection* conn, const QByteArray& data);

public:
    explicit UsbDeviceManager(QObject* parent = nullptr);
    ~UsbDeviceManager() override = default;

private slots:
    void handlePollFinished();

private:
    void connectPendingDevices();
    void pollDevices();
    void processBufferedData(UsbDeviceContext* usbDeviceContext);

    QSet<QString> previousDevices;
    QHash<UsbDeviceContext*, QByteArray> deviceBuffers;

    QHash<DeviceConnection*, UsbDeviceContext*> connToContext;
    QHash<QString, bool> devices;

    QFutureWatcher<QSet<QString>>* watcher;
    QTimer* pollTimer;
    QTimer* connectTimer;
};
