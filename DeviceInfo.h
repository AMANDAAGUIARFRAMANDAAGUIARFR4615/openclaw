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
        jbType(json["jbType"].toInt()),
        localIp(json["localIp"].toString()),
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
    static double getOptimalAspectRatio(QList<DeviceInfo*> devices)
    {
        if (devices.isEmpty())
            return 1;

        QList<double> deviceRatios;
        QSet<double> candidateRatios;

        for (auto device : devices) {
            // 无论设备当前也是横是竖，长边都算作 Height，短边算作 Width
            double h = std::max(device->screenWidth, device->screenHeight);
            double w = std::min(device->screenWidth, device->screenHeight);
            
            double r = h / w; // 这里得到的结果通常 > 1.0
            
            deviceRatios.append(r);
            candidateRatios.insert(r);
        }

        double bestRatio = 0.0;
        double maxTotalFillRate = -1.0;

        // 遍历所有存在的比例作为“标准框” (CandidateR)
        for (double candidateR : candidateRatios) {
            double currentTotalFillRate = 0.0;

            for (double devR : deviceRatios) {
                // 计算填充率 (Fill Rate)
                // candidateR 是框的高宽比，devR 是设备的高宽比
                
                double fillRate;
                
                if (devR > candidateR) {
                    // 设备比框更“细长” (更直)
                    fillRate = candidateR / devR; 
                } else {
                    // 设备比框更“矮胖” (devR < candidateR)
                    fillRate = devR / candidateR;
                }

                currentTotalFillRate += fillRate;
            }

            if (currentTotalFillRate > maxTotalFillRate) {
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

private:
    inline static QList<DeviceInfo*> allDevices;
    inline static QHash<QString, DeviceInfo*> devices;
};
