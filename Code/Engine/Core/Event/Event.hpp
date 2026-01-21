#pragma once

// ============================================================================
// Event.hpp - Event base class with CRTP support
// Part of Event System
// ============================================================================

#include <cstddef>
#include <type_traits>

namespace enigma::event
{
    /// Event type ID generator (compile-time unique IDs)
    class EventTypeIdGenerator
    {
    public:
        template <typename T>
        static size_t GetId()
        {
            static const size_t id = s_nextId++;
            return id;
        }

    private:
        static inline size_t s_nextId = 0;
    };

    /// Event base class
    class Event
    {
    public:
        virtual ~Event() = default;

        /// Get runtime event type ID
        virtual size_t GetEventTypeId() const = 0;

        /// Get event name for debugging
        virtual const char* GetEventName() const = 0;

        /// Check if event has been handled
        bool IsHandled() const { return m_handled; }

        /// Mark event as handled
        void SetHandled(bool handled = true) { m_handled = handled; }

    protected:
        bool m_handled = false;
    };

    /// Cancellable event interface
    class ICancellableEvent
    {
    public:
        virtual ~ICancellableEvent() = default;

        bool IsCancelled() const { return m_cancelled; }
        void SetCancelled(bool cancelled = true) { m_cancelled = cancelled; }

    protected:
        bool m_cancelled = false;
    };

    /// CRTP base class for automatic type ID implementation
    /// Usage: class MyEvent : public EventBase<MyEvent> { ... };
    template <typename Derived>
    class EventBase : public Event
    {
    public:
        size_t GetEventTypeId() const override
        {
            return StaticEventTypeId();
        }

        /// Get static type ID (can be called without instance)
        static size_t StaticEventTypeId()
        {
            return EventTypeIdGenerator::GetId<Derived>();
        }
    };

    /// Cancellable event CRTP base
    template <typename Derived>
    class CancellableEventBase : public EventBase<Derived>, public ICancellableEvent
    {
    };
} // namespace enigma::event
