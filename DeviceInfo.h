#pragma once

#include "global.h"
#include "SafeObject.h"
#include <QJsonObject>
#include <QString>
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
        version(json["version"].toString()) {

        groupMask = settings->value(deviceId + "/groupMask", 0u).toUInt();
        geometry = settings->value(deviceId + "/geometry").toRect();

        allDevices.append(this);
        devices.insert(deviceId, this);
        devices.insert(localIp, this);
    }
    
    ~DeviceInfo() {
        allDevices.removeOne(this);

        if (devices.value(deviceId) == this)
            devices.remove(deviceId);

        if (devices.value(localIp) == this)
            devices.remove(localIp);
    }

    static DeviceInfo* getDevice(const QString& id)
    {
        return devices[id];
    }

    static QList<DeviceInfo*> getDevices(quint32 mask)
    {
        if (!mask)
            return allDevices;

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

    int orientation;
    bool lockedStatus;
    quint32 groupMask = 0;
    QRect geometry;
    SafeObject<qint64> expireAt;

    inline static QHash<QString, SafeObject<qint64>> expirations;

private:
    inline static QList<DeviceInfo*> allDevices;
    inline static QHash<QString, DeviceInfo*> devices;
};
