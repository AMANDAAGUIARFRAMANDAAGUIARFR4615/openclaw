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

    static bool isCurrentNetworkAllowed()
    {
    #ifdef Q_OS_WIN
        HRESULT hr = S_OK;
        INetFwPolicy2 *pNetFwPolicy2 = nullptr;
        INetFwRules *pFwRules = nullptr;
        IUnknown *pEnumerator = nullptr;
        IEnumVARIANT *pVariant = nullptr;
        
        bool hasAllowRule = false;
        bool hasBlockRule = false;
        long currentProfileMask = 0; // 用于存储当前激活的网络类型（公用/专用/域）

        hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return false;
        }

        hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, 
                            __uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);

        if (SUCCEEDED(hr)) {
            // 获取当前激活的网络配置文件类型，比如 NET_FW_PROFILE2_PUBLIC 或 NET_FW_PROFILE2_PRIVATE
            hr = pNetFwPolicy2->get_CurrentProfileTypes(&currentProfileMask);
        }

        if (SUCCEEDED(hr)) {
            // --- 步骤 A: 检查当前网络类型的防火墙总开关 ---
            // 我们只关心当前网络类型的防火墙是否开启
            bool isFirewallOnForCurrent = false;
            VARIANT_BOOL fwEnabled;

            // 检查特定 Profile 是否在当前掩码中，且是否开启了防火墙
            auto checkProfile = [&](NET_FW_PROFILE_TYPE2 type) {
                if (currentProfileMask & type) {
                    if (SUCCEEDED(pNetFwPolicy2->get_FirewallEnabled(type, &fwEnabled)) && fwEnabled == VARIANT_TRUE) {
                        isFirewallOnForCurrent = true;
                    }
                }
            };

            checkProfile(NET_FW_PROFILE2_DOMAIN);
            checkProfile(NET_FW_PROFILE2_PRIVATE);
            checkProfile(NET_FW_PROFILE2_PUBLIC);

            // 如果当前网络环境的防火墙压根没开，那就直接允许
            if (!isFirewallOnForCurrent) {
                pNetFwPolicy2->Release();
                CoUninitialize();
                return true; 
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

            // --- 步骤 C: 遍历所有规则 ---
            while (pVariant->Next(1, &var, &cFetched) == S_OK) {
                INetFwRule *pFwRule = (INetFwRule*)var.pdispVal;
                BSTR bstrAppPath = nullptr;

                if (SUCCEEDED(pFwRule->get_ApplicationName(&bstrAppPath)) && bstrAppPath != nullptr) {
                    QString ruleAppPath = QString::fromWCharArray(bstrAppPath).toLower();
                    SysFreeString(bstrAppPath);

                    if (ruleAppPath == currentAppPath) {
                        VARIANT_BOOL bEnabled;
                        pFwRule->get_Enabled(&bEnabled);

                        if (bEnabled == VARIANT_TRUE) {
                            NET_FW_ACTION action;
                            long ruleProfiles = 0;
                            pFwRule->get_Action(&action);
                            pFwRule->get_Profiles(&ruleProfiles);

                            // 检查这条规则适用的网络 (ruleProfiles) 是否包含当前的网络环境 (currentProfileMask)
                            // 只有当规则适用于“当前环境”时，我们才关心它是 Allow 还是 Block
                            if (ruleProfiles & currentProfileMask) {
                                if (action == NET_FW_ACTION_BLOCK) {
                                    hasBlockRule = true;
                                    VariantClear(&var);
                                    break; // 发现针对当前网络的阻止规则，直接结束
                                } else if (action == NET_FW_ACTION_ALLOW) {
                                    hasAllowRule = true;
                                }
                            }
                        }
                    }
                }
                VariantClear(&var);
            }
            
            pVariant->Release();
            pEnumerator->Release();
        }

        if (pFwRules) pFwRules->Release();
        if (pNetFwPolicy2) pNetFwPolicy2->Release();
        CoUninitialize();

        // --- 步骤 D: 最终判定 ---
        if (hasBlockRule) return false;
        return hasAllowRule; 
    #else
        return true;
    #endif
    }
};
