#pragma once

#include <QJsonObject>
#include <QString>
#include <QGuiApplication>
#include <QScreen>
#include <QList>
#include <QDebug>

class DeviceInfo {
public:
    explicit DeviceInfo(const QJsonObject &json);
    ~DeviceInfo();

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

    QString toString() const;
    QString uniqueName() const;

    friend QDebug operator<<(QDebug dbg, const DeviceInfo* deviceInfo);

private:
    static QList<DeviceInfo*> allDevices;
};
