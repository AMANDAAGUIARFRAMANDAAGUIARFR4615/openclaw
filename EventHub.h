#pragma once
#include <QMap>
#include <QList>
#include <QJsonValue>
#include <functional>
#include <QDebug>
#include <QObject>

class DeviceConnection;

class EventHub {
public:
    using Callback = std::function<void(const QJsonValue&, DeviceConnection*)>;

    static void on(QObject* owner, const QString& event, Callback cb);
    static void once(QObject* owner, const QString& event, Callback cb);
    static void off(QObject* owner, const QString& event);
    static void off(const QString& event);
    static void trigger(const QString& event, const QJsonValue& data = {}, DeviceConnection* conn = nullptr);

private:
    struct Item {
        QObject* owner = nullptr;
        Callback cb;
        bool     once = false;
    };

    static QMap<QString, QList<Item>> events;
};
