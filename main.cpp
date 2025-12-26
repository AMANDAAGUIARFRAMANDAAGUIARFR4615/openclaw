#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "NetworkUtils.h"
#include "DeviceInfo.h"
#include "EventHub.h"
#include "DeviceWindow.h"
#include "UsbDeviceManager.h"
#include "LoginWidget.h"
#include "LogWindow.h"
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
    connection->deleteLater();
}

void onError(QTcpSocket* socket, QAbstractSocket::SocketError socketError) {
    qCriticalEx() << "发生错误：" << socketError;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QTranslator qtTranslator;
    if (qtTranslator.load("qt_zh_CN", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    app.setApplicationDisplayName("RemotePro");

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QDir dir(dataPath);
    if (!dir.exists())
        dir.mkpath(".");

    new LogWindow();

#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    QTimer* timer = new QTimer();
    timer->setInterval(1000 + (rand() % 500));
    QObject::connect(timer, &QTimer::timeout, [](){
        bool detected = false;

        if (IsDebuggerPresent())
            detected = true;

        BOOL isRemote = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemote);
        if (isRemote)
            detected = true;

#ifdef _WIN64
        auto peb = (char*)__readgsqword(0x60);
#else
        auto peb = (char*)__readfsdword(0x30);
#endif
        if (peb[2])
            detected = true;

        if (detected)
            __fastfail(7);
    });
    timer->start();
#endif

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

    QObject::connect(loginWidget, &LoginWidget::authorized, [=]() {
        auto tcpServer = new TcpServer(onClientConnected, [](QTcpSocket* socket, const QJsonObject &jsonObject) {
            DeviceConnection *connection = DeviceConnection::find(socket);
            if (!connection)
                connection = new DeviceConnection(socket);

            onDataReceived(connection, jsonObject);
        }, onClientDisconnected, onError);

        QObject::connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::deviceDisconnected, [](DeviceConnection* conn){
            EventHub::trigger("disconnected", QJsonValue(), conn);
        });

        QObject::connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::dataReceived, [](DeviceConnection* conn, const QJsonObject& data){
            onDataReceived(conn, data);
        });

        QObject::connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::errorOccurred, [](DeviceConnection* conn, const QString& msg){
            qCriticalEx() << "⚠️ 设备错误:" << msg;
        });

        UsbDeviceManager::getInstance()->start();

        QObject::connect(qApp, &QApplication::aboutToQuit, [=]() {
            UsbDeviceManager::getInstance()->stop();
        });

        MainWindow::getInstance()->show();
    });

    return app.exec();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
