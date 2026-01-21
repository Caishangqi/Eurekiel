#include "RegisterSubsystem.hpp"
#include "../../Core/Event/EventSubsystem.hpp"
#include "../../Core/Logger/LoggerSubsystem.hpp"
#include "../../Core/NamedStrings.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::core
{
    bool RegisterConfig::IsValid() const
    {
        if (defaultNamespaces.empty())
        {
            return false;
        }

        for (const auto& ns : defaultNamespaces)
        {
            if (ns.name.empty())
            {
                return false;
            }
        }

        return true;
    }

    RegisterSubsystem::RegisterSubsystem(const RegisterConfig& config)
        : m_config(config)
    {
        if (!m_config.IsValid())
        {
            // Use default config if invalid
            m_config = RegisterConfig{};
        }
    }

    RegisterSubsystem::~RegisterSubsystem()
    {
        if (m_initialized)
        {
            Shutdown();
        }
    }

    void RegisterSubsystem::Startup()
    {
        if (m_initialized)
        {
            return;
        }

        // Register event handlers if events are enabled
        if (m_config.enableEvents)
        {
            auto* eventSubsystem = SubsystemManager{}.GetSubsystem<enigma::event::EventSubsystem>();
            if (eventSubsystem)
            {
                eventSubsystem->SubscribeStringEvent("RegisterItem", Event_RegistrationChanged);
                eventSubsystem->SubscribeStringEvent("UnregisterItem", Event_RegistrationChanged);
            }
        }

        // Initialize default namespaces
        InitializeDefaultNamespaces();

        m_initialized = true;

        // Log initialization
        auto* logger = enigma::core::SubsystemManager{}.GetSubsystem<LoggerSubsystem>();
        if (logger)
        {
            logger->LogInfo("RegisterSubsystem", "RegisterSubsystem initialized successfully");
        }
    }

    void RegisterSubsystem::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        // Unregister event handlers
        if (m_config.enableEvents)
        {
            auto* eventSubsystem = SubsystemManager{}.GetSubsystem<enigma::event::EventSubsystem>();
            if (eventSubsystem)
            {
                eventSubsystem->UnsubscribeStringEvent("RegisterItem", Event_RegistrationChanged);
                eventSubsystem->UnsubscribeStringEvent("UnregisterItem", Event_RegistrationChanged);
            }
        }

        // Clear all registries
        ClearAllRegistries();

        m_initialized = false;

        // Log shutdown
        auto* logger = enigma::core::SubsystemManager{}.GetSubsystem<LoggerSubsystem>();
        if (logger)
        {
            logger->LogInfo("RegisterSubsystem", "RegisterSubsystem shutdown complete");
        }
    }

    IRegistry* RegisterSubsystem::GetRegistry(const std::string& typeName)
    {
        std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
        auto                                it = m_registriesByName.find(typeName);
        return it != m_registriesByName.end() ? it->second : nullptr;
    }

    const IRegistry* RegisterSubsystem::GetRegistry(const std::string& typeName) const
    {
        std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
        auto                                it = m_registriesByName.find(typeName);
        return it != m_registriesByName.end() ? it->second : nullptr;
    }

    std::vector<std::string> RegisterSubsystem::GetAllRegistryTypes() const
    {
        std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
        std::vector<std::string>            types;
        types.reserve(m_registriesByName.size());

        for (const auto& pair : m_registriesByName)
        {
            types.push_back(pair.first);
        }

        return types;
    }

    size_t RegisterSubsystem::GetTotalRegistrations() const
    {
        std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
        size_t                              total = 0;

        for (const auto& pair : m_registriesByType)
        {
            total += pair.second->GetRegistrationCount();
        }

        return total;
    }

    void RegisterSubsystem::ClearAllRegistries()
    {
        std::unique_lock<std::shared_mutex> lock(m_registriesMutex);

        // [FIX] Unfreeze all registries before clearing (shutdown phase)
        for (auto& pair : m_registriesByType)
        {
            pair.second->Unfreeze();
        }

        // Clear all registrations in each registry
        for (auto& pair : m_registriesByType)
        {
            pair.second->Clear();
        }

        // Clear the registry mappings
        m_registriesByType.clear();
        m_registriesByName.clear();
    }

    void RegisterSubsystem::InitializeDefaultNamespaces()
    {
        // Default namespaces are primarily for organizational purposes
        // The actual namespace handling is done at the registration level

        auto* logger = enigma::core::SubsystemManager{}.GetSubsystem<LoggerSubsystem>();
        if (logger && m_config.enableNamespaces)
        {
            for (const auto& ns : m_config.defaultNamespaces)
            {
                logger->LogInfo("RegisterSubsystem", ("Initialized namespace: " + ns.name).c_str());
            }
        }
    }

    void RegisterSubsystem::FireRegistrationEvent(const std::string& eventType, const RegistrationKey& key, const std::string& typeName)
    {
        UNUSED(key)
        UNUSED(typeName)
        if (!m_config.enableEvents)
        {
            return;
        }

        // Fire event through EventSubsystem
        auto* eventSubsystem = SubsystemManager{}.GetSubsystem<enigma::event::EventSubsystem>();
        if (eventSubsystem)
        {
            eventSubsystem->FireStringEvent(eventType);
        }
    }

    bool RegisterSubsystem::Event_RegistrationChanged(EventArgs& args)
    {
        UNUSED(args)

        // Handle registration change events
        // This could be used for logging, validation, or other system notifications

        auto* logger = enigma::core::SubsystemManager{}.GetSubsystem<LoggerSubsystem>();
        if (logger)
        {
            logger->LogDebug("RegisterSubsystem", "Registration changed event fired");
        }

        return false; // Allow other handlers to process
    }
}
