#pragma once

// ============================================================================
// MulticastDelegate.hpp - Multi-cast delegate template
// Part of Event System
// Supports multiple listeners with handle-based removal
// ============================================================================

#include <functional>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <cstdint>

namespace enigma::event
{
    /// Handle type for listener identification
    using DelegateHandle = uint64_t;

    /// Multi-cast delegate - can bind multiple callbacks
    /// @tparam Args Callback argument types (return type is always void)
    template <typename... Args>
    class MulticastDelegate
    {
    public:
        using CallbackType = std::function<void(Args...)>;

        MulticastDelegate()  = default;
        ~MulticastDelegate() = default;

        // Move semantics
        MulticastDelegate(MulticastDelegate&&) noexcept            = default;
        MulticastDelegate& operator=(MulticastDelegate&&) noexcept = default;

        // Disable copy (listeners contain state)
        MulticastDelegate(const MulticastDelegate&)            = delete;
        MulticastDelegate& operator=(const MulticastDelegate&) = delete;

        // ========================================================================
        // Add Listener Methods
        // ========================================================================

        /// Add a callable listener
        /// @return Handle for later removal
        template <typename F>
        DelegateHandle Add(F&& func)
        {
            DelegateHandle handle = m_nextHandle++;
            m_listeners.push_back({handle, std::forward<F>(func)});
            return handle;
        }

        /// Add a member function listener
        /// @param instance Pointer to object instance
        /// @param method Pointer to member function
        /// @return Handle for later removal
        template <typename T, typename Method>
        DelegateHandle Add(T* instance, Method method)
        {
            static_assert(std::is_member_function_pointer_v<Method>,
                          "Method must be a member function pointer");

            return Add([instance, method](Args... args)
            {
                (instance->*method)(std::forward<Args>(args)...);
            });
        }

        // ========================================================================
        // Remove Listener Methods
        // ========================================================================

        /// Remove a listener by handle
        /// @return true if removed, false if handle not found
        bool Remove(DelegateHandle handle)
        {
            auto it = std::find_if(m_listeners.begin(), m_listeners.end(),
                                   [handle](const Listener& l) { return l.handle == handle; });

            if (it != m_listeners.end())
            {
                m_listeners.erase(it);
                return true;
            }
            return false;
        }

        /// Remove all listeners
        void Clear()
        {
            m_listeners.clear();
        }

        // ========================================================================
        // Query Methods
        // ========================================================================

        /// Get number of listeners
        size_t GetCount() const
        {
            return m_listeners.size();
        }

        /// Check if any listeners are bound
        bool HasListeners() const
        {
            return !m_listeners.empty();
        }

        /// Implicit bool conversion
        explicit operator bool() const
        {
            return HasListeners();
        }

        // ========================================================================
        // Broadcast Methods
        // ========================================================================

        /// Broadcast to all listeners
        void Broadcast(Args... args) const
        {
            // Copy listeners in case callback modifies the list
            auto listenersCopy = m_listeners;
            for (const auto& listener : listenersCopy)
            {
                listener.callback(args...);
            }
        }

        /// Function call operator (same as Broadcast)
        void operator()(Args... args) const
        {
            Broadcast(std::forward<Args>(args)...);
        }

    private:
        struct Listener
        {
            DelegateHandle handle;
            CallbackType   callback;
        };

        std::vector<Listener> m_listeners;
        DelegateHandle        m_nextHandle = 1; // Start from 1, 0 is invalid
    };
} // namespace enigma::event
