#include "EventSystem.hpp"

#include "DevConsole.hpp"
#include "NamedStrings.hpp"
#include "Rgba8.hpp"
EventSystem* g_theEventSystem = nullptr;

EventSystem::EventSystem(const EventSystemConfig& config): m_config(config)
{
}

EventSystem::~EventSystem()
{
}

void EventSystem::Startup()
{
}

void EventSystem::Shutdown()
{
}

void EventSystem::BeginFrame()
{
}

void EventSystem::EndFrame()
{
}

void EventSystem::SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
    // Find or create the subscription list for the given event name
    auto&             subscriptionList = m_subscriptionListsByEventName[eventName];
    EventSubscription subscription;
    subscription.callbackFunction = functionPtr;
    subscription.type             = EventSubscriptionType::STATIC;
    subscriptionList.push_back(subscription);
}

void EventSystem::UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
    auto it = m_subscriptionListsByEventName.find(eventName);
    if (it == m_subscriptionListsByEventName.end())
    {
        return;
    }

    SubscriptionList& subscriptionList = it->second;

    for (auto subIt = subscriptionList.begin(); subIt != subscriptionList.end(); ++subIt)
    {
        if (subIt->callbackFunction == functionPtr)
        {
            subscriptionList.erase(subIt);
            break;
        }
    }

    // If the subscription list is empty, remove the event name from the map
    if (subscriptionList.empty())
    {
        m_subscriptionListsByEventName.erase(it);
    }
}

void EventSystem::FireEvent(const std::string& eventName, EventArgs& args)
{
    auto& subscriptionList = m_subscriptionListsByEventName[eventName];
    for (EventSubscription& sub : subscriptionList)
    {
        if (sub.callbackFunction)
        {
            // https://refactoring.guru/design-patterns/chain-of-responsibility
            bool wasConsumed = sub.callbackFunction(args);
            if (wasConsumed)
                break;
        }
    }
}

void EventSystem::FireEvent(const std::string& eventName)
{
    auto& subscriptionList = m_subscriptionListsByEventName[eventName];
    for (EventSubscription& sub : subscriptionList)
    {
        if (sub.callbackFunction)
        {
            EventArgs arg;
            bool      wasConsumed = sub.callbackFunction(arg);
            if (wasConsumed)
                break;
        }
    }
}

void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
    g_theEventSystem->SubscribeEventCallbackFunction(eventName, functionPtr);
}

void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
    g_theEventSystem->UnsubscribeEventCallbackFunction(eventName, functionPtr);
}

void FireEvent(const std::string& eventName, EventArgs& args)
{
    g_theEventSystem->FireEvent(eventName, args);
}

void FireEvent(const std::string& eventName)
{
    g_theEventSystem->FireEvent(eventName);
}
