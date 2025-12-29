#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "EventHub.h"
#include "DeviceWindow.h"
#include "UsbDeviceManager.h"
#include "LoginWidget.h"
#include "LogWindow.h"
#include "Account.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QLoggingCategory>

void onDataReceived(DeviceConnection *connection, const QJsonObject &jsonObject) {
    auto event = jsonObject["event"].toString();
    auto data = jsonObject["data"];

    qDebugEx() << event << data;

    EventHub::trigger(event, data, connection);
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

#if defined(Q_OS_WIN) && !defined(QT_DEBUG)
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

    QObject::connect(webSocketClient, &QWebSocket::connected, []() {
        if (!Account::getInstance()->id.isEmpty())
            webSocketClient->emitEvent("reconnect", Account::getInstance()->id);
    });

    webSocketClient->on("force_logout", [this](const QJsonValue &data) {
        qDebugEx("被强制踢出");
    });

    QObject::connect(loginWidget, &LoginWidget::authorized, [=](const QJsonValue &account) {
        Account::getInstance()->id = account["_id"].toString();
        Account::getInstance()->phone = account["phone"].toString();
        Account::getInstance()->balance = account["balance"].toInt();

        QObject::connect(TcpServer::getInstance(), &TcpServer::clientDisconnected, [](DeviceConnection* conn){
            EventHub::trigger("disconnected", QJsonValue(), conn);
        });

        QObject::connect(TcpServer::getInstance(), &TcpServer::dataReceived, onDataReceived);

        QObject::connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::deviceDisconnected, [](DeviceConnection* conn){
            EventHub::trigger("disconnected", QJsonValue(), conn);
        });

        QObject::connect(UsbDeviceManager::getInstance(), &UsbDeviceManager::dataReceived, onDataReceived);

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
