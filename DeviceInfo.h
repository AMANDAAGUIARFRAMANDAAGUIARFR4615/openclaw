#pragma once

#include "global.h"
#include <QJsonObject>
#include <QString>
#include <QGuiApplication>
#include <QScreen>
#include <QList>

class DeviceInfo {
public:
    explicit DeviceInfo(const QJsonObject &json)
      : deviceId(json["deviceId"].toString()),
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
            
        auto screenSize = QGuiApplication::primaryScreen()->size();
        auto maxWidth = screenSize.width() * 0.8;
        auto maxHeight = screenSize.height() * 0.8;

        if (screenWidth > maxWidth || screenHeight > maxHeight) {
            scaleFactor = std::min(maxWidth / static_cast<float>(screenWidth),
                                maxHeight / static_cast<float>(screenHeight));
        }

        const QString key = QStringLiteral("groupMask_%1").arg(deviceId);
        groupMask = settings.value(key, 0u).toUInt();

        allDevices.append(this);
    }
    
    ~DeviceInfo() {
        allDevices.removeOne(this);
    }

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
    static QList<DeviceInfo*> allDevices;
};
