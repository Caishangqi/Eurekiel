#include "EventBus.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

// ============================================================================
// EventBus.cpp - Event bus implementation
// Part of Event System
// ============================================================================

namespace enigma::event
{
    using namespace enigma::core;

    // Define the log category for Event module
    DEFINE_LOG_CATEGORY(LogEvent);

    EventBus::EventBus()
    {
        LogInfo(LogEvent, "EventBus::Create Event bus created");
    }

    EventBus::~EventBus()
    {
        if (!m_shutdown)
        {
            Shutdown();
        }
    }

    bool EventBus::RemoveListener(ListenerHandle handle)
    {
        if (m_shutdown)
            return false;

        for (auto& pair : m_listeners)
        {
            ListenerList& listeners = pair.second;
            auto          it        = std::find_if(listeners.begin(), listeners.end(),
                                   [handle](const ListenerWrapper& w) { return w.handle == handle; });

            if (it != listeners.end())
            {
                listeners.erase(it);
                LogDebug(LogEvent, "EventBus::RemoveListener Removed listener handle %llu", handle);
                return true;
            }
        }

        LogWarn(LogEvent, "EventBus::RemoveListener Handle %llu not found", handle);
        return false;
    }

    void EventBus::Clear()
    {
        m_listeners.clear();
        LogInfo(LogEvent, "EventBus::Clear All listeners cleared");
    }

    void EventBus::Shutdown()
    {
        if (m_shutdown)
            return;

        m_shutdown = true;
        Clear();
        LogInfo(LogEvent, "EventBus::Shutdown Event bus shutdown complete");
    }

    void EventBus::SortListeners(ListenerList& listeners)
    {
        std::stable_sort(listeners.begin(), listeners.end(),
                         [](const ListenerWrapper& a, const ListenerWrapper& b)
                         {
                             return static_cast<uint8_t>(a.priority) < static_cast<uint8_t>(b.priority);
                         });
    }
} // namespace enigma::event
