#pragma once

#include "DeviceConnection.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMap>
#include <QList>
#include <functional>
#include <QString>
#include <memory>
#include <algorithm>

class EventHub
{
public:
    static void StartListening(const QString& eventName, std::function<void(QJsonValue, DeviceConnection*)> listener, int priority = 0);
    static void StopListening(const QString& eventName, std::function<void(QJsonValue, DeviceConnection*)> listener = nullptr);
    static void TriggerEvent(const QString& eventName, const QJsonValue& data = QJsonValue(), DeviceConnection* connection = nullptr);

private:
    static QMap<QString, QList<std::pair<std::function<void(QJsonValue, DeviceConnection*)>, int>>> listeners;
};
