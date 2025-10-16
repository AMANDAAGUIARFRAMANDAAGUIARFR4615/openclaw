#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <libimobiledevice/libimobiledevice.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    idevice_t device = NULL;
    idevice_connection_t connection = NULL;

    // 连接 iPhone
    if (IDEVICE_E_SUCCESS != idevice_new(&device, NULL)) {
        qDebug() << "无法找到设备";
        return -1;
    }

    uint16_t port = 32839;
    idevice_error_t err = idevice_connect(device, port, &connection);

    if (err != IDEVICE_E_SUCCESS) {
        qDebug() << "连接失败:" << err;
        idevice_free(device);
        return -1;
    }

    qDebug() << "成功连接 iOS USB 端口" << port;

    // 发送数据
    const char *test_data = "Hello from Qt over USB!";
    uint32_t bytes_sent = 0;
    err = idevice_connection_send(connection, test_data, strlen(test_data), &bytes_sent);

    if (err == IDEVICE_E_SUCCESS)
        qDebug() << "成功发送数据，字节数:" << bytes_sent;
    else
        qDebug() << "发送失败:" << err;

    // 接收返回（可选）
    char buffer[256] = {0};
    uint32_t bytes_received = 0;
    if (idevice_connection_receive_timeout(connection, buffer, sizeof(buffer)-1, &bytes_received, 2000) == IDEVICE_E_SUCCESS) {
        qDebug() << "接收数据:" << buffer;
    } else {
        qDebug() << "2秒内未收到返回";
    }

    idevice_disconnect(connection);
    idevice_free(device);
    return 0;
}
