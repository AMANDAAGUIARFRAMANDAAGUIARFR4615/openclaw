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

// 辅助函数：将接口类型枚举转换为字符串
QString getInterfaceTypeName(QNetworkInterface::InterfaceType type) {
    switch (type) {
        case QNetworkInterface::Loopback: return "Loopback (回环)";
        case QNetworkInterface::Virtual: return "Virtual (虚拟)";
        case QNetworkInterface::Ethernet: return "Ethernet (以太网)";
        case QNetworkInterface::Wifi: return "Wifi (无线)";
        case QNetworkInterface::CanBus: return "CanBus";
        case QNetworkInterface::Fddi: return "FDDI";
        case QNetworkInterface::Ppp: return "PPP (点对点)";
        case QNetworkInterface::Phonet: return "Phonet";
        case QNetworkInterface::Ieee802154: return "IEEE 802.15.4";
        case QNetworkInterface::SixLoWPAN: return "6LoWPAN";
        case QNetworkInterface::Unknown:
        default: return "Unknown (未知)";
    }
}

// 辅助函数：将标志位转换为字符串列表
QJsonArray getInterfaceFlags(QNetworkInterface::InterfaceFlags flags) {
    QJsonArray flagArray;
    if (flags & QNetworkInterface::IsUp) flagArray << "IsUp (启用)";
    if (flags & QNetworkInterface::IsRunning) flagArray << "IsRunning (运行中)";
    if (flags & QNetworkInterface::CanBroadcast) flagArray << "CanBroadcast (支持广播)";
    if (flags & QNetworkInterface::IsLoopBack) flagArray << "IsLoopback (回环)";
    if (flags & QNetworkInterface::IsPointToPoint) flagArray << "IsP2P (点对点)";
    if (flags & QNetworkInterface::CanMulticast) flagArray << "CanMulticast (支持组播)";
    
    return flagArray;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationDisplayName("RemotePro");

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

    // 获取所有网卡
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    // 根 JSON 数组，用于存放所有网卡对象
    QJsonArray rootArray;

    for (const QNetworkInterface &netInterface : interfaces) {
        QJsonObject ifaceObj;

        // 1. 填充网卡基本属性
        ifaceObj["index"] = netInterface.index();
        ifaceObj["name"] = netInterface.name(); // 系统名 (eth0, {UUID})
        ifaceObj["humanName"] = netInterface.humanReadableName(); // 可读名 (Ethernet, Wi-Fi)
        ifaceObj["mac"] = netInterface.hardwareAddress();
        ifaceObj["type"] = getInterfaceTypeName(netInterface.type());
        ifaceObj["mtu"] = netInterface.maximumTransmissionUnit();
        ifaceObj["flags"] = getInterfaceFlags(netInterface.flags());

        // 2. 填充 IP 地址列表
        QJsonArray ipArray;
        QList<QNetworkAddressEntry> entries = netInterface.addressEntries();

        for (const QNetworkAddressEntry &entry : entries) {
            QJsonObject ipObj;
            QHostAddress ip = entry.ip();
            QHostAddress netmask = entry.netmask();
            QHostAddress broadcast = entry.broadcast();

            // 区分 IPv4 和 IPv6
            if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                ipObj["protocol"] = "IPv4";
            } else if (ip.protocol() == QAbstractSocket::IPv6Protocol) {
                ipObj["protocol"] = "IPv6";
            } else {
                ipObj["protocol"] = "Other";
            }

            ipObj["ip"] = ip.toString();

            // 如果有子网掩码则添加
            if (!netmask.isNull()) {
                ipObj["netmask"] = netmask.toString();
            }
            
            // 如果有广播地址则添加
            if (!broadcast.isNull()) {
                ipObj["broadcast"] = broadcast.toString();
            }
            
            // CIDR 前缀长度
            if (entry.prefixLength() != -1) {
                ipObj["prefixLen"] = entry.prefixLength();
            }

            ipArray.append(ipObj);
        }

        // 将 IP 数组放入网卡对象
        ifaceObj["ipAddresses"] = ipArray;

        // 将网卡对象放入根数组
        rootArray.append(ifaceObj);
    }

    // 3. 转换为 JSON 文档并打印
    QJsonDocument doc(rootArray);
    
    // Indented 格式表示带缩进和换行，方便阅读
    // 如果要压缩格式（单行），使用 QJsonDocument::Compact
    QByteArray formattedJson = doc.toJson(QJsonDocument::Indented);

    qDebug().noquote() << formattedJson;

    return app.exec();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
