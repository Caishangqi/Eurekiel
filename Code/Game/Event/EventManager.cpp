#include "EventManager.hpp"
#include "Listener.hpp"

EventManager* EventManager::instance = nullptr;

EventManager::EventManager()
{
    printf("[event]    EventManager Initialized\n");
}

EventManager::~EventManager()
{
    for (Listener* listener : listeners)
    {
        delete listener;
        listener = nullptr;
    }
}

EventManager* EventManager::GetInstance()
{
    if (instance == nullptr)
        instance = new EventManager();
    return instance;
}

void EventManager::RegisterListener(Listener* listener)
{
    listeners.push_back(listener);
    printf("[event]    Register Event: %s\n", listener->getHandledEventType().name());
}

void EventManager::DispatchEvent(Event* event)
{
    for (auto& listener : listeners)
    {
        if (listener->getHandledEventType() == event->getType())
        {
            listener->handleEvent(event);
        }
    }
    delete event;
}
