#pragma once
#include <functional>
#include <map>
#include <memory>
#include <typeindex>

#define EVENT_BIND(eventType, handler) \
IllegalEventManager::getInstance()->registerListener<eventType>( \
[this](std::shared_ptr<eventType> event) { this->handler(event); })

#define EVENT_TRIGGER(eventType, eventData) \
IllegalEventManager::getInstance()->triggerEvent<eventType>(eventData)


struct Event;

class IllegalEventManager
{
public:
    using EventCallback = std::function<void(const std::shared_ptr<Event>&)>;
    static IllegalEventManager* getInstance();

protected:
    static IllegalEventManager* _instance;

public:
    
    template <typename EventType>
    void registerListener(std::function<void(std::shared_ptr<EventType>)> listener)
    {
        {
            std::function<void(std::shared_ptr<Event>)> wrapper = [listener](std::shared_ptr<Event> data)
            {
                listener(std::static_pointer_cast<EventType>(data));
            };
            listeners[typeid(EventType)].push_back(wrapper);
        }
    }

    template <typename EventType>
    void triggerEvent(std::shared_ptr<EventType> event)
    {
        auto eventTypeIndex = std::type_index(typeid(EventType));

        if (listeners.find(eventTypeIndex) != listeners.end())
        {
            for (const auto& listener : listeners[eventTypeIndex])
            {
                listener(event);
            }
        }
    }

private:
    std::map<std::type_index, std::vector<EventCallback>> listeners;
    IllegalEventManager();
    IllegalEventManager(const IllegalEventManager&)            = delete;
    IllegalEventManager& operator=(const IllegalEventManager&) = delete;
};
