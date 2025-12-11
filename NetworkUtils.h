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
            // --- VPN 与 隧道协议 ---
            "zerotier",     // ZeroTier 异地组网虚拟网卡
            "tap-windows",  // OpenVPN 等 VPN 软件使用的虚拟 TAP 驱动
            "vpn",          // 各类 VPN 软件的通用标识
            "utun",         // macOS/iOS 用户态网络隧道 (User Tunnel)
            "pseudo",       // 伪设备接口 / 隧道接口
            "gif",          // IPv4 到 IPv6 的通用隧道接口
            "stf",          // 6to4 隧道接口

            // --- 虚拟机 与 容器 ---
            "vmware",       // VMware 虚拟机网卡 (VMnet系列)
            "virtualbox",   // VirtualBox 虚拟机网卡
            "vbox",         // VirtualBox 的缩写标识
            "docker",       // Docker 容器默认网桥 (docker0)
            "wsl",          // Windows Subsystem for Linux (WSL2) 虚拟网卡

            // --- 特定硬件与协议 ---
            "awdl",         // Apple Wireless Direct Link (AirDrop 专用)
            "llw",          // Apple Low Latency WLAN (Sidecar 专用)
            "feth",         // Linux veth pair (容器间通信接口)

            // --- 特殊的虚拟交换与热点 (精确匹配，避免误杀物理网卡) ---
            "default switch",               // Hyper-V 强制生成的内部 NAT 交换机
            "microsoft wi-fi direct virtual" // Windows 移动热点虚拟适配器
        };

        for (const QString &kw : keywords) {
            if (name.contains(kw) || dName.contains(kw)) {
                return true; // 是虚拟网卡
            }
        }
        return false; // 看起来像物理网卡
    }

    static QStringList getPhysicalIPs() {
        QStringList ips;

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

                ips.append(ipStr);
            }
        }

        return ips;
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
