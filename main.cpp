#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "UdpTransport.h"
#include "NetworkUtils.h"
#include "DeviceInfo.h"
#include "EventHub.h"
#include "DeviceWindow.h"
#include "UsbDeviceManager.h"
#include "LoginWidget.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QLoggingCategory>
#include <QHostInfo>

void onClientConnected(QTcpSocket* socket) {

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
    DeviceConnection *connection = DeviceConnection::find(socket);
    EventHub::trigger("disconnected", QJsonValue(), connection);
    delete connection;
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

    auto loginWidget = new LoginWidget();
    loginWidget->show();

    QObject::connect(loginWidget, &QObject::destroyed, [=]() {
        auto tcpServer = new TcpServer(onClientConnected, [](QTcpSocket* socket, const QJsonObject &jsonObject) {
            DeviceConnection *connection = DeviceConnection::find(socket);
            if (!connection)
                connection = new DeviceConnection(socket);

            onDataReceived(connection, jsonObject);
        }, onClientDisconnected, onError);

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::deviceDisconnected, [](DeviceConnection* conn){
            EventHub::trigger("disconnected", QJsonValue(), conn);
        });

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::dataReceived, [](DeviceConnection* conn, const QJsonObject& data){
            onDataReceived(conn, data);
        });

        QObject::connect(g_usbDeviceManager, &UsbDeviceManager::errorOccurred, [](DeviceConnection* conn, const QString& msg){
            qCriticalEx() << "⚠️ 设备错误:" << msg;
        });

        g_usbDeviceManager->start();

        QObject::connect(qApp, &QApplication::aboutToQuit, [=]() {
            g_usbDeviceManager->stop();
        });

        QString localIP = NetworkUtils::getLocalIP();
        qDebugEx() << "本机内网IP:" << localIP;

        auto udpTransport = new UdpTransport(
            [](const QJsonObject &jsonObject) {
                qDebugEx() << "Received Data:" << jsonObject;
            }
        );

        QList<QHostAddress> subnetIPs = NetworkUtils::getSubnetIPs(localIP);
        for (const QHostAddress &ip : subnetIPs) {
            // qDebugEx() << "同子网IP: " << ip.toString();
            udpTransport->sendData(tcpServer->getHostInfo(localIP), ip, 32838);
        }
    });

    g_mainWindow->show();
    loginWidget->close();

    return app.exec();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
