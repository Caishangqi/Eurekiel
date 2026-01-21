// ============================================================================
// GameData.cpp - Unified registry lifecycle management implementation
// Part of Registry System
// Reference: NeoForge GameData.java:38-107
// ============================================================================

#include "GameData.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
// #include "Engine/Registry/Item/ItemRegistry.hpp"  // Future
// #include "Engine/Registry/Entity/EntityRegistry.hpp"  // Future

DEFINE_LOG_CATEGORY(LogGameData)

namespace enigma::registry
{
    // ============================================================================
    // Static Storage
    // ============================================================================

    std::vector<GameData::RegistryEntry>& GameData::GetRegistries()
    {
        static std::vector<RegistryEntry> s_registries;
        return s_registries;
    }

    bool& GameData::GetInitializedFlag()
    {
        static bool s_initialized = false;
        return s_initialized;
    }

    bool& GameData::GetFrozenFlag()
    {
        static bool s_frozen = false;
        return s_frozen;
    }

    bool& GameData::GetRegistrationCompleteFlag()
    {
        static bool s_registrationComplete = false;
        return s_registrationComplete;
    }

    // ============================================================================
    // Initialization
    // ============================================================================

    void GameData::Initialize()
    {
        if (GetInitializedFlag())
            return;

        LogInfo(LogGameData, "GameData::Initialize Registering default registries");

        // Register Block registry
        // Reference: NeoForge registers BLOCK first in GameData.java
        RegisterRegistry(
            "Block",
            [](enigma::event::EventBus& bus)
            {
                block::BlockRegistry::FireRegisterEvent(bus);
            },
            []()
            {
                block::BlockRegistry::Freeze();
            },
            []()
            {
                block::BlockRegistry::Unfreeze();
            },
            []()
            {
                return block::BlockRegistry::IsFrozen();
            }
        );

        // Future: Register Item registry
        // RegisterRegistry(
        //     "Item",
        //     [](enigma::event::EventBus& bus) {
        //         item::ItemRegistry::FireRegisterEvent(bus);
        //     },
        //     []() { item::ItemRegistry::Freeze(); },
        //     []() { item::ItemRegistry::Unfreeze(); },
        //     []() { return item::ItemRegistry::IsFrozen(); }
        // );

        // Future: Register Entity registry
        // RegisterRegistry(
        //     "Entity",
        //     [](enigma::event::EventBus& bus) {
        //         entity::EntityRegistry::FireRegisterEvent(bus);
        //     },
        //     []() { entity::EntityRegistry::Freeze(); },
        //     []() { entity::EntityRegistry::Unfreeze(); },
        //     []() { return entity::EntityRegistry::IsFrozen(); }
        // );

        GetInitializedFlag() = true;
        LogInfo(LogGameData, "GameData::Initialize Completed with %zu registries", GetRegistries().size());
    }

    void GameData::EnsureInitialized()
    {
        if (!GetInitializedFlag())
        {
            Initialize();
        }
    }

    // ============================================================================
    // Registration Phase Control
    // Reference: GameData.java:78-107
    // ============================================================================

    void GameData::PostRegisterEvents()
    {
        // Get ModBus from EventSubsystem
        auto* eventSubsystem = GEngine->GetSubsystem<enigma::event::EventSubsystem>();
        if (!eventSubsystem)
        {
            LogError(LogGameData, "GameData::PostRegisterEvents EventSubsystem not available");
            return;
        }

        PostRegisterEvents(eventSubsystem->GetModBus());
    }

    void GameData::PostRegisterEvents(enigma::event::EventBus& eventBus)
    {
        EnsureInitialized();

        if (GetRegistrationCompleteFlag())
        {
            LogWarn(LogGameData, "GameData::PostRegisterEvents Already called, ignoring");
            return;
        }

        LogInfo(LogGameData, "GameData::PostRegisterEvents Starting registration phase");

        // Post RegisterEvent for each registry in order
        // Reference: GameData.java:78-107 iterates through ORDERED_REGISTRIES
        for (const auto& entry : GetRegistries())
        {
            LogInfo(LogGameData, "GameData::PostRegisterEvents Posting RegisterEvent for '%s'", entry.name.c_str());

            try
            {
                entry.postEvent(eventBus);
                LogInfo(LogGameData, "GameData::PostRegisterEvents '%s' registration completed", entry.name.c_str());
            }
            catch (const std::exception& e)
            {
                LogError(LogGameData, "GameData::PostRegisterEvents '%s' registration failed: %s",
                         entry.name.c_str(), e.what());
                throw;
            }
        }

        GetRegistrationCompleteFlag() = true;
        LogInfo(LogGameData, "GameData::PostRegisterEvents Registration phase completed");
    }

    // ============================================================================
    // Freeze Mechanism
    // Reference: GameData.java:65-76
    // ============================================================================

    void GameData::FreezeData()
    {
        EnsureInitialized();

        if (GetFrozenFlag())
        {
            LogWarn(LogGameData, "GameData::FreezeData Already frozen, ignoring");
            return;
        }

        LogInfo(LogGameData, "GameData::FreezeData Freezing all registries");

        // Freeze all registries
        // Reference: GameData.java:65-76 calls freeze() on each MappedRegistry
        for (const auto& entry : GetRegistries())
        {
            LogDebug(LogGameData, "GameData::FreezeData Freezing '%s'", entry.name.c_str());
            entry.freeze();
        }

        GetFrozenFlag() = true;
        LogInfo(LogGameData, "GameData::FreezeData All registries frozen");
    }

    void GameData::UnfreezeData()
    {
        EnsureInitialized();

        if (!GetFrozenFlag())
        {
            LogWarn(LogGameData, "GameData::UnfreezeData Not frozen, ignoring");
            return;
        }

        LogWarn(LogGameData, "GameData::UnfreezeData [WARNING] Unfreezing registries - use with caution!");

        // Unfreeze all registries
        // Reference: GameData.java:60-63
        for (const auto& entry : GetRegistries())
        {
            LogDebug(LogGameData, "GameData::UnfreezeData Unfreezing '%s'", entry.name.c_str());
            entry.unfreeze();
        }

        GetFrozenFlag()               = false;
        GetRegistrationCompleteFlag() = false; // Allow re-registration
        LogInfo(LogGameData, "GameData::UnfreezeData All registries unfrozen");
    }

    // ============================================================================
    // State Query
    // ============================================================================

    bool GameData::IsFrozen()
    {
        return GetFrozenFlag();
    }

    bool GameData::IsRegistrationComplete()
    {
        return GetRegistrationCompleteFlag();
    }

    // ============================================================================
    // Registry Registration
    // ============================================================================

    void GameData::RegisterRegistry(
        const std::string&                            name,
        std::function<void(enigma::event::EventBus&)> eventPoster,
        std::function<void()>                         freezer,
        std::function<void()>                         unfreezer,
        std::function<bool()>                         isFrozenChecker)
    {
        RegistryEntry entry;
        entry.name      = name;
        entry.postEvent = std::move(eventPoster);
        entry.freeze    = std::move(freezer);
        entry.unfreeze  = std::move(unfreezer);
        entry.isFrozen  = std::move(isFrozenChecker);

        GetRegistries().push_back(std::move(entry));

        LogDebug(LogGameData, "GameData::RegisterRegistry Added registry '%s' (order: %zu)",
                 name.c_str(), GetRegistries().size());
    }
} // namespace enigma::registry
