#include "EventSubsystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

// [FIX] Include for g_theEventSubsystem global pointer
extern enigma::event::EventSubsystem* g_theEventSubsystem;

// ============================================================================
// EventSubsystem.cpp - Event subsystem implementation
// Part of Event System
// ============================================================================

namespace enigma::event
{
    EventSubsystem::EventSubsystem(EventSubsystemConfig& config)
        : m_config(config)
    {
        LogInfo(LogEvent, "EventSubsystem::Create Event subsystem created");
    }

    EventSubsystem::~EventSubsystem()
    {
        if (m_modBus || m_gameBus || m_stringBus)
        {
            LogWarn(LogEvent, "EventSubsystem::Destroy Subsystem destroyed without proper shutdown");
        }
    }

    void EventSubsystem::Startup()
    {
        LogInfo(LogEvent, "EventSubsystem::Startup Initializing event subsystem...");

        // Create type-safe event buses
        m_modBus  = std::make_unique<EventBus>();
        m_gameBus = std::make_unique<EventBus>();

        // Create string-based event bus (replaces legacy EventSystem)
        m_stringBus = std::make_unique<StringEventBus>();

        // [FIX] Set global pointer for FireEvent macro and other global access
        g_theEventSubsystem = this;

        LogInfo(LogEvent, "EventSubsystem::Startup Event subsystem started successfully");
        LogInfo(LogEvent, "EventSubsystem::Startup ModBus, GameBus, and StringBus created");
    }

    void EventSubsystem::Shutdown()
    {
        LogInfo(LogEvent, "EventSubsystem::Shutdown Shutting down event subsystem...");

        // [FIX] Clear global pointer before shutdown
        g_theEventSubsystem = nullptr;

        // Shutdown string event bus
        if (m_stringBus)
        {
            m_stringBus->Clear();
            m_stringBus.reset();
            LogDebug(LogEvent, "EventSubsystem::Shutdown StringBus shutdown complete");
        }

        // Shutdown type-safe event buses
        if (m_gameBus)
        {
            m_gameBus->Shutdown();
            m_gameBus.reset();
            LogDebug(LogEvent, "EventSubsystem::Shutdown GameBus shutdown complete");
        }

        if (m_modBus)
        {
            m_modBus->Shutdown();
            m_modBus.reset();
            LogDebug(LogEvent, "EventSubsystem::Shutdown ModBus shutdown complete");
        }

        LogInfo(LogEvent, "EventSubsystem::Shutdown Event subsystem shutdown complete");
    }
} // namespace enigma::event
