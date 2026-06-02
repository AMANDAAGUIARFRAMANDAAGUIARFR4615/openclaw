#pragma once

#include "Logger.h"
#include "Safe.h"
#include <QHash>
#include <QList>
#include <QJsonValue>
#include <functional>

class DeviceConnection;

class EventHub {
public:
    using Callback = std::function<void(const QJsonValue&, DeviceConnection*)>;

    static void on(QObject* listener, const DataGuard::StrObfuscator<>& event, Callback cb) {
        if (!listener) {
            qCriticalEx() << "EventHub::on - listener is null for event";
            return;
        }

        const QString eventKey = QString::fromUtf8(event.decrypt());
        auto& list = events[eventKey];

        auto exists = std::any_of(list.cbegin(), list.cend(), [listener](const Item& i){ return i.listener == listener; });
        if (exists) {
            qCriticalEx() << "EventHub::on - listener already registered for event";
            return;
        }

        list.append({listener, std::move(cb), false});
    }

    static void once(QObject* listener, const DataGuard::StrObfuscator<>& event, Callback cb) {
        if (!listener) {
            qCriticalEx() << "EventHub::once - listener is null for event";
            return;
        }

        auto eventCopy = event;
        const QString eventKey = QString::fromUtf8(event.decrypt());
        auto& list = events[eventKey];

        auto exists = std::any_of(list.cbegin(), list.cend(), [listener](const Item& i){ return i.listener == listener; });
        if (exists) {
            qCriticalEx() << "EventHub::once - listener already registered for event";
            return;
        }

        // 包装回调，使其在执行后自行解绑
        QObject* ownerCopy = listener;
        Callback wrapper = [cb = std::move(cb), ownerCopy, eventCopy](const QJsonValue& d, DeviceConnection* c) mutable {
            cb(d, c);
            off(ownerCopy, eventCopy);
        };

        list.append({listener, std::move(wrapper), true});
    }

    static void off(QObject* listener, const DataGuard::StrObfuscator<>& event) {
        if (!listener) return;

        const QString eventKey = QString::fromUtf8(event.decrypt());
        auto it = events.find(eventKey);
        if (it == events.end()) return;

        auto& list = *it;
        list.erase(std::remove_if(list.begin(), list.end(),
                                [listener](const Item& i){ return i.listener == listener; }),
                list.end());
        if (list.isEmpty()) events.remove(eventKey);
    }

    static void off(const DataGuard::StrObfuscator<>& event) {
        events.remove(QString::fromUtf8(event.decrypt()));
    }

    static void trigger(const DataGuard::StrObfuscator<>& event, const QJsonValue& data = {}, DeviceConnection* conn = nullptr) {
        dispatch(QString::fromUtf8(event.decrypt()), data, conn);
    }

    static void dispatch(const QString& eventKey, const QJsonValue& data, DeviceConnection* conn) {
        auto it = events.find(eventKey);
        if (it == events.end()) return;

        QList<Item> copy = *it;
        for (const Item& item : copy) {
            try {
                item.cb(data, conn);
            } catch (...) {
                qCriticalEx() << "EventHub error in" << eventKey;
            }
        }
    }

private:
    struct Item {
        QObject* listener = nullptr;
        Callback cb;
        bool     once = false;
    };

    inline static QHash<QString, QList<Item>> events;
};
