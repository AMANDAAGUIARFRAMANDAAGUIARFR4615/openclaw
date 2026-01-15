#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "EventHub.h"
#include "UsbDeviceManager.h"
#include "LoginWidget.h"
#include "LogWindow.h"
#include "Account.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QStyleHints>
#include <QStyleFactory>

void onDataReceived(DeviceConnection *connection, const QJsonObject &jsonObject) {
    auto event = jsonObject["event"].toString();
    auto data = jsonObject["data"];

    qDebugEx() << event << data;

    EventHub::trigger(event, data, connection);
}

class CursorFilter : public QObject
{
public:
    CursorFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // 拦截 "Polish" 事件 (这个事件在控件初始化或样式表改变时触发)
        if (event->type() == QEvent::Polish) {
            if (auto button = qobject_cast<QAbstractButton *>(obj))
                button->setCursor(Qt::PointingHandCursor);
        }

        return QObject::eventFilter(obj, event);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (auto styleHints = QGuiApplication::styleHints())
        styleHints->setColorScheme(Qt::ColorScheme::Light);

    app.setStyle(QStyleFactory::create("Fusion"));

    app.installEventFilter(new CursorFilter(&app));

    QTranslator qtTranslator;
    if (qtTranslator.load("qt_zh_CN", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

#ifdef QT_DEBUG
    app.setApplicationDisplayName("远控Pro测试版");
#else
    app.setApplicationDisplayName(QString("远控Pro[%1]").arg(Config::VERSION));
#endif

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QDir dir(dataPath);
    if (!dir.exists())
        dir.mkpath(".");

    new LogWindow();

    qDebug() << QStyleFactory::keys(); 

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
        {
            __fastfail(7);
            *(int*)qApp = 0;
        }
    });
    timer->start();
#endif

    // QObject::connect(&app, &QApplication::focusChanged, [](QWidget *old, QWidget *now) {
    //     qDebugEx() << "焦点从" << old << "变为" << now;

    //     if (!now || now->isWindow())
    //         return;

    //     auto window = qobject_cast<DeviceWindow*>(now->window());
    //     if (window)
    //         window->setFocus();
    // });

    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    // qputenv("QT_FFMPEG_DEBUG", "1");
    // qputenv("QT_MEDIA_BACKEND", "ffmpeg");

    // QLoggingCategory::setFilterRules("qt.multimedia.*=true");

    auto loginWidget = new LoginWidget();
    loginWidget->show();

    if (settings->contains("force_logout"))
    {
        QMessageBox::warning(loginWidget, "下线通知", settings->value("force_logout").toString());
        settings->remove("force_logout");
    }

    QObject::connect(webSocketClient, &QWebSocket::connected, []() {
        if (!Account::getInstance()->id.isEmpty())
            webSocketClient->emitEvent("reconnect", Account::getInstance()->id);
    });

    webSocketClient->on("force_logout", [](const QJsonValue &data) {
        settings->setValue("force_logout", data.toString());

        QProcess::startDetached(qApp->applicationFilePath());
        qApp->quit();
    });

    QObject::connect(loginWidget, &LoginWidget::authorized, [=](const QJsonValue &account) {
        Account::getInstance()->id = account["_id"].toString();
        Account::getInstance()->phone = account["phone"].toString();
        Account::getInstance()->balance = account["balance"].toInt();
        Account::getInstance()->loginTime = account["loginTime"].toInteger();

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
            std::exit(0);
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
