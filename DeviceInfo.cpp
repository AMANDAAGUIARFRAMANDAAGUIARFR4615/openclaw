#include "DeviceInfo.h"
#include <algorithm>

QList<DeviceInfo*> DeviceInfo::allDevices;

DeviceInfo::DeviceInfo(const QJsonObject &json)
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
      version(json["version"].toString())
{
    auto screenSize = QGuiApplication::primaryScreen()->size();
    auto maxWidth = screenSize.width() * 0.8;
    auto maxHeight = screenSize.height() * 0.8;

    if (screenWidth > maxWidth || screenHeight > maxHeight) {
        scaleFactor = std::min(maxWidth / static_cast<float>(screenWidth),
                               maxHeight / static_cast<float>(screenHeight));
    }

    allDevices.append(this);
}

DeviceInfo::~DeviceInfo() {
    allDevices.removeOne(this);
}

QString DeviceInfo::toString() const {
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

QString DeviceInfo::uniqueName() const {
    int sameCount = 0;
    for (auto dev : allDevices) {
        if (dev->deviceName == deviceName)
            sameCount++;
    }

    if (sameCount == 1)
        return deviceName;

    return QString("%1_%2").arg(deviceName, deviceId);
}

QDebug operator<<(QDebug dbg, const DeviceInfo* deviceInfo)
{
    if (deviceInfo)
        dbg << QString("DeviceInfo(%1)").arg(deviceInfo->toString());
    else
        dbg << "DeviceInfo(nullptr)";

    return dbg;
}
