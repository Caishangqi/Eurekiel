#include "EventException.hpp"
#include <sstream>

// ============================================================================
// EventException.cpp - Exception implementations for Event module
// Part of Event System
// ============================================================================

namespace enigma::event
{
    // ========== Base Exception ==========

    EventException::EventException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    // ========== Delegate Exceptions ==========

    DelegateNotBoundException::DelegateNotBoundException(const std::string& context)
        : EventException("Delegate::Execute " + context + " - Delegate is not bound")
    {
    }

    // ========== EventBus Exceptions ==========

    EventBusShutdownException::EventBusShutdownException(const std::string& operation)
        : EventException("EventBus::" + operation + " - EventBus has been shutdown")
    {
    }

    EventRecursionException::EventRecursionException(const std::string& eventName, uint32_t depth)
        : EventException("EventBus::Post " + eventName + " - Recursion depth " +
            std::to_string(depth) + " exceeded maximum allowed")
    {
    }

    // ========== Registry Exceptions ==========

    RegistryFrozenException::RegistryFrozenException(const std::string& registryName, const std::string& itemId)
        : EventException("Registry::Register " + registryName + " is frozen, cannot register '" + itemId + "'")
    {
    }

    HolderNotResolvedException::HolderNotResolvedException(const std::string& holderId)
        : EventException("DeferredHolder::Get '" + holderId + "' - Holder not resolved, registration not complete")
    {
    }

    InvalidListenerHandleException::InvalidListenerHandleException(uint64_t handle, const std::string& context)
        : EventException("EventBus::" + context + " - Invalid listener handle " + std::to_string(handle))
    {
    }
} // namespace enigma::event
