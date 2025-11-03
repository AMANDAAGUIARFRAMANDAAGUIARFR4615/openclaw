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
    static bool isPrivateIPv4(const QString &ip) {
        // 判断是否为内网 IPv4（10.x.x.x, 172.16.0.0 - 172.31.255.255, 192.168.x.x）
        static QRegularExpression rePrivate(R"(^\s*(10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2\d|3[0-1])\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3})\s*$)");
        return rePrivate.match(ip).hasMatch();
    }

    // 获取本机内网IP地址
    static QString getLocalIP()
    {
        if (QOperatingSystemVersion::current().type() == QOperatingSystemVersion::MacOS) {
            QProcess process;
            process.start("networksetup", QStringList() << "-getinfo" << "Wi-Fi");
            process.waitForFinished();

            auto output = process.readAllStandardOutput();
            auto lines = output.split('\n');

            // 查找IP地址所在行
            for (const auto &line : lines) {
                if (line.startsWith("IP address:")) {
                    qDebugEx() << "networksetup" << line;
                    return line.mid(12).trimmed();
                }
            }
        }
        else
        {
            QProcess process;
            process.start("ipconfig");
            process.waitForFinished(3000);

            QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
            
            // 按顶格空行分块，每个块对应一个网卡
            QStringList blocks = output.split(QRegularExpression("(?m)^\\s*$"), Qt::SkipEmptyParts);

            // IPv4 地址正则
            QRegularExpression reIPv4(R"((?:IPv4 地址|IPv4 Address).*?:\s*([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+))",
                                    QRegularExpression::CaseInsensitiveOption);

            // IPv4 网关正则
            QRegularExpression reIPv4Addr(R"(([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+))");

            for (const QString &block : blocks) {
                QRegularExpressionMatch matchIPv4 = reIPv4.match(block);
                if (!matchIPv4.hasMatch())
                    continue; // 没有 IPv4 地址就跳过

                QString ip = matchIPv4.captured(1);
                QString gateway;

                QStringList lines = block.split(QRegularExpression("\\r?\\n"));
                bool gwSection = false;
                for (const QString &line : lines) {
                    if (!gwSection) {
                        // 检查本行是否是“默认网关”行
                        if (line.contains("Default Gateway") || line.contains("默认网关")) {
                            gwSection = true;
                            // 本行是否直接有 IPv4
                            QRegularExpressionMatch m = reIPv4Addr.match(line);
                            if (m.hasMatch()) {
                                gateway = m.captured(1);
                                break;
                            }
                        }
                    } else {
                        // 下一行缩进的内容可能是 IPv4
                        if (line.trimmed().isEmpty())
                            break; // 空行就结束
                        QRegularExpressionMatch m = reIPv4Addr.match(line);
                        if (m.hasMatch()) {
                            gateway = m.captured(1);
                            break;
                        }
                    }
                }

                if (!gateway.isEmpty())
                    return ip;
            }
        }

        return QString();
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
