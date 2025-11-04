#include "EventHub.h"
#include <algorithm>

QMap<QString, QList<EventHub::Item>> EventHub::events;
EventHub::Handle EventHub::nextId = 0;

EventHub::Handle EventHub::on(const QString& event, Callback cb, int priority) {
    auto& list = events[event];
    Handle id = ++nextId;
    list.append({id, std::move(cb), priority, false});
    std::sort(list.begin(), list.end(), [](const Item& a, const Item& b) {
        return a.priority > b.priority;
    });
    return id;
}

EventHub::Handle EventHub::once(const QString& event, Callback cb, int priority) {
    Handle id = ++nextId;
    Callback wrapper = [event, id, cb = std::move(cb)](const QJsonValue& d, DeviceConnection* c) mutable {
        cb(d, c);
        off(event, id);
    };
    auto& list = events[event];
    list.append({id, std::move(wrapper), priority, true});
    std::sort(list.begin(), list.end(), [](const Item& a, const Item& b) {
        return a.priority > b.priority;
    });
    return id;
}

bool EventHub::off(const QString& event, Handle h) {
    auto it = events.find(event);
    if (it == events.end()) return false;
    auto& list = *it;
    auto pos = std::find_if(list.begin(), list.end(), [h](const Item& i){ return i.id == h; });
    if (pos == list.end()) return false;
    list.erase(pos);
    if (list.isEmpty()) events.remove(event);
    return true;
}

void EventHub::off(const QString& event) {
    events.remove(event);
}

void EventHub::trigger(const QString& event, const QJsonValue& data, DeviceConnection* conn) {
    auto it = events.find(event);
    if (it == events.end()) return;
    QList<Item> copy = *it;
    for (const Item& item : copy) {
        if (events.contains(event) &&
            std::any_of(events[event].cbegin(), events[event].cend(),
                        [item](const Item& i){ return i.id == item.id; })) {
            try {
                item.cb(data, conn);
            } catch (...) {
                qCritical() << "EventHub error in" << event;
            }
            if (item.once) off(event, item.id);
        }
    }
}