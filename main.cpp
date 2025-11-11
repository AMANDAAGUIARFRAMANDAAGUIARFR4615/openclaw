#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "UdpTransport.h"
#include "NetworkUtils.h"
#include "LogWindow.h"
#include "DeviceInfo.h"
#include "EventHub.h"
#include "DeviceWindow.h"
#include "UsbDeviceManager.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QLoggingCategory>
#include <QShortcut>
#include <QHostInfo>

void onClientConnected(QTcpSocket* socket) {
    // qDebugEx() << "有新的客户端连接！";
}

void onDataReceived(DeviceConnection *connection, const QJsonObject &jsonObject) {
    auto event = jsonObject["event"].toString();
    auto data = jsonObject["data"];

    if (event == "ping")
        return;

    qDebugEx() << event << data;

    EventHub::trigger(event, data, connection);
}

void onClientDisconnected(QTcpSocket* socket) {
    // qDebugEx() << "客户端断开连接！";
}

void onError(QTcpSocket* socket, QAbstractSocket::SocketError socketError) {
    qCriticalEx() << "发生错误：" << socketError;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QObject::connect(&app, &QApplication::focusChanged, [](QWidget *old, QWidget *now) {
        qDebugEx() << "焦点从" << old << "变为" << now;

        if (!now || now->isWindow())
            return;

        auto window = qobject_cast<DeviceWindow*>(now->window());
        if (window)
            window->setFocus();
    });

    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // qputenv("QT_FFMPEG_DEBUG", "1");
    // qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // QLoggingCategory::setFilterRules("qt.multimedia.*=true");

    auto logWindow = new LogWindow(g_mainWindow);
    logWindow->resize(g_mainWindow->size().width(), 400);
    auto shortcut = new QShortcut(QKeySequence(Qt::Key_F5), g_mainWindow);
    QObject::connect(shortcut, &QShortcut::activated, logWindow, &LogWindow::toggleVisibility);

    TcpServer server(onClientConnected, [](QTcpSocket* socket, const QJsonObject &jsonObject) {
            DeviceConnection *connection = DeviceConnection::find(socket);
            if (!connection)
                connection = new DeviceConnection(socket);

            onDataReceived(connection, jsonObject);
        }, onClientDisconnected, onError);

    // if (QOperatingSystemVersion::current().type() == QOperatingSystemVersion::Windows || QOperatingSystemVersion::current().type() == QOperatingSystemVersion::MacOS) {
        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::deviceConnected, [](DeviceConnection* conn){
            qDebugEx() << "✅ 设备已连接:" << conn;
        });

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::deviceDisconnected, [](DeviceConnection* conn){
            qDebugEx() << "❌ 设备已断开:" << conn;
        });

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::dataReceived, [](DeviceConnection* conn, const QJsonObject& data){
            onDataReceived(conn, data);
        });

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::errorOccurred, [](DeviceConnection* conn, const QString& msg){
            qCriticalEx() << "⚠️ 设备错误:" << msg;
        });

        g_usbDeviceManager->start();

        QObject::connect(&app, &QCoreApplication::aboutToQuit, [=]() {
            g_usbDeviceManager->stop();
        });
    // }

    QString localIP = NetworkUtils::getLocalIP();
    qDebugEx() << "本机内网IP:" << localIP;

    UdpTransport udpTransport(
        [](const QJsonObject &jsonObject) {
            qDebugEx() << "Received Data:" << jsonObject;
        }
    );

    QList<QHostAddress> subnetIPs = NetworkUtils::getSubnetIPs(localIP);
    for (const QHostAddress &ip : subnetIPs) {
        // qDebugEx() << "同子网IP: " << ip.toString();
        udpTransport.sendData(server.getHostInfo(localIP), ip, 32838);
    }

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int x = (screenGeometry.width() - g_mainWindow->width()) / 2;
    int y = (screenGeometry.height() - g_mainWindow->height()) / 2;
    g_mainWindow->move(x, y);

    g_mainWindow->show();

    return app.exec();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
