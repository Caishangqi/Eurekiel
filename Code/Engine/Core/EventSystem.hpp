#pragma once
#include <map>
#include <string>
#include <vector>

class NamedStrings;
using EventArgs             = NamedStrings;
using EventCallbackFunction = bool(*)(EventArgs& args);

struct EventSystemConfig
{
};

enum class EventSubscriptionType
{
    STATIC,
    STANDALONE
};

struct EventSubscription
{
    EventCallbackFunction callbackFunction;
    EventSubscriptionType type;
};

using SubscriptionList = std::vector<EventSubscription>;


class EventSystem
{
public:
    EventSystem(const EventSystemConfig& config);
    ~EventSystem();

    void Startup();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
    void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
    void FireEvent(const std::string& eventName, EventArgs& args);
    void FireEvent(const std::string& eventName);

protected:
    EventSystemConfig                       m_config;
    std::map<std::string, SubscriptionList> m_subscriptionListsByEventName;
};

// Standalone global-namespace helper functions; these forward to "the" event system, if it exists
void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
void FireEvent(const std::string& eventName, EventArgs& args);
void FireEvent(const std::string& eventName);
