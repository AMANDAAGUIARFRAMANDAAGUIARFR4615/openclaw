#pragma once

#include "global.h"
#include "SafeObject.h"
#include "Account.h"
#include "EventHub.h"
#include "DeviceConnection.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVersionNumber>
#include <QApplication>
#include <QScreen>
#include <QList>

class DeviceConnection;

class DeviceInfo {
public:
    explicit DeviceInfo(DeviceConnection* const connection, const QJsonObject &json)
        : connection(connection),
        deviceId(json["deviceId"].toString()),
        deviceName(json["deviceName"].toString()),
        videoPort(json["videoPort"].toInt()),
        jbType(json["jbType"].toInt()),//有根/无根/隐根
        localIp([](const QString &ip) -> QString {
            QHostAddress address;
            if (address.setAddress(ip))
                return ip;

            return QString();
        }(json["localIp"].toString())),
        orientation(json["orientation"].toInt()),
        model(json["model"].toString()),
        systemVersion(json["systemVersion"].toString()),
        screenWidth(json["screenWidth"].toInt()),
        screenHeight(json["screenHeight"].toInt()),
        lockedStatus(json["lockedStatus"].toBool()),
        version(json["version"].toString()),
        scanType(json["scanType"].toBool()) {

        groupMask = settings->value(deviceId + "/groupMask", 1u).toUInt();
        geometry = settings->value(deviceId + "/geometry").toRect();
        controller = settings->value(deviceId + "/controller", true).toBool();

        settings->setValue(deviceId + "/deviceName", deviceName);

        allDevices.append(this);
        devices.insert(deviceId, this);

        locker = lockers.value(deviceId);

        if (!localIp.isEmpty()) {
            devices.insert(localIp, this);
            ips.insert(deviceId, localIp);

            if (!locker.isEmpty() && lockers.value(localIp).isEmpty())
                lockers.insert(localIp, locker);
        }
    }

    ~DeviceInfo() {
        allDevices.removeOne(this);

        if (devices.value(deviceId) == this)
            devices.remove(deviceId);

        if (devices.value(localIp) == this)
            devices.remove(localIp);
    }

    QString displayName(bool richText = false) {
        const auto& palette = qApp->palette();

        const auto& accentColor = palette.color(QPalette::WindowText).name();

        auto shadowQColor = palette.color(QPalette::WindowText);
        shadowQColor.setAlpha(150);
        const auto& shadowColor = shadowQColor.name(QColor::HexArgb);

        const auto& connType = connection->type == DeviceConnection::Usb ? "USB" : "WiFi";
        
        if (richText)
            return QString("<a href='#' style='text-decoration:none; color:%1;'><b>%2</b>✏️</a> <font color='%3'>[%4]</font>").arg(accentColor, deviceName, shadowColor, connType);
    
        return QString("%1 [%2]").arg(deviceName, connType);
    }

    bool hasLocker()
    {
        return !locker.isEmpty();
    }

    const QString& getLocker()
    {
        return locker;
    }

    void setLocker(const QString& value)
    {
        locker = value;

        lockers.insert(deviceId, value);
        lockers.insert(localIp, value);

        if (isLockByOther()) {
            sendExclusiveReject(connection, locker, version);
            EventHub::trigger("disconnected", QJsonValue(), connection);
        }
    }

    static void setLocker(const QString& udid, const QString& value)
    {
        const auto& deviceInfo = getDevice(udid);
        if (deviceInfo) {
            deviceInfo->setLocker(value);
            return;
        }

        lockers.insert(udid, value);

        const auto& ip = ips.value(udid);
        if (!ip.isEmpty())
            lockers.insert(ip, value);
    }

    bool isLockByOther()
    {
        return isLockByOther(deviceId);
    }

    static bool isLockByOther(const QString& id)
    {
        const auto locker = lockers.value(id);
        return !locker.isEmpty() && locker != Account::getInstance()->phone;
    }

