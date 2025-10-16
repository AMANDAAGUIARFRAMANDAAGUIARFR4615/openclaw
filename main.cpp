#include "Logger.h"
#include "MainWindow.h"
#include "TcpServer.h"
#include "UdpTransport.h"
#include "NetworkUtils.h"
#include "LogWindow.h"
#include "DeviceInfo.h"
#include "EventHub.h"
#include <QApplication>
#include <QLoggingCategory>
#include <QShortcut>

MainWindow* mainWindow;

void onClientConnected(QTcpSocket* socket) {
    // qDebugEx() << "有新的客户端连接！";
}

void onDataReceived(QTcpSocket* socket, const QJsonObject &jsonObject) {
    auto event = jsonObject["event"].toString();
    auto data = jsonObject["data"];

    if (event == "ping")
        return;

    qDebugEx() << event << data;

    EventHub::TriggerEvent(event, data, socket);
}

void onClientDisconnected(QTcpSocket* socket) {
    // qDebugEx() << "客户端断开连接！";
}

void onError(QTcpSocket* socket, QAbstractSocket::SocketError socketError) {
    qCriticalEx() << "发生错误：" << socketError;
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    // qputenv("QT_FFMPEG_DEBUG", "1");

    QLoggingCategory::setFilterRules("qt.multimedia.*=true");

    mainWindow = new MainWindow;
    auto logWindow = new LogWindow(mainWindow);
    logWindow->resize(mainWindow->size().width(), 400);
    auto shortcut = new QShortcut(QKeySequence(Qt::Key_F5), mainWindow);
    QObject::connect(shortcut, &QShortcut::activated, logWindow, &LogWindow::toggleVisibility);

    TcpServer server(onClientConnected, onDataReceived, onClientDisconnected, onError);

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
        udpTransport.sendData(QJsonObject{{"ip", localIP}, {"port", server.serverPort()}}, ip, 32838);
    }

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int x = (screenGeometry.width() - mainWindow->width()) / 2;
    int y = (screenGeometry.height() - mainWindow->height()) / 2;
    mainWindow->move(x, y);

    mainWindow->show();
    // mainWindow->showMinimized();

    // QTimer::singleShot(3000, [&mainWindow](){
    //     qDebugEx() << "singleShot";
    //     mainWindow.addItem();
    //     mainWindow.addItem();
    //     mainWindow.addItem();
    //     mainWindow.addItem();
    //     mainWindow.addItem();
    // });

    // auto player = new DeviceWidget();
    // // player->setSource(QString("tcp://192.168.0.102:23145"));
    // mainWindow.addPlayer(player);
    // mainWindow.addPlayer(new DeviceWidget());
    // mainWindow.addPlayer(new DeviceWidget());

    return application.exec();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
