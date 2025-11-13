#pragma once

#include "Logger.h"
#include <QMap>
#include <QList>
#include <QJsonValue>
#include <functional>

class DeviceConnection;

class EventHub {
public:
    using Callback = std::function<void(const QJsonValue&, DeviceConnection*)>;

    static void on(QObject* listener, const QString& event, Callback cb) {
        if (!listener) {
            qCriticalEx() << "EventHub::on - listener is null for event" << event;
            return;
        }

        auto& list = events[event];

        auto exists = std::any_of(list.cbegin(), list.cend(), [listener](const Item& i){ return i.listener == listener; });
        if (exists) {
            qCriticalEx() << "EventHub::on - listener already registered for event" << event;
            return;
        }

        list.append({listener, std::move(cb), false});
    }

    static void once(QObject* listener, const QString& event, Callback cb) {
        if (!listener) {
            qCriticalEx() << "EventHub::once - listener is null for event" << event;
            return;
        }

        auto& list = events[event];

        auto exists = std::any_of(list.cbegin(), list.cend(), [listener](const Item& i){ return i.listener == listener; });
        if (exists) {
            qCriticalEx() << "EventHub::once - listener already registered for event" << event;
            return;
        }

        // 包装回调，使其在执行后自行解绑
        QObject* ownerCopy = listener;
        Callback wrapper = [cb = std::move(cb), ownerCopy, event](const QJsonValue& d, DeviceConnection* c) mutable {
            cb(d, c);
            EventHub::off(ownerCopy, event);
        };

        list.append({listener, std::move(wrapper), true});
    }

    static void off(QObject* listener, const QString& event) {
        if (!listener) return;
        auto it = events.find(event);
        if (it == events.end()) return;

        auto& list = *it;
        list.erase(std::remove_if(list.begin(), list.end(),
                                [listener](const Item& i){ return i.listener == listener; }),
                list.end());
        if (list.isEmpty()) events.remove(event);
    }

    static void off(const QString& event) {
        events.remove(event);
    }

    static void trigger(const QString& event, const QJsonValue& data = {}, DeviceConnection* conn = nullptr) {
        auto it = events.find(event);
        if (it == events.end()) return;

        QList<Item> copy = *it;
        for (const Item& item : copy) {
            try {
                item.cb(data, conn);
            } catch (...) {
                qCriticalEx() << "EventHub error in" << event;
            }
        }
    }

private:
    struct Item {
        QObject* listener = nullptr;
        Callback cb;
        bool     once = false;
    };

    inline static QMap<QString, QList<Item>> events;
};
