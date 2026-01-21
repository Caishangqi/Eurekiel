#pragma once

// ============================================================================
// EventException.hpp - Exception classes for Event module
// Part of Event System
// ============================================================================

#include <stdexcept>
#include <string>
#include <cstdint>

namespace enigma::event
{
    // ========== Base Exception ==========

    /// Base exception for all Event system errors
    class EventException : public std::runtime_error
    {
    public:
        explicit EventException(const std::string& message);
    };

    // ========== Delegate Exceptions ==========

    /// Delegate not bound when Execute called (recoverable)
    class DelegateNotBoundException : public EventException
    {
    public:
        explicit DelegateNotBoundException(const std::string& context);
    };

    // ========== EventBus Exceptions ==========

    /// EventBus is shutdown, cannot perform operation (recoverable)
    class EventBusShutdownException : public EventException
    {
    public:
        explicit EventBusShutdownException(const std::string& operation);
    };

    /// Event recursion depth exceeded (fatal)
    class EventRecursionException : public EventException
    {
    public:
        EventRecursionException(const std::string& eventName, uint32_t depth);
    };

    // ========== Registry Exceptions ==========

    /// Registry is frozen, cannot register new items (fatal)
    class RegistryFrozenException : public EventException
    {
    public:
        RegistryFrozenException(const std::string& registryName, const std::string& itemId);
    };

    /// DeferredHolder accessed before resolution (fatal)
    class HolderNotResolvedException : public EventException
    {
    public:
        explicit HolderNotResolvedException(const std::string& holderId);
    };

    /// Invalid listener handle (recoverable)
    class InvalidListenerHandleException : public EventException
    {
    public:
        InvalidListenerHandleException(uint64_t handle, const std::string& context);
    };
} // namespace enigma::event
