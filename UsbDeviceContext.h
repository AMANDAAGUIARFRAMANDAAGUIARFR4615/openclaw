#pragma once

#include <QSocketNotifier>
#include <QByteArray>
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#include <libimobiledevice/libimobiledevice.h>
#endif

class DeviceConnection;

struct UsbDeviceContext {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    idevice_t device = nullptr;
    idevice_connection_t connection = nullptr;
#endif
    QSocketNotifier* notifier = nullptr;
    DeviceConnection* handler = nullptr;
    QString udid;
    uint16_t port = 0;

    QByteArray readBuffer; 

    UsbDeviceContext() {
        readBuffer.resize(1024 * 1024); // 预分配 1MB
    }
};