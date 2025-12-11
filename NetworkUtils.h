#pragma once

#include "Logger.h"
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QString>
#include <QList>
#include <QRegularExpression>

class NetworkUtils
{
public:
    // 判断是否为虚拟网卡名称的辅助函数
    static bool isVirtualAdapter(const QString &humanName, const QString &devName) {
        // 转小写以便不区分大小写比较
        QString name = humanName.toLower();
        QString dName = devName.toLower();

        // 常见的虚拟网卡关键字黑名单
        QStringList keywords = {
            "zerotier",     // ZeroTier
            "vmware",       // VMware 虚拟机
            "virtualbox",   // VirtualBox 虚拟机
            "vbox",         // VirtualBox
            "virtual",      // 通用虚拟
            "pseudo",       // 伪接口 (如 Loopback Pseudo-Interface)
            "tap-windows",  // OpenVPN 常用的 TAP 适配器
            "vpn",          // 各类 VPN
            "docker",       // Docker 容器
            "wsl",          // Windows Subsystem for Linux
            "hyper-v",      // 微软 Hyper-V
            "switch"        // 虚拟交换机
        };

        for (const QString &kw : keywords) {
            if (name.contains(kw) || dName.contains(kw)) {
                return true; // 是虚拟网卡
            }
        }
        return false; // 看起来像物理网卡
    }

    static QStringList getPhysicalIPs() {
        QStringList priority1; // 192.168.x.x (物理 Wifi/LAN 首选)
        QStringList priority2; // 其他物理 IPv4 (10.x, 172.x, 公网IP)

        const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

        for (const QNetworkInterface &interface : interfaces) {
            if (!(interface.flags() & QNetworkInterface::IsUp) || !(interface.flags() & QNetworkInterface::IsRunning))
                continue;

            if (interface.flags() & QNetworkInterface::IsLoopBack)
                continue;

            // 如果类型明确是 Wifi，绝对是物理网卡，跳过黑名单检查
            bool isWifi = (interface.type() == QNetworkInterface::Wifi);
            
            if (!isWifi) {
                // 如果不是 Wifi (通常是 Ethernet)，则检查名字是否包含虚拟关键字
                if (isVirtualAdapter(interface.humanReadableName(), interface.name())) {
                    qDebug() << "过滤掉虚拟网卡:" << interface.humanReadableName();
                    continue; 
                }
            }

            const QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                QHostAddress ip = entry.ip();

                if (ip.protocol() != QAbstractSocket::IPv4Protocol)
                    continue;

                QString ipStr = ip.toString();

                // 排除 169.254 (自动专用IP，无效)
                if (ipStr.startsWith("169.254"))
                    continue;

                if (isWifi || ipStr.startsWith("192.168."))
                    priority1.append(ipStr);
                else
                    priority2.append(ipStr);
            }
        }

        return priority1 + priority2;
    }

    // 遍历同一子网的IP地址，IP范围从 .1 到 .254
    static QList<QHostAddress> getSubnetIPs(const QString& localIP)
    {
        QList<QHostAddress> ipList;

        QHostAddress networkAddress(localIP);
        if (networkAddress.isNull()) {
            qCriticalEx() << "无效的子网地址";
            return ipList;
        }

        quint32 networkIpv4 = networkAddress.toIPv4Address();

        // 遍历 IP 地址范围（1到254）
        for (int i = 1; i <= 254; ++i) {
            // 直接修改最后一个字节，生成有效的主机地址
            quint32 ipIpv4 = (networkIpv4 & 0xFFFFFF00) | i;

            QHostAddress ip(ipIpv4);

            if (ip.toString() != localIP)
                ipList.append(ip);
        }

        return ipList;
    }
};
