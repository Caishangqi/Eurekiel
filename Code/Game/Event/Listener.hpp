// Listener.h
#pragma once
#include "Event.hpp"
#include <typeinfo>

class Listener
{
protected:
    const std::type_info& eventType;

public:
    Listener(const std::type_info& eventType) : eventType(eventType)
    {
    }

    virtual ~Listener() = default;

    const std::type_info& getHandledEventType() const
    {
        return eventType;
    }

    virtual void handleEvent(Event* event) = 0;
};

template <typename EventType>
class TemplateListener : public Listener
{
public:
    TemplateListener() : Listener(typeid(EventType))
    {
    }

    // a wrapper function that can combined with template achieve automation
    void handleEvent(Event* event) override
    {
        EventType* specificEvent = dynamic_cast<EventType*>(event);
        if (specificEvent)
        {
            onEvent(specificEvent);
        }
    }

protected:
    virtual void onEvent(EventType* event) = 0;
};
