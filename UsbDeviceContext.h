#pragma once

#include <QSocketNotifier>
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include <libimobiledevice/libimobiledevice.h>
#endif

class DeviceConnection;

struct UsbDeviceContext {
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    idevice_t device = nullptr;
    idevice_connection_t connection = nullptr;
#endif
    QSocketNotifier* notifier = nullptr;
    DeviceConnection* handler = nullptr;
    QString udid;
    uint16_t port = 0;
};
