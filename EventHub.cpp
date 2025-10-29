#include "EventHub.h"

QMap<QString, QList<std::pair<std::function<void(QJsonValue, DeviceConnection*)>, int>>> EventHub::listeners;

void EventHub::StartListening(const QString& eventName, std::function<void(QJsonValue, DeviceConnection*)> listener, int priority)
{
    listeners[eventName].emplace_back(listener, priority);

    auto& actions = listeners[eventName];
    std::sort(actions.begin(), actions.end(), [](const auto& a, const auto& b)
    {
        return a.second > b.second;
    });
}

void EventHub::StopListening(const QString& eventName, std::function<void(QJsonValue, DeviceConnection*)> listener)
{
    if (listeners.contains(eventName))
    {
        auto& actions = listeners[eventName];

        if (!listener)
        {
            actions.clear();
        }
        else
        {
            actions.erase(
                std::remove_if(actions.begin(), actions.end(), 
                               [&listener](const auto& pair) { return &pair.first == &listener; }),
                actions.end());
        }
    }
}

void EventHub::TriggerEvent(const QString& eventName, const QJsonValue& data, DeviceConnection* connection)
{
    if (listeners.contains(eventName))
    {
        for (const auto& action : listeners[eventName])
        {
            action.first(data, connection);
        }
    }
}
