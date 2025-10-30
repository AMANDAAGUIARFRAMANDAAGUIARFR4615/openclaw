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
      model(json["model"].toString()),
      systemVersion(json["systemVersion"].toString()),
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
