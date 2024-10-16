#pragma once
#include <functional>
#include <map>
#include <memory>
#include <typeindex>

#define EVENT_REGISTER(eventType, handler) \
EventManager::getInstance()->registerListener<eventType>( \
[this](std::shared_ptr<eventType> event) { this->handler(event); })

#define EVENT_TRIGGER(eventType, eventData) \
EventManager::getInstance()->triggerEvent<eventType>(eventData)


struct Event;

class EventManager
{
public:
    using EventCallback = std::function<void(const std::shared_ptr<Event>&)>;
    static EventManager* getInstance();

protected:
    static EventManager* _instance;

public:
    // 注册事件监听器
    template <typename EventType>
    void registerListener(std::function<void(std::shared_ptr<EventType>)> listener)
    {
        {
            // 包装为可以存储在通用容器中的函数
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
        // 使用 typeid 获取类型信息并传递给 type_index
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
    EventManager();
    // 禁用拷贝构造和赋值操作
    EventManager(const EventManager&)            = delete;
    EventManager& operator=(const EventManager&) = delete;
};
