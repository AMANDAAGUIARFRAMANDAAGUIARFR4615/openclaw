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
#include <QQueue>
#include <QPointer>
#include <QThreadPool>

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include <libimobiledevice/libimobiledevice.h>
#endif

class UsbDeviceManager : public QObject {
    Q_OBJECT

public:
    static UsbDeviceManager* getInstance() { static UsbDeviceManager* instance = new UsbDeviceManager; return instance; }

    void start();
    void stop();

    DeviceConnection* connectDevice(const QString& udid, uint16_t port, bool rawMode);
    void disconnectDevice(DeviceConnection* conn);

signals:
    void deviceConnected(DeviceConnection* conn);
    void deviceDisconnected(DeviceConnection* conn);
    void dataReceived(DeviceConnection* conn, const QJsonObject& json);
    void errorOccurred(DeviceConnection* conn, const QString& message);

public:
    explicit UsbDeviceManager(QObject* parent = nullptr);
    ~UsbDeviceManager() override = default;

private slots:
    void handlePollFinished();

private:
    void connectPendingDevices();
    void pollDevices();
    void processBufferedData(UsbDeviceContext* usbDeviceContext);
    void processConnectionQueue();

    struct ConnectionTask {
        QString udid;
        uint16_t port;
        bool rawMode;
        QPointer<DeviceConnection> conn; // 使用 QPointer 防止排队期间对象被销毁导致悬空指针
        UsbDeviceContext* ctx;
        int retries = 0;
    };

    QQueue<ConnectionTask> connectionQueue;
    int pendingConnections = 0;
    const int MAX_CONCURRENT_CONNECTIONS = 20;
    const int MISSING_POLLS_BEFORE_DISCONNECT = 3;

    QSet<QString> previousDevices;
    QHash<QString, int> missingPollCounts;
    QHash<UsbDeviceContext*, QByteArray> deviceBuffers;

    QHash<DeviceConnection*, UsbDeviceContext*> connToContext;
    QHash<QString, bool> devices;

    QFutureWatcher<QSet<QString>>* watcher;
    QTimer* pollTimer;
    QTimer* connectTimer;
    
    QThreadPool* usbThreadPool; // 独立的 USB 线程池，防止底层 API 阻塞全局线程池
};
