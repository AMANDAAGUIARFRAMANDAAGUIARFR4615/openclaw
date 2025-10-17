#include "DeviceConnection.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <libimobiledevice/libimobiledevice.h>
#include <QSocketNotifier>
#include <QHostInfo>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    idevice_t device = NULL;
    idevice_connection_t connection = NULL;

    if (IDEVICE_E_SUCCESS != idevice_new(&device, NULL)) {
        qDebugEx() << "无法找到设备";
        return -1;
    }

    uint16_t port = 32839;
    if (idevice_connect(device, port, &connection) != IDEVICE_E_SUCCESS) {
        qDebugEx() << "连接失败";
        idevice_free(device);
        return -1;
    }

    qDebugEx() << "成功连接 iOS USB 端口" << port;

    int fd = -1;
    if (idevice_connection_get_fd(connection, &fd) != IDEVICE_E_SUCCESS || fd < 0) {
        qDebugEx() << "无法获取文件描述符 (libimobiledevice 后端不支持)";
    } else {
        qDebugEx() << "fd =" << fd;

        QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read);

        QObject::connect(notifier, &QSocketNotifier::activated, [&](int){
            char buffer[512];
            uint32_t bytes = 0;
            idevice_error_t err = idevice_connection_receive(connection, buffer, sizeof(buffer), &bytes);
            if (err == IDEVICE_E_SUCCESS && bytes > 0) {
                qDebugEx() << "收到数据:" << QByteArray(buffer, bytes);
            }
        });
    }

    DeviceConnection *deviceConnection = new DeviceConnection(connection);
    deviceConnection->send("deviceInfo", QJsonObject{{"remoteDeviceName", QHostInfo::localHostName()}});

    QObject::connect(&a, &QCoreApplication::aboutToQuit, [&] {
        idevice_disconnect(connection);
        idevice_free(device);
    });

    return a.exec();
}
