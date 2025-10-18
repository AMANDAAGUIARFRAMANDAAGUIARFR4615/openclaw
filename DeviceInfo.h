#pragma once

#include <QJsonObject>
#include <QString>
#include <QGuiApplication>
#include <QScreen>

class DeviceInfo {
public:
    DeviceInfo(const QJsonObject &json)
        : deviceId(json["deviceId"].toString()),
          deviceName(json["deviceName"].toString()),
          videoPort(json["videoPort"].toInt()),
          jbType(json["jbType"].toInt()),
          localIp(json["localIp"].toString()),
          orientation(json["orientation"].toInt()),
          platform(json["platform"].toString()),
          screenWidth(json["screenWidth"].toInt()),
          screenHeight(json["screenHeight"].toInt()),
          lockedStatus(json["lockedStatus"].toBool()),
          version(json["version"].toString()) {

        auto screenSize = QGuiApplication::primaryScreen()->size();
        auto maxWidth = screenSize.width() * 0.8;
        auto maxHeight = screenSize.height() * 0.8;

        if (screenWidth > maxWidth || screenHeight > maxHeight) {
            // 选择最小的缩放比例，保证宽高都不会超过屏幕
            scaleFactor = std::min(maxWidth / screenWidth, maxHeight / screenHeight);
        }
    }

    const QString deviceId;
    const QString deviceName;
    const int videoPort;
    const int jbType;
    const QString localIp;
    const QString platform;
    const int screenWidth;
    const int screenHeight;
    const QString version;

    int orientation;
    bool lockedStatus;
    float scaleFactor = 1;

    QString toString() const {
        return QString("deviceId: %1, deviceName: %2, videoPort: %3, jbType: %4, "
                       "localIp: %5, orientation: %6, platform: %7, screenWidth: %8, "
                       "screenHeight: %9, scaleFactor: %10, lockedStatus: %11, version: %12")
            .arg(deviceId)
            .arg(deviceName)
            .arg(videoPort)
            .arg(jbType)
            .arg(localIp)
            .arg(orientation)
            .arg(platform)
            .arg(screenWidth)
            .arg(screenHeight)
            .arg(scaleFactor)
            .arg(lockedStatus)
            .arg(version);
    }

    friend QDebug operator<<(QDebug dbg, const DeviceInfo* deviceInfo)
    {
        if (deviceInfo)
            dbg << QString("DeviceInfo(%1)").arg(deviceInfo->toString());
        else
            dbg << "DeviceInfo(nullptr)";

        return dbg;
    }
};
