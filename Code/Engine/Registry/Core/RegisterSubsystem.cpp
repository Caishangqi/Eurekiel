#include "RegisterSubsystem.hpp"
#include "../../Core/Event/EventSubsystem.hpp"
#include "../../Core/Logger/LoggerSubsystem.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Core/NamedStrings.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"

DECLARE_LOG_CATEGORY_EXTERN(LogRegisterSubsystem)
DEFINE_LOG_CATEGORY(LogRegisterSubsystem)

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

        // Initialize default registry lifecycles (Block, Item, Entity, etc.)
        InitializeDefaultRegistryLifecycles();

        m_initialized = true;

        LogInfo(LogRegisterSubsystem, "RegisterSubsystem initialized successfully");
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

        // Clear all registries (this will unfreeze first)
        ClearAllRegistries();

        // Clear lifecycle entries
        m_lifecycleEntries.clear();
        m_frozen               = false;
        m_registrationComplete = false;

        m_initialized = false;

        LogInfo(LogRegisterSubsystem, "RegisterSubsystem shutdown complete");
    }

    // ============================================================
    // Registry Lifecycle Management Implementation
    // ============================================================

    void RegisterSubsystem::InitializeDefaultRegistryLifecycles()
    {
        LogInfo(LogRegisterSubsystem, "Registering default registry lifecycles");

        // Register Block registry lifecycle
        // Reference: NeoForge registers BLOCK first in GameData.java
        RegisterRegistryLifecycle(
            "Block",
            [](event::EventBus& bus)
            {
                registry::block::BlockRegistry::FireRegisterEvent(bus);
            },
            []()
            {
                registry::block::BlockRegistry::Freeze();
            },
            []()
            {
                registry::block::BlockRegistry::Unfreeze();
            },
            []()
            {
                return registry::block::BlockRegistry::IsFrozen();
            }
        );

        // Future: Register Item registry lifecycle
        // RegisterRegistryLifecycle(
        //     "Item",
        //     [](event::EventBus& bus) { item::ItemRegistry::FireRegisterEvent(bus); },
        //     []() { item::ItemRegistry::Freeze(); },
        //     []() { item::ItemRegistry::Unfreeze(); },
        //     []() { return item::ItemRegistry::IsFrozen(); }
        // );

        LogInfo(LogRegisterSubsystem, "Registered %zu registry lifecycles", m_lifecycleEntries.size());
    }

    void RegisterSubsystem::RegisterRegistryLifecycle(
        const std::string&                    name,
        std::function<void(event::EventBus&)> eventPoster,
        std::function<void()>                 freezer,
        std::function<void()>                 unfreezer,
        std::function<bool()>                 isFrozenChecker)
    {
        RegistryLifecycleEntry entry;
        entry.name      = name;
        entry.postEvent = std::move(eventPoster);
        entry.freeze    = std::move(freezer);
        entry.unfreeze  = std::move(unfreezer);
        entry.isFrozen  = std::move(isFrozenChecker);

        m_lifecycleEntries.push_back(std::move(entry));

        LogDebug(LogRegisterSubsystem, "Added registry lifecycle '%s' (order: %zu)",
                 name.c_str(), m_lifecycleEntries.size());
    }

    void RegisterSubsystem::PostRegisterEvents()
    {
        // Get ModBus from EventSubsystem
        auto* eventSubsystem = SubsystemManager{}.GetSubsystem<event::EventSubsystem>();
        if (!eventSubsystem)
        {
            LogError(LogRegisterSubsystem, "PostRegisterEvents: EventSubsystem not available");
            return;
        }

        PostRegisterEvents(eventSubsystem->GetModBus());
    }

    void RegisterSubsystem::PostRegisterEvents(event::EventBus& eventBus)
    {
        if (m_registrationComplete)
        {
            LogWarn(LogRegisterSubsystem, "PostRegisterEvents: Already called, ignoring");
            return;
        }

        LogInfo(LogRegisterSubsystem, "PostRegisterEvents: Starting registration phase");

        // Post RegisterEvent for each registry in order
        // Reference: GameData.java:78-107 iterates through ORDERED_REGISTRIES
        for (const auto& entry : m_lifecycleEntries)
        {
            LogInfo(LogRegisterSubsystem, "PostRegisterEvents: Posting RegisterEvent for '%s'", entry.name.c_str());

            try
            {
                entry.postEvent(eventBus);
                LogInfo(LogRegisterSubsystem, "PostRegisterEvents: '%s' registration completed", entry.name.c_str());
            }
            catch (const std::exception& e)
            {
                LogError(LogRegisterSubsystem, "PostRegisterEvents: '%s' registration failed: %s",
                         entry.name.c_str(), e.what());
                throw;
            }
        }

        m_registrationComplete = true;
        LogInfo(LogRegisterSubsystem, "PostRegisterEvents: Registration phase completed");
    }

    void RegisterSubsystem::FreezeAllRegistries()
    {
        if (m_frozen)
        {
            LogWarn(LogRegisterSubsystem, "FreezeAllRegistries: Already frozen, ignoring");
            return;
        }

        LogInfo(LogRegisterSubsystem, "FreezeAllRegistries: Freezing all registries");

        // Freeze all registry lifecycles
        // Reference: GameData.java:65-76 calls freeze() on each MappedRegistry
        for (const auto& entry : m_lifecycleEntries)
        {
            LogDebug(LogRegisterSubsystem, "FreezeAllRegistries: Freezing '%s'", entry.name.c_str());
            entry.freeze();
        }

        // Also freeze registries managed by this subsystem
        {
            std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
            for (auto& pair : m_registriesByType)
            {
                pair.second->Freeze();
            }
        }

        m_frozen = true;
        LogInfo(LogRegisterSubsystem, "FreezeAllRegistries: All registries frozen");
    }

    void RegisterSubsystem::UnfreezeAllRegistries()
    {
        if (!m_frozen)
        {
            LogWarn(LogRegisterSubsystem, "UnfreezeAllRegistries: Not frozen, ignoring");
            return;
        }

        LogWarn(LogRegisterSubsystem, "UnfreezeAllRegistries: [WARNING] Unfreezing registries - use with caution!");

        // Unfreeze all registry lifecycles
        for (const auto& entry : m_lifecycleEntries)
        {
            LogDebug(LogRegisterSubsystem, "UnfreezeAllRegistries: Unfreezing '%s'", entry.name.c_str());
            entry.unfreeze();
        }

        // Also unfreeze registries managed by this subsystem
        {
            std::shared_lock<std::shared_mutex> lock(m_registriesMutex);
            for (auto& pair : m_registriesByType)
            {
                pair.second->Unfreeze();
            }
        }

        m_frozen               = false;
        m_registrationComplete = false; // Allow re-registration
        LogInfo(LogRegisterSubsystem, "UnfreezeAllRegistries: All registries unfrozen");
    }

    bool RegisterSubsystem::AreRegistriesFrozen() const
    {
        return m_frozen;
    }

    bool RegisterSubsystem::IsRegistrationComplete() const
    {
        return m_registrationComplete;
    }

    // ============================================================
    // Registry Instance Management Implementation
    // ============================================================

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

        // Unfreeze all registries before clearing (shutdown phase)
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

        if (m_config.enableNamespaces)
        {
            for (const auto& ns : m_config.defaultNamespaces)
            {
                LogInfo(LogRegisterSubsystem, "Initialized namespace: %s", ns.name.c_str());
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

        LogDebug(LogRegisterSubsystem, "Registration changed event fired");

        return false; // Allow other handlers to process
    }
}
