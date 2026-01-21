#pragma once

// ============================================================================
// StringEventBus.hpp - String-based event dispatch system
// Part of Event System
// 
// Replaces the legacy EventSystem with a cleaner implementation that
// integrates with the new event architecture while maintaining API compatibility.
// 
// Key Features:
// - String-based event names (for console commands, input events, etc.)
// - Static function pointer callbacks (compatible with legacy code)
// - Return value propagation (event consumption)
// ============================================================================

#include "EventCommon.hpp"
#include "../NamedStrings.hpp"
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace enigma::event
{
    // Type aliases for compatibility with legacy EventSystem
    using EventArgs             = NamedStrings;
    using EventCallbackFunction = bool(*)(EventArgs& args);

    /**
     * @class StringEventBus
     * @brief String-based event bus for command-style events
     * 
     * This class provides a string-keyed event dispatch system, primarily used for:
     * - Console commands (e.g., "quit", "help", "clear")
     * - Input events (e.g., "KeyPressed", "KeyReleased", "CharInput")
     * - Window events (e.g., "WindowCloseEvent")
     * 
     * Unlike the type-safe EventBus, this uses string event names and
     * NamedStrings (EventArgs) for flexible, dynamic event handling.
     * 
     * Usage:
     * @code
     * // Subscribe to an event
     * stringEventBus.Subscribe("KeyPressed", &MyClass::OnKeyPressed);
     * 
     * // Fire an event with arguments
     * EventArgs args;
     * args.SetValue("KeyCode", 65);
     * stringEventBus.Fire("KeyPressed", args);
     * 
     * // Fire an event without arguments
     * stringEventBus.Fire("quit");
     * @endcode
     */
    class StringEventBus
    {
    public:
        StringEventBus()  = default;
        ~StringEventBus() = default;

        // Non-copyable, non-movable
        StringEventBus(const StringEventBus&)            = delete;
        StringEventBus& operator=(const StringEventBus&) = delete;
        StringEventBus(StringEventBus&&)                 = delete;
        StringEventBus& operator=(StringEventBus&&)      = delete;

        /**
         * @brief Subscribe a static function to an event
         * 
         * @param eventName The name of the event to subscribe to
         * @param callback The static function pointer to call when event fires
         * 
         * @note Only static functions are supported (same as legacy EventSystem)
         */
        void Subscribe(const std::string& eventName, EventCallbackFunction callback)
        {
            if (!callback)
                return;

            auto& subscribers = m_subscribersByEventName[eventName];

            // Check if already subscribed
            auto it = std::find(subscribers.begin(), subscribers.end(), callback);
            if (it == subscribers.end())
            {
                subscribers.push_back(callback);
            }
        }

        /**
         * @brief Unsubscribe a static function from an event
         * 
         * @param eventName The name of the event to unsubscribe from
         * @param callback The static function pointer to remove
         */
        void Unsubscribe(const std::string& eventName, EventCallbackFunction callback)
        {
            auto it = m_subscribersByEventName.find(eventName);
            if (it == m_subscribersByEventName.end())
                return;

            auto& subscribers = it->second;
            auto  callbackIt  = std::find(subscribers.begin(), subscribers.end(), callback);
            if (callbackIt != subscribers.end())
            {
                subscribers.erase(callbackIt);
            }

            // Clean up empty subscription lists
            if (subscribers.empty())
            {
                m_subscribersByEventName.erase(it);
            }
        }

        /**
         * @brief Fire an event with arguments
         * 
         * @param eventName The name of the event to fire
         * @param args The event arguments (NamedStrings)
         * @return true if any subscriber consumed the event, false otherwise
         * 
         * Callbacks are invoked in subscription order. If a callback returns true,
         * the event is considered "consumed" but remaining callbacks are still invoked.
         */
        bool Fire(const std::string& eventName, EventArgs& args)
        {
            auto it = m_subscribersByEventName.find(eventName);
            if (it == m_subscribersByEventName.end())
                return false;

            bool consumed = false;

            // Make a copy of subscribers in case callbacks modify the list
            auto subscribers = it->second;

            for (auto& callback : subscribers)
            {
                if (callback)
                {
                    if (callback(args))
                    {
                        consumed = true;
                    }
                }
            }

            return consumed;
        }

        /**
         * @brief Fire an event without arguments
         * 
         * @param eventName The name of the event to fire
         * @return true if any subscriber consumed the event, false otherwise
         */
        bool Fire(const std::string& eventName)
        {
            EventArgs emptyArgs;
            return Fire(eventName, emptyArgs);
        }

        /**
         * @brief Check if an event has any subscribers
         * 
         * @param eventName The name of the event to check
         * @return true if the event has at least one subscriber
         */
        bool HasSubscribers(const std::string& eventName) const
        {
            auto it = m_subscribersByEventName.find(eventName);
            return it != m_subscribersByEventName.end() && !it->second.empty();
        }

        /**
         * @brief Get the number of subscribers for an event
         * 
         * @param eventName The name of the event
         * @return The number of subscribers
         */
        size_t GetSubscriberCount(const std::string& eventName) const
        {
            auto it = m_subscribersByEventName.find(eventName);
            return it != m_subscribersByEventName.end() ? it->second.size() : 0;
        }

        /**
         * @brief Get all registered event names
         * 
         * @return Vector of event names that have subscribers
         */
        std::vector<std::string> GetAllEventNames() const
        {
            std::vector<std::string> names;
            names.reserve(m_subscribersByEventName.size());
            for (const auto& pair : m_subscribersByEventName)
            {
                names.push_back(pair.first);
            }
            return names;
        }

        /**
         * @brief Clear all subscriptions
         */
        void Clear()
        {
            m_subscribersByEventName.clear();
        }

        /**
         * @brief Clear subscriptions for a specific event
         * 
         * @param eventName The name of the event to clear
         */
        void ClearEvent(const std::string& eventName)
        {
            m_subscribersByEventName.erase(eventName);
        }

    private:
        std::map<std::string, std::vector<EventCallbackFunction>> m_subscribersByEventName;
    };
} // namespace enigma::event
