#pragma once

// ============================================================================
// RegisterEvent.hpp - Registration event template for DeferredRegister
// Part of Event System
// Used to trigger deferred registration during startup
// ============================================================================

#include "Event.hpp"
#include <string>

namespace enigma::event
{
    //-----------------------------------------------------------------------------------------------
    // Register Event Template
    //
    // USAGE:
    // This event is posted by GameData during the registration phase.
    // DeferredRegister instances listen for this event to perform actual registration.
    //
    // Example:
    //   // In DeferredRegister<Block>
    //   modBus.AddListener<RegisterEvent<BlockRegistry>>([this](auto& e) {
    //       this->OnRegisterEvent(e);
    //   });
    //
    //   // In GameData
    //   RegisterEvent<BlockRegistry> event(blockRegistry);
    //   modBus.Post(event);
    //-----------------------------------------------------------------------------------------------
    template <typename TRegistry>
    class RegisterEvent : public EventBase<RegisterEvent<TRegistry>>
    {
    public:
        explicit RegisterEvent(TRegistry& registry)
            : m_registry(registry)
        {
        }

        const char* GetEventName() const override
        {
            return "RegisterEvent";
        }

        /// Get the registry reference
        TRegistry&       GetRegistry() { return m_registry; }
        const TRegistry& GetRegistry() const { return m_registry; }

    private:
        TRegistry& m_registry;
    };
} // namespace enigma::event
