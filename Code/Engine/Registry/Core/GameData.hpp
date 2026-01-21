#pragma once

// ============================================================================
// GameData.hpp - Unified registry lifecycle management
// Part of Registry System
// Reference: NeoForge GameData.java:38-107
// ============================================================================

#include "Engine/Core/Event/EventBus.hpp"
#include "Engine/Core/Event/EventSubsystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/Engine.hpp"
#include <vector>
#include <functional>
#include <string>

DECLARE_LOG_CATEGORY_EXTERN(LogGameData)

namespace enigma::registry
{
    //-----------------------------------------------------------------------------------------------
    // GameData - Unified Registry Lifecycle Manager
    //
    // DESIGN PHILOSOPHY (Reference: NeoForge GameData.java):
    // - Centralized management of all registry lifecycles
    // - Controls registration order (Block -> Item -> Entity -> ...)
    // - Provides freeze mechanism to prevent late registrations
    // - Single point of control for registration phase
    //
    // LIFECYCLE (Reference: GameData.java:78-107):
    // 1. UnfreezeData() - Prepare registries for registration (optional, for reload)
    // 2. PostRegisterEvents() - Fire RegisterEvent for each registry in order
    // 3. FreezeData() - Lock all registries, prevent further modifications
    //
    // USAGE:
    //   // During game initialization
    //   GameData::PostRegisterEvents();  // Triggers all RegisterEvents
    //   GameData::FreezeData();          // Locks all registries
    //
    // NeoForge Reference:
    //   - GameData.java:65-76 (freezeData)
    //   - GameData.java:78-107 (postRegisterEvents)
    //-----------------------------------------------------------------------------------------------
    class GameData
    {
    public:
        // ========================================================================
        // Registration Phase Control
        // Reference: GameData.java:78-107
        // ========================================================================

        /**
         * @brief Post RegisterEvent for all registries in order
         * 
         * Triggers registration events in the following order:
         * 1. Block
         * 2. Item
         * 3. Entity (future)
         * 4. ... (extensible)
         * 
         * Reference: GameData.java:78-107
         * 
         * @note This should be called once during game initialization
         * @note Uses ModBus from EventSubsystem
         */
        static void PostRegisterEvents();

        /**
         * @brief Post RegisterEvent using a specific EventBus
         * 
         * @param eventBus The event bus to post events to
         */
        static void PostRegisterEvents(enigma::event::EventBus& eventBus);

        // ========================================================================
        // Freeze Mechanism
        // Reference: GameData.java:65-76
        // ========================================================================

        /**
         * @brief Freeze all registries, preventing further registrations
         * 
         * Once frozen:
         * - All Register() calls will throw RegistryFrozenException
         * - Read operations remain available
         * - Model compilation can safely proceed
         * 
         * Reference: GameData.java:65-76
         * 
         * @note Should be called after PostRegisterEvents() completes
         */
        static void FreezeData();

        /**
         * @brief Unfreeze all registries (use with caution)
         * 
         * Reference: GameData.java:60-63
         * 
         * [WARNING] This should only be used for:
         * - Testing scenarios
         * - Hot-reload functionality (future)
         * 
         * In production, registries should remain frozen after initialization.
         */
        static void UnfreezeData();

        // ========================================================================
        // State Query
        // ========================================================================

        /**
         * @brief Check if all registries are frozen
         * @return true if FreezeData() has been called
         */
        static bool IsFrozen();

        /**
         * @brief Check if registration events have been posted
         * @return true if PostRegisterEvents() has been called
         */
        static bool IsRegistrationComplete();

        // ========================================================================
        // Registry Registration (for extensibility)
        // ========================================================================

        /**
         * @brief Register a custom registry's event poster
         * 
         * Allows adding new registry types to the registration order.
         * 
         * @param name Registry name (for logging)
         * @param eventPoster Function that posts the RegisterEvent
         * @param freezer Function that freezes the registry
         * @param unfreezer Function that unfreezes the registry
         * @param isFrozenChecker Function that checks if registry is frozen
         */
        static void RegisterRegistry(
            const std::string&                            name,
            std::function<void(enigma::event::EventBus&)> eventPoster,
            std::function<void()>                         freezer,
            std::function<void()>                         unfreezer,
            std::function<bool()>                         isFrozenChecker
        );

        // ========================================================================
        // Initialization
        // ========================================================================

        /**
         * @brief Initialize GameData with default registries
         * 
         * Registers Block, Item, Entity registries in order.
         * Called automatically on first use, but can be called explicitly.
         */
        static void Initialize();

    private:
        struct RegistryEntry
        {
            std::string                                   name;
            std::function<void(enigma::event::EventBus&)> postEvent;
            std::function<void()>                         freeze;
            std::function<void()>                         unfreeze;
            std::function<bool()>                         isFrozen;
        };

        static std::vector<RegistryEntry>& GetRegistries();
        static bool&                       GetInitializedFlag();
        static bool&                       GetFrozenFlag();
        static bool&                       GetRegistrationCompleteFlag();

        static void EnsureInitialized();
    };
} // namespace enigma::registry
