#include "DeviceConnection.h"
#include <QSocketNotifier>
#include <libimobiledevice/libimobiledevice.h>

struct UsbDeviceContext {
    idevice_t device = nullptr;
    idevice_connection_t connection = nullptr;
    QSocketNotifier* notifier = nullptr;
    DeviceConnection* handler = nullptr;
    QString udid;
    uint16_t port = 0;
};