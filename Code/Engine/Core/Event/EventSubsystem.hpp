#pragma once

// ============================================================================
// EventSubsystem.hpp - Event system engine subsystem
// Part of Event System
// Provides global EventBus access through GEngine
// ============================================================================

#include "Engine/Core/SubsystemManager.hpp"
#include "EventBus.hpp"
#include "StringEventBus.hpp"
#include "EventCommon.hpp"
#include <memory>

namespace enigma::event
{
    //-----------------------------------------------------------------------------------------------
    // Event Subsystem Configuration
    // Can be extended for future configuration needs
    //-----------------------------------------------------------------------------------------------
    struct EventSubsystemConfig
    {
        // Reserved for future configuration options
        uint32_t initialListenerCapacity = DEFAULT_LISTENER_CAPACITY;
    };

    //-----------------------------------------------------------------------------------------------
    // Event Subsystem
    //
    // DESIGN PHILOSOPHY:
    // - Provides centralized event bus management
    // - Integrates with engine subsystem lifecycle
    // - Supports multiple named event buses (ModBus, GameBus, StringEventBus)
    //
    // EVENT BUS TYPES:
    // - ModBus: Type-safe events for registration (RegisterEvent<Block>, etc.)
    // - GameBus: Type-safe events for gameplay
    // - StringEventBus: String-based events for console commands, input, window events
    //
    // LIFECYCLE:
    // 1. Construction: Initialize config
    // 2. Startup(): Create default event buses
    // 3. Runtime: Post/Listen events through GetEventBus()
    // 4. Shutdown(): Cleanup all event buses
    //
    // USAGE:
    //   auto* eventSubsystem = GEngine->GetSubsystem<EventSubsystem>();
    //   
    //   // Type-safe events
    //   eventSubsystem->GetModBus().Post(MyEvent{});
    //   
    //   // String-based events (console commands, input)
    //   eventSubsystem->GetStringBus().Subscribe("KeyPressed", OnKeyPressed);
    //   eventSubsystem->GetStringBus().Fire("quit");
    //-----------------------------------------------------------------------------------------------
    using namespace enigma::core;

    class EventSubsystem : public EngineSubsystem
    {
    public:
        // Priority 10: Event system should start early (before most other subsystems)
        DECLARE_SUBSYSTEM(EventSubsystem, "EventSubsystem", 10)

    public:
        explicit EventSubsystem(EventSubsystemConfig& config);
        ~EventSubsystem() override;

        // EngineSubsystem Interface
        void Startup() override;
        void Shutdown() override;

        //-------------------------------------------------------------------------------------------
        // Type-Safe Event Bus Access
        //-------------------------------------------------------------------------------------------

        /// Get the Mod event bus (for registration events like RegisterEvent<Block>)
        EventBus&       GetModBus() { return *m_modBus; }
        const EventBus& GetModBus() const { return *m_modBus; }

        /// Get the Game event bus (for gameplay events)
        EventBus&       GetGameBus() { return *m_gameBus; }
        const EventBus& GetGameBus() const { return *m_gameBus; }

        //-------------------------------------------------------------------------------------------
        // String-Based Event Bus Access (replaces legacy EventSystem)
        //-------------------------------------------------------------------------------------------

        /// Get the String event bus (for console commands, input events, window events)
        StringEventBus&       GetStringBus() { return *m_stringBus; }
        const StringEventBus& GetStringBus() const { return *m_stringBus; }

        //-------------------------------------------------------------------------------------------
        // Convenience Methods - Type-Safe Events
        //-------------------------------------------------------------------------------------------

        /// Post event to ModBus
        template <typename TEvent>
        bool PostToModBus(TEvent&& event)
        {
            return m_modBus->Post(std::forward<TEvent>(event));
        }

        /// Post event to GameBus
        template <typename TEvent>
        bool PostToGameBus(TEvent&& event)
        {
            return m_gameBus->Post(std::forward<TEvent>(event));
        }

        //-------------------------------------------------------------------------------------------
        // Convenience Methods - String Events (Legacy EventSystem compatibility)
        //-------------------------------------------------------------------------------------------

        /// Subscribe to a string event
        void SubscribeStringEvent(const std::string& eventName, EventCallbackFunction callback)
        {
            m_stringBus->Subscribe(eventName, callback);
        }

        /// Unsubscribe from a string event
        void UnsubscribeStringEvent(const std::string& eventName, EventCallbackFunction callback)
        {
            m_stringBus->Unsubscribe(eventName, callback);
        }

        /// Fire a string event with arguments
        bool FireStringEvent(const std::string& eventName, EventArgs& args)
        {
            return m_stringBus->Fire(eventName, args);
        }

        /// Fire a string event without arguments
        bool FireStringEvent(const std::string& eventName)
        {
            return m_stringBus->Fire(eventName);
        }

    private:
        EventSubsystemConfig& m_config;

        // Type-safe event buses
        std::unique_ptr<EventBus> m_modBus; // For mod/registration events
        std::unique_ptr<EventBus> m_gameBus; // For gameplay events

        // String-based event bus (replaces legacy EventSystem)
        std::unique_ptr<StringEventBus> m_stringBus; // For console commands, input, window events
    };
} // namespace enigma::event
