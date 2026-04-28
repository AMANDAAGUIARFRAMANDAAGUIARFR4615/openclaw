#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "EventHub.h"
#include "UsbDeviceManager.h"
#include "LoginWidget.h"
#include "LogTextBrowser.h"
#include "Account.h"
#include "DeviceWindow.h"
#include <QApplication>
#include <QNetworkProxy>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QStyleHints>
#include <QStyleFactory>

void onDataReceived(DeviceConnection *connection, const QJsonObject &jsonObject) {
    auto event = jsonObject["event"].toString();
    auto data = jsonObject["data"];

    qDebugEx() << connection << event << data;

    EventHub::trigger(event, data, connection);
}

class GlobalEventFilter : public QObject
{
public:
    GlobalEventFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // 拦截 "Polish" 事件 (这个事件在控件初始化或样式表改变时触发)
        if (event->type() == QEvent::Polish) {
            if (auto button = qobject_cast<QAbstractButton *>(obj))
                button->setCursor(Qt::PointingHandCursor);
        }

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        if (event->type() == QEvent::ContextMenu) {
            // 返回 true 表示事件已处理，不再向下传递，从而阻止菜单弹出
            return true;
        }
#endif

        return QObject::eventFilter(obj, event);
    }
};

QSettings* settings;
WebSocketClient *webSocketClient;
QElapsedTimer* elapsedTimer;
QNetworkAccessManager* networkAccessManager;
TunnelClient* tunnelClient;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    settings = new QSettings("deepseek", "RemotePro");
    webSocketClient = new WebSocketClient();
    elapsedTimer = new QElapsedTimer();
    networkAccessManager = new QNetworkAccessManager();

// #if defined(Q_OS_IOS)
//     QNetworkRequest request(QUrl("http://captive.apple.com"));
//     networkAccessManager->get(request);
// #endif

    QString ip = Tools::getIpFromDomain("ws.remotepro.cn");
    if (ip.isEmpty()) {
        QMessageBox::warning(nullptr, "警告", "域名解析失败，请稍后重试");
        return 0;
    }

    qApp->setProperty("SERVER_IP", ip);

    auto lockFile = new QLockFile(QDir::temp().absoluteFilePath("RemotePro.lock"));

    // 尝试加锁，设置超时时间为 100 毫秒（防止之前的僵死进程导致的短暂锁定）
    if (!lockFile->tryLock(100)) {
        QMessageBox::warning(nullptr, "警告", "应用程序已经在运行中");
        return 0;
    }

    if (QFile::exists(qApp->applicationFilePath() + ".old")) {
#if defined(Q_OS_MACOS)
        QDir dir(QCoreApplication::applicationDirPath());
        dir.cdUp();
        dir.cdUp();
        Tools::removeFilesRecursively(dir.absolutePath(), {"*.old"});
#else
        Tools::removeFilesRecursively(QCoreApplication::applicationDirPath(), {"*.old"});
#endif
    }

    app.styleHints()->setColorScheme((Qt::ColorScheme)settings->value("colorScheme").toInt());
    QStyle *fusionStyle = QStyleFactory::create("Fusion");
    app.setStyle(fusionStyle);
    app.setPalette(fusionStyle->standardPalette());

    app.installEventFilter(new GlobalEventFilter(&app));

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QDir dir(dataPath);
    if (!dir.exists())
        dir.mkpath(".");

    new LogTextBrowser();

    QTranslator qtTranslator;
#ifdef QT_DEBUG
    QString translationsPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#else
    QString translationsPath = qApp->applicationDirPath() + "/translations";
#endif
    if (qtTranslator.load("qt_zh_CN", translationsPath))
        app.installTranslator(&qtTranslator);

#ifdef QT_DEBUG
    app.setApplicationDisplayName("远控Pro测试版");
#else
    app.setApplicationDisplayName(QString("远控Pro[%1]").arg(Config::VERSION));
#endif

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

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    QObject::connect(&app, &QApplication::focusChanged, [](QWidget *old, QWidget *now) {
        qDebugEx() << "焦点从" << old << "变为" << now;

        if (!now || now->isWindow())
            return;

        auto window = qobject_cast<DeviceWindow*>(now->window());
        if (window)
            window->setFocus();
    });
#endif

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
        if (Account::getInstance()->id.isEmpty())
            return;

        webSocketClient->emitEvent("reconnect", Account::getInstance()->id, [=](const QJsonValue &res) {
            for (const QJsonValue& device: res[HIDE_STR("devices")].toArray()) {
                const auto udid = device[HIDE_STR("udid")].toString();
                const auto expireAt = device[HIDE_STR("expireAt")].toInteger();
                DeviceInfo::expirations[udid] = expireAt;
                DeviceInfo::remotePorts[udid] = static_cast<quint16>(device["remotePort"].toInt());
                DeviceInfo::setLocker(udid, device["locker"].toString());

                auto deviceInfo = DeviceInfo::getDevice(udid);
                if (deviceInfo)
                    deviceInfo->expireAt = expireAt;
            }
        });
    });

    webSocketClient->on("force_logout", [](const QJsonValue &data) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        settings->setValue("force_logout", data.toString());
        Tools::quitApplication(true);
#endif
    });

    QObject::connect(loginWidget, &LoginWidget::authorized, [&](const QJsonValue &account, bool isLanMode) {
        Account::getInstance()->id = account["_id"].toString();
        Account::getInstance()->phone = account["phone"].toString();
        Account::getInstance()->balance = account["balance"].toInt();
        Account::getInstance()->hasRedeemCode = account["hasRedeemCode"].toBool();
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

        if (!isLanMode) {
            if (!tunnelClient) {
                tunnelClient = new TunnelClient(Config::SERVER_IP(), 10000, 10001);
                tunnelClient->connectToServer();
            }

            const qint16 mainPort = TcpServer::getInstance()->serverPort();

            QObject::connect(tunnelClient, &TunnelClient::remotePortChanged, &app,
                             [=](const QString &lanIp, quint16 lanPort, quint16 remotePort) {
                                 if (lanPort == mainPort && remotePort > 0)
                                     qApp->setProperty("MAIN_REMOTE_PORT", remotePort);
                             });
            tunnelClient->requestAdd("127.0.0.1", mainPort);
        }

        QObject::connect(qApp, &QApplication::aboutToQuit, [=]() {
            qInstallMessageHandler(nullptr);
            lockFile->unlock();
            UsbDeviceManager::getInstance()->stop();
            MainWindow::getInstance()->deleteLater();
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
