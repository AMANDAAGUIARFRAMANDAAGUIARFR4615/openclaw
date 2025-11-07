#include "EventHub.h"
#include "Logger.h"
#include <algorithm>

QMap<QString, QList<EventHub::Item>> EventHub::events;

void EventHub::on(QObject* owner, const QString& event, Callback cb) {
    if (!owner) {
        qCriticalEx() << "EventHub::on - owner is null for event" << event;
        return;
    }

    auto& list = events[event];

    auto exists = std::any_of(list.cbegin(), list.cend(), [owner](const Item& i){ return i.owner == owner; });
    if (exists) {
        qCriticalEx() << "EventHub::on - owner already registered for event" << event;
        return;
    }

    list.append({owner, std::move(cb), false});
}

void EventHub::once(QObject* owner, const QString& event, Callback cb) {
    if (!owner) {
        qCriticalEx() << "EventHub::once - owner is null for event" << event;
        return;
    }

    auto& list = events[event];

    auto exists = std::any_of(list.cbegin(), list.cend(), [owner](const Item& i){ return i.owner == owner; });
    if (exists) {
        qCriticalEx() << "EventHub::once - owner already registered for event" << event;
        return;
    }

    // 包装回调，使其在执行后自行解绑
    QObject* ownerCopy = owner;
    Callback wrapper = [cb = std::move(cb), ownerCopy, event](const QJsonValue& d, DeviceConnection* c) mutable {
        cb(d, c);
        EventHub::off(ownerCopy, event);
    };

    list.append({owner, std::move(wrapper), true});
}

void EventHub::off(QObject* owner, const QString& event) {
    if (!owner) return;
    auto it = events.find(event);
    if (it == events.end()) return;

    auto& list = *it;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [owner](const Item& i){ return i.owner == owner; }),
               list.end());
    if (list.isEmpty()) events.remove(event);
}

void EventHub::off(const QString& event) {
    events.remove(event);
}

void EventHub::trigger(const QString& event, const QJsonValue& data, DeviceConnection* conn) {
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
