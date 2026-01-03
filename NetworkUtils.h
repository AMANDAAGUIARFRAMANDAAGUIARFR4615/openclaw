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

        QStringList keywords = {
            // --- Windows Hyper-V 虚拟化系列 ---
            "vethernet",    // ✅ 完美覆盖 vEthernet (nat), vEthernet (WSL), vEthernet (Default Switch)
            "hyper-v",      // ✅ 覆盖 Hyper-V Virtual Ethernet Adapter

            // --- VPN 与 隧道协议 ---
            "zerotier",
            "tap-windows",
            "vpn",
            "utun",
            "pseudo",
            "gif",
            "stf",
            "radmin",
            "hamachi",
            "tailscale",

            // --- 虚拟机 与 容器 ---
            "vmware",
            "virtualbox",
            "vbox",
            "docker",
            "wsl",          // 虽然 vEthernet 能覆盖一部分，但保留这个双重保险

            // --- 容器网络 (K8s/Docker/Linux) ---
            "cni",          // K8s 容器网络接口 (cni0)
            "flannel",      // Flannel 网络插件
            "calico",       // Calico 网络插件

            // --- 特定硬件与协议 ---
            "awdl",
            "llw",
            "feth",

            // --- 特殊的虚拟交换与热点 ---
            "default switch",
            "microsoft wi-fi direct virtual"
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
                    qDebugEx() << "过滤掉虚拟网卡:" << interface.humanReadableName();
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

        std::sort(ips.begin(), ips.end(), [](const QString &a, const QString &b) {
            auto getScore = [](const QString &ip) -> int {
                if (ip.startsWith("192.168.")) return 3; // 最高优先级：家用/办公路由
                if (ip.startsWith("10."))      return 2; // 次高优先级：企业内网
                if (ip.startsWith("172."))     return 1; // 低优先级：容易混淆容器网络
                return 0;                                // 其他 (如公网IP)
            };

            int scoreA = getScore(a);
            int scoreB = getScore(b);

            // 如果分数不同，分数高的排前面
            if (scoreA != scoreB)
                return scoreA > scoreB;

            return a < b;
        });

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
