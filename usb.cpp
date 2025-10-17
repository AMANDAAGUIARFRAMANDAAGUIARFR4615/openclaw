#include "DeviceConnection.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <libimobiledevice/libimobiledevice.h>
#include <QSocketNotifier>
#include <QHostInfo>

static void device_event_callback(const idevice_event_t* event, void* user_data)
{
    if (!event)
        return;

    switch (event->event)
    {
    case IDEVICE_DEVICE_ADD:
        qDebug() << "检测到设备插入:" << event->udid;
        break;

    case IDEVICE_DEVICE_REMOVE:
        qDebug() << "检测到设备拔出:" << event->udid;
        break;

    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    idevice_event_subscribe(device_event_callback, nullptr);

    idevice_t device = NULL;
    idevice_connection_t connection = NULL;

    if (IDEVICE_E_SUCCESS != idevice_new(&device, NULL)) {
        qDebug() << "无法找到设备";
        return -1;
    }

    uint16_t port = 32839;
    if (idevice_connect(device, port, &connection) != IDEVICE_E_SUCCESS) {
        qDebug() << "连接失败";
        idevice_free(device);
        return -1;
    }

    qDebug() << "成功连接 iOS USB 端口" << port;

    int fd = -1;
    if (idevice_connection_get_fd(connection, &fd) != IDEVICE_E_SUCCESS || fd < 0) {
        qDebug() << "无法获取文件描述符 (libimobiledevice 后端不支持)";
    } else {
        qDebug() << "fd =" << fd;

        QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read);

        QObject::connect(notifier, &QSocketNotifier::activated, [&](int){
            char buffer[512];
            uint32_t bytes = 0;
            idevice_error_t err = idevice_connection_receive(connection, buffer, sizeof(buffer), &bytes);
            if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                qDebug() << "收到数据:" << QByteArray(buffer, bytes);
            }
        });
    }

    DeviceConnection *deviceConnection = new DeviceConnection(connection);
    deviceConnection->send("deviceInfo", QJsonObject{{"remoteDeviceName", QHostInfo::localHostName()}});

    QObject::connect(&a, &QCoreApplication::aboutToQuit, [&] {
        idevice_disconnect(connection);
        idevice_free(device);
        idevice_event_unsubscribe();
    });

    return a.exec();
}