    static bool supportsExclusiveRejectDialog(const QString& version)
    {
        return QVersionNumber::fromString(version) > QVersionNumber(2, 6, 5);
    }

    static QString exclusiveRejectMessage(const QString& locker)
    {
        return QStringLiteral("此设备被【%1】独占，需要该账号退出独占模式您才能连接").arg(locker);
    }

    static bool sendExclusiveReject(DeviceConnection* connection, const QString& locker, const QString& version)
    {
        if (supportsExclusiveRejectDialog(version)) {
            connection->send("dialog", buildExclusiveRejectDialogData(locker));
            return true;
        }

        connection->send("reject", exclusiveRejectMessage(locker));
        return false;
    }

    static QJsonObject buildExclusiveRejectDialogData(const QString& locker)
    {
        return QJsonObject{
            {"id", QStringLiteral("exclusiveLock")},
            {"title", QStringLiteral("无法连接")},
            {"message", QStringLiteral("此设备被【%1】独占，需要该账号退出独占模式您才能连接").arg(locker)},
            {"options", QJsonArray{
                QStringLiteral("知道了"),
                QStringLiteral("退出独占")
            }}
        };
    }

    static DeviceInfo* getDevice(const QString& id)
    {
        return devices.value(id);
    }

    static QList<DeviceInfo*> getDevices(quint32 mask)
    {
        QList<DeviceInfo*> result;
        for (const auto& deviceInfo : std::as_const(allDevices)) {
            if (deviceInfo->groupMask & mask)
                result.append(deviceInfo);
        }

        return result;
    }

    /**
     * @brief 获取最佳显示宽高比 (Height / Width)
     */
    static float getOptimalAspectRatio(const QList<DeviceInfo*>& devices)
    {
        if (devices.isEmpty())
            return 1.0;

        QList<float> allRatios;
        allRatios.reserve(devices.size());

        for (auto device : devices) {
            allRatios.append((float)device->screenHeight / device->screenWidth);
        }

        std::sort(allRatios.begin(), allRatios.end());

        float bestRatio = allRatios.first();
        float maxTotalFillRate = -1.0;

        for (int i = 0; i < allRatios.size(); ++i) {
            float candidateR = allRatios[i];

            // 如果当前 candidate 和上一个非常接近，直接跳过
            // 避免因为精度问题重复计算 (例如 1.7777777 和 1.7777778)
            if (i > 0 && qAbs(candidateR - allRatios[i-1]) < 0.0001)
                continue;

            float currentTotalFillRate = 0.0;

            // 内层循环：计算该 candidateR 适应所有设备时的总填充率
            for (float devR : allRatios) {
                if (devR > candidateR)
                    currentTotalFillRate += candidateR / devR; // 框比设备“胖”，设备高度受限
                else
                    currentTotalFillRate += devR / candidateR; // 框比设备“瘦”，设备宽度受限
            }

            if (currentTotalFillRate >= maxTotalFillRate) {
                maxTotalFillRate = currentTotalFillRate;
                bestRatio = candidateR;
            }
        }

        return bestRatio;
    }

    DeviceConnection* const connection;

    const QString deviceId;
    QString deviceName;
    const int videoPort;
    const int jbType;
    const QString localIp;
    const QString model;
    const QString systemVersion;
    const int screenWidth;
    const int screenHeight;
    const QString version;
    const int scanType;

    int orientation;
    bool lockedStatus;
    quint32 groupMask;
    QRect geometry;
    bool controller;
    QString clipboardText;
    int randomDelay = 0;
    SafeObject<qint64> expireAt;

    inline static QHash<QString, SafeObject<qint64>> expirations;
    inline static QHash<QString, quint16> remotePorts;

private:
    QString locker;

    inline static QList<DeviceInfo*> allDevices;
    inline static QHash<QString, DeviceInfo*> devices;
    inline static QHash<QString, QString> lockers;
    inline static QHash<QString, QString> ips;
};
