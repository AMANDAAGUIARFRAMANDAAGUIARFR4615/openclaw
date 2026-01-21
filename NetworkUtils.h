#pragma once

#include "Logger.h"
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QString>
#include <QList>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <windows.h>
#include <netfw.h>
#include <objbase.h>
#include <comutil.h>
#endif

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

        for (const QNetworkInterface &networkInterface : QNetworkInterface::allInterfaces()) {
            if (!(networkInterface.flags() & QNetworkInterface::IsUp) || !(networkInterface.flags() & QNetworkInterface::IsRunning))
                continue;

            if (networkInterface.flags() & QNetworkInterface::IsLoopBack)
                continue;

            // 如果类型明确是 Wifi，绝对是物理网卡，跳过黑名单检查
            bool isWifi = (networkInterface.type() == QNetworkInterface::Wifi);
            
            if (!isWifi) {
                // 如果不是 Wifi (通常是 Ethernet)，则检查名字是否包含虚拟关键字
                if (isVirtualAdapter(networkInterface.humanReadableName(), networkInterface.name())) {
                    qDebugEx() << "过滤掉虚拟网卡:" << networkInterface.humanReadableName();
                    continue; 
                }
            }

            for (const QNetworkAddressEntry &entry : networkInterface.addressEntries()) {
                QHostAddress ip = entry.ip();

                if (ip.protocol() != QAbstractSocket::IPv4Protocol)
                    continue;

                QString ipStr = ip.toString();

                // 排除 169.254 (自动专用IP，无效)
                if (ipStr.startsWith("169.254"))
                    continue;

                if (ipStr.endsWith(".1"))
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

    static QString getSubnet(const QString& ip)
    {
        auto lastDotIndex = ip.lastIndexOf('.');
        return lastDotIndex != -1 ? ip.left(lastDotIndex + 1) + "0" : "";
    }
    
    // 遍历同一子网的IP地址，IP范围从 .1 到 .254
    static QList<QString> getSubnetIPs(const QString& localIP)
    {
        QList<QString> ipList;

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
                ipList.append(ip.toString());
        }

        return ipList;
    }

    static bool isFirewallPrivateAllowed()
    {
#ifdef Q_OS_WIN
        HRESULT hr = S_OK;
        INetFwPolicy2 *pNetFwPolicy2 = nullptr;
        INetFwRules *pFwRules = nullptr;
        IUnknown *pEnumerator = nullptr;
        IEnumVARIANT *pVariant = nullptr;
        
        bool hasAllowRule = false; // 是否发现了允许规则
        bool hasBlockRule = false; // 是否发现了阻止规则

        // 1. 初始化 COM
        hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return false;
        }

        // 2. 创建防火墙策略实例
        hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, 
                            __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);

        if (SUCCEEDED(hr)) {
            // --- 步骤 A: 检查防火墙总开关 ---
            // 如果专用网络(Private)的防火墙被彻底关闭了，那么程序自然是有权限的
            VARIANT_BOOL fwEnabled;
            if (SUCCEEDED(pNetFwPolicy2->get_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, &fwEnabled))) {
                if (fwEnabled == VARIANT_FALSE) {
                    pNetFwPolicy2->Release();
                    CoUninitialize();
                    return true; // 防火墙没开，直接允许
                }
            }

            // --- 步骤 B: 获取规则集合 ---
            hr = pNetFwPolicy2->get_Rules(&pFwRules);
        }

        if (SUCCEEDED(hr)) {
            hr = pFwRules->get__NewEnum(&pEnumerator);
            if (pEnumerator) {
                hr = pEnumerator->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pVariant);
            }
        }

        if (SUCCEEDED(hr) && pVariant) {
            QString currentAppPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toLower();
            
            ULONG cFetched = 0;
            VARIANT var;
            VariantInit(&var);

            // --- 步骤 C: 遍历所有规则 (核心逻辑修复) ---
            while (pVariant->Next(1, &var, &cFetched) == S_OK) {
                INetFwRule *pFwRule = (INetFwRule*)var.pdispVal;
                BSTR bstrAppPath = nullptr;

                // 获取规则关联的程序路径
                if (SUCCEEDED(pFwRule->get_ApplicationName(&bstrAppPath)) && bstrAppPath != nullptr) {
                    QString ruleAppPath = QString::fromWCharArray(bstrAppPath).toLower();
                    SysFreeString(bstrAppPath);

                    // 只有路径匹配才进行深入检查
                    if (ruleAppPath == currentAppPath) {
                        VARIANT_BOOL bEnabled;
                        pFwRule->get_Enabled(&bEnabled);

                        // 只有启用的规则才有效
                        if (bEnabled == VARIANT_TRUE) {
                            NET_FW_ACTION action;
                            long lProfiles = 0;
                            pFwRule->get_Action(&action);
                            pFwRule->get_Profiles(&lProfiles);

                            // 检查该规则是否适用于专用网络(Private)
                            // 注意：Profile 是位掩码，必须用 & 运算
                            if (lProfiles & NET_FW_PROFILE2_PRIVATE) {
                                if (action == NET_FW_ACTION_BLOCK) {
                                    // 发现了一条针对 Private 网络的【阻止】规则
                                    hasBlockRule = true;
                                    // 根据 Windows 逻辑，Block 优先级最高，一旦发现可以直接认定为不通
                                    // 但为了代码稳健，我们可以继续循环或者直接 break
                                    VariantClear(&var);
                                    break; 
                                } else if (action == NET_FW_ACTION_ALLOW) {
                                    // 发现了一条针对 Private 网络的【允许】规则
                                    hasAllowRule = true;
                                }
                            }
                        }
                    }
                }
                VariantClear(&var); // 释放当前项
            }
            
            pVariant->Release();
            pEnumerator->Release();
        }

        if (pFwRules) pFwRules->Release();
        if (pNetFwPolicy2) pNetFwPolicy2->Release();
        CoUninitialize();

        // --- 步骤 D: 最终判定 ---
        // 必须满足：有允许规则 且 没有阻止规则
        if (hasBlockRule)
            return false; // 被显式阻止

        return hasAllowRule; // 如果有允许规则则通过，否则(没弹窗/没规则)为不通
#else
        return true;
#endif
    }
};
