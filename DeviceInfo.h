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
            
        auto screenSize = qApp->primaryScreen()->size();
        auto maxWidth = screenSize.width() * 0.8;
        auto maxHeight = screenSize.height() * 0.8;

        if (screenWidth > maxWidth || screenHeight > maxHeight) {
            scaleFactor = qMin(maxWidth / static_cast<float>(screenWidth),
                                maxHeight / static_cast<float>(screenHeight));
        }

        groupMask = settings.value(deviceId + "/groupMask", 0u).toUInt();

        allDevices.append(this);
        devices.insert(deviceId, this);
    }
    
    ~DeviceInfo() {
        allDevices.removeOne(this);
        devices.remove(deviceId);
    }

    static DeviceInfo* getDevice(const QString& udid)
    {
        return devices[udid];
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
    SafeObject<QDateTime> expireAt;

private:
    inline static QList<DeviceInfo*> allDevices;
    inline static QHash<QString, DeviceInfo*> devices;
};
