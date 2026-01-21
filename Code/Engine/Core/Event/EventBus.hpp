#pragma once

// ============================================================================
// EventBus.hpp - Type-safe event bus with priority support
// Part of Event System
// ============================================================================

#include "Event.hpp"
#include "EventPriority.hpp"
#include "EventCommon.hpp"
#include <functional>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <type_traits>

namespace enigma::event
{
    /// Listener handle for removal
    using ListenerHandle = uint64_t;

    /// Event bus - type-safe event dispatching with priority
    class EventBus
    {
    public:
        EventBus();
        ~EventBus();

        // Disable copy
        EventBus(const EventBus&)            = delete;
        EventBus& operator=(const EventBus&) = delete;

        // ========================================================================
        // Listener Registration
        // ========================================================================

        /// Add a listener for event type TEvent
        /// @tparam TEvent Event type (must derive from Event)
        /// @param callback Callback function
        /// @param priority Listener priority
        /// @param receiveCancelled Whether to receive cancelled events
        /// @return Handle for later removal
        template <typename TEvent, typename F>
        ListenerHandle AddListener(F&&           callback,
                                   EventPriority priority         = EventPriority::Normal,
                                   bool          receiveCancelled = false);

        /// Add a member function listener
        template <typename TEvent, typename T, typename Method>
        ListenerHandle AddListener(T*            instance, Method method,
                                   EventPriority priority         = EventPriority::Normal,
                                   bool          receiveCancelled = false);

        /// Remove a listener by handle
        /// @return true if removed, false if not found
        bool RemoveListener(ListenerHandle handle);

        // ========================================================================
        // Event Posting
        // ========================================================================

        /// Post an event to all listeners
        /// @return true if event was cancelled
        template <typename TEvent>
        bool Post(TEvent& event);

        /// Post an rvalue event
        template <typename TEvent>
        bool Post(TEvent&& event);

        // ========================================================================
        // Management
        // ========================================================================

        /// Clear all listeners
        void Clear();

        /// Clear listeners for specific event type
        template <typename TEvent>
        void ClearListeners();

        /// Shutdown the event bus
        void Shutdown();

        /// Check if shutdown
        bool IsShutdown() const { return m_shutdown; }

    private:
        struct ListenerWrapper
        {
            ListenerHandle              handle;
            std::function<void(Event&)> callback;
            EventPriority               priority;
            bool                        receiveCancelled;
        };

        using ListenerList = std::vector<ListenerWrapper>;

        void SortListeners(ListenerList& listeners);

        std::unordered_map<size_t, ListenerList> m_listeners;
        ListenerHandle                           m_nextHandle = 1;
        bool                                     m_shutdown   = false;
    };

    // ============================================================================
    // Template Implementations
    // ============================================================================

    template <typename TEvent, typename F>
    ListenerHandle EventBus::AddListener(F&&           callback,
                                         EventPriority priority,
                                         bool          receiveCancelled)
    {
        static_assert(std::is_base_of_v<Event, TEvent>,
                      "TEvent must derive from Event");

        if (m_shutdown)
            return INVALID_LISTENER_HANDLE;

        const size_t typeId = EventTypeIdGenerator::GetId<TEvent>();

        ListenerWrapper wrapper;
        wrapper.handle   = m_nextHandle++;
        wrapper.callback = [cb = std::forward<F>(callback)](Event& e)
        {
            cb(static_cast<TEvent&>(e));
        };
        wrapper.priority         = priority;
        wrapper.receiveCancelled = receiveCancelled;

        auto& listeners = m_listeners[typeId];
        listeners.push_back(std::move(wrapper));
        SortListeners(listeners);

        return wrapper.handle;
    }

    template <typename TEvent, typename T, typename Method>
    ListenerHandle EventBus::AddListener(T*            instance, Method method,
                                         EventPriority priority,
                                         bool          receiveCancelled)
    {
        static_assert(std::is_member_function_pointer_v<Method>,
                      "Method must be a member function pointer");

        return AddListener<TEvent>(
            [instance, method](TEvent& e)
            {
                (instance->*method)(e);
            },
            priority,
            receiveCancelled
        );
    }

    template <typename TEvent>
    bool EventBus::Post(TEvent& event)
    {
        static_assert(std::is_base_of_v<Event, TEvent>,
                      "TEvent must derive from Event");

        if (m_shutdown)
            return false;

        const size_t typeId = EventTypeIdGenerator::GetId<TEvent>();

        auto it = m_listeners.find(typeId);
        if (it == m_listeners.end())
            return false;

        bool cancelled = false;

        for (const auto& wrapper : it->second)
        {
            // Check cancellation for ICancellableEvent
            if constexpr (std::is_base_of_v<ICancellableEvent, TEvent>)
            {
                if (static_cast<ICancellableEvent&>(event).IsCancelled())
                {
                    cancelled = true;
                    if (!wrapper.receiveCancelled)
                        continue;
                }
            }

            wrapper.callback(event);
        }

        return cancelled;
    }

    template <typename TEvent>
    bool EventBus::Post(TEvent&& event)
    {
        return Post(static_cast<TEvent&>(event));
    }

    template <typename TEvent>
    void EventBus::ClearListeners()
    {
        const size_t typeId = EventTypeIdGenerator::GetId<TEvent>();
        m_listeners.erase(typeId);
    }
} // namespace enigma::event
