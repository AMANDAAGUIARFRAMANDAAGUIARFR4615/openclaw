#pragma once

#include "global.h"
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
            
        auto screenSize = QApplication::primaryScreen()->size();
        auto maxWidth = screenSize.width() * 0.8;
        auto maxHeight = screenSize.height() * 0.8;

        if (screenWidth > maxWidth || screenHeight > maxHeight) {
            scaleFactor = qMin(maxWidth / static_cast<float>(screenWidth),
                                maxHeight / static_cast<float>(screenHeight));
        }

        groupMask = settings.value(deviceId + "/groupMask", 0u).toUInt();

        allDevices.append(this);
    }
    
    ~DeviceInfo() {
        allDevices.removeOne(this);
    }

    static QList<DeviceInfo*> getDevices(quint32 mask)
    {
        if (!mask)
            return allDevices;

        QList<DeviceInfo*> result;
        for (auto deviceInfo : allDevices) {
            if (deviceInfo->groupMask & mask)
                result.append(deviceInfo);
        }

        return result;
    }

    DeviceConnection* const connection;
    
    const QString deviceId;
    const QString deviceName;
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
    float scaleFactor = 1;
    quint32 groupMask = 0;

    QString uniqueName() const {
        int sameCount = 0;
        for (auto dev : allDevices) {
            if (dev->deviceName == deviceName)
                sameCount++;
        }

        if (sameCount == 1)
            return deviceName;

        return QString("%1_%2").arg(deviceName, deviceId);
    }

private:
    inline static QList<DeviceInfo*> allDevices;
};
