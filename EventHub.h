#pragma once
#include <QMap>
#include <QList>
#include <QJsonValue>
#include <functional>
#include <QDebug>

class DeviceConnection;

class EventHub {
public:
    using Callback = std::function<void(const QJsonValue&, DeviceConnection*)>;
    using Handle   = std::size_t;

    static Handle on(const QString& event, Callback cb, int priority = 0);
    static bool   off(const QString& event, Handle h);
    static void   off(const QString& event);
    static void   trigger(const QString& event, const QJsonValue& data = {}, DeviceConnection* conn = nullptr);
    static Handle once(const QString& event, Callback cb, int priority = 0);

private:
    struct Item {
        Handle   id;
        Callback cb;
        int      priority;
        bool     once = false;
    };

    static QMap<QString, QList<Item>> events;
    static Handle nextId;
};