#pragma once

#include <QJsonObject>
#include <QString>
#include <QGuiApplication>
#include <QScreen>
#include <QList>

class DeviceInfo {
public:
    explicit DeviceInfo(const QJsonObject &json);
    ~DeviceInfo();

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

    QString uniqueName() const;

private:
    static QList<DeviceInfo*> allDevices;
};
