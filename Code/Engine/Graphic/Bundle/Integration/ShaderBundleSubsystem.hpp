#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderBundleSubsystem.hpp
//
// [REFACTOR] Complete ShaderBundle lifecycle management subsystem
//
// This subsystem provides:
//   - Engine bundle initialization (from .enigma/assets/engine/shaders)
//   - User bundle discovery (from .enigma/shaderpacks)
//   - Load/Unload operations with event firing
//   - SubsystemManager lifecycle integration
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: System-level ShaderBundle lifecycle management
//   - Open/Closed: Extensible via event subscriptions
//   - Dependency Inversion: Uses ShaderBundle abstraction
//
// Ownership Model:
//   - m_engineBundle: shared_ptr (can be referenced by user bundles)
//   - m_currentBundle: shared_ptr (external access via GetCurrentShaderBundle)
//   - Engine bundle always available as fallback
//
// Event Integration:
//   - Fires EVENT_SHADER_BUNDLE_LOADED after successful load
//   - Fires EVENT_SHADER_BUNDLE_UNLOADED after unload
//   - Uses EventSystem::FireEvent() with EventArgs containing bundle name
//
// Usage:
//   // In App.cpp, register after RendererSubsystem
//   subsystemManager.RegisterSubsystem<ShaderBundleSubsystem>(config);
//
//   // Load user bundle
//   auto result = g_theShaderBundleSubsystem->LoadShaderBundle(meta);
//   if (result.success) {
//       auto* program = result.bundle->GetProgram("gbuffers_basic");
//   }
//
//   // Unload returns to engine bundle
//   g_theShaderBundleSubsystem->UnloadShaderBundle();
//
//-----------------------------------------------------------------------------------------------

#include <filesystem>
#include <memory>
#include <optional>

#include "ShaderBundleSubsystemConfiguration.hpp"
#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Event/MulticastDelegate.hpp" // For DelegateHandle
#include "Engine/Graphic/Bundle/Integration/ShaderBundleReloadAdapter.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Reload/RendererFrontendReloadService.hpp"
#include "Engine/Graphic/Reload/RenderPipelineReloadCoordinator.hpp"

namespace enigma::graphic
{
    class ShaderBundleReloadAdapter;

    class ShaderBundleSubsystem : public enigma::core::EngineSubsystem
    {
        DECLARE_SUBSYSTEM(ShaderBundleSubsystem, "ShaderBundleSubsystem", 300)

#pragma region SUBSYSTEM_LIFECYCLE

    public:
        //-------------------------------------------------------------------------------------------
        // Constructor
        //
        // Initialize with configuration containing paths
        //
        // Parameters:
        //   config - Configuration with shaderBundleEnginePath and shaderBundleUserDiscoveryPath
        //-------------------------------------------------------------------------------------------
        explicit ShaderBundleSubsystem(ShaderBundleSubsystemConfiguration config);

        //-------------------------------------------------------------------------------------------
        // Startup
        //
        // [REFACTOR] Complete initialization with engine bundle and discovery
        //
        // Workflow:
        //   1. Load Engine ShaderBundle from .enigma/assets/engine/shaders
        //   2. Set Engine bundle as m_currentBundle initially
        //   3. Discover user ShaderBundles from .enigma/shaderpacks
        //   4. Fire OnShaderBundleLoaded event
        //-------------------------------------------------------------------------------------------
        void Startup() override;

        void Initialize() override;

        //-------------------------------------------------------------------------------------------
        // Shutdown
        //
        // [REFACTOR] Clean shutdown with resource release
        //
        // Workflow:
        //   1. Clear m_currentBundle
        //   2. Clear m_engineBundle
        //   3. Clear discovered list
        //-------------------------------------------------------------------------------------------
        void Shutdown() override;

        //-------------------------------------------------------------------------------------------
        // Update
        //
        // [REFACTOR] Per-frame update - processes pending bundle load/unload requests
        // This ensures RT resource changes happen at frame boundaries, not mid-frame
        //-------------------------------------------------------------------------------------------
        void Update(float deltaTime) override;

#pragma endregion

#pragma region SHADER_BUNDLE_OPERATION

    public:
        //-------------------------------------------------------------------------------------------
        // Configuration file path constant
        // Used for loading/saving ShaderBundle configuration
        //-------------------------------------------------------------------------------------------
        static constexpr const char* CONFIG_FILE_PATH = ".enigma/config/engine/shaderbundle.yml";

        //-------------------------------------------------------------------------------------------
        // ListDiscoveredShaderBundles
        //
        // Get list of all discovered user ShaderBundles
        //
        // Returns:
        //   Vector of ShaderBundleMeta for all discovered bundles in .enigma/shaderpacks
        //-------------------------------------------------------------------------------------------
        std::vector<ShaderBundleMeta> ListDiscoveredShaderBundles();

        //-------------------------------------------------------------------------------------------
        // RefreshDiscoveredShaderBundles
        //
        // Re-scan .enigma/shaderpacks directory and update discovered list
        //
        // Returns:
        //   true if the list changed (bundles added or removed)
        //   false if the list is unchanged
        //-------------------------------------------------------------------------------------------
        bool RefreshDiscoveredShaderBundles();

        //-------------------------------------------------------------------------------------------
        // LoadShaderBundle
        //
        // Load a user ShaderBundle and set as current
        //
        // Parameters:
        //   meta - Metadata of the bundle to load
        //
        // Returns:
        //   ShaderBundleResult with success status, error message, and loaded bundle
        //
        // Workflow:
        //   1. Create ShaderBundle with meta and engine bundle reference
        //   2. Call Initialize() to load and precompile
        //   3. Set as m_currentBundle
        //   4. Fire OnShaderBundleLoaded event
        //
        // Error Handling:
        //   - Catches ShaderBundleException and returns error result
        //   - Does NOT crash on load failure (ERROR_RECOVERABLE)
        //-------------------------------------------------------------------------------------------
        ShaderBundleResult LoadShaderBundle(const ShaderBundleMeta& meta);

        //-------------------------------------------------------------------------------------------
        // UnloadShaderBundle
        //
        // Unload current user bundle and reset to engine bundle
        //
        // Returns:
        //   ShaderBundleResult with success status (always succeeds)
        //
        // Workflow:
        //   1. Reset m_currentBundle to m_engineBundle
        //   2. Fire OnShaderBundleUnloaded event
        //
        // Note: Engine bundle remains loaded, this just resets current to engine default
        //-------------------------------------------------------------------------------------------
        ShaderBundleResult UnloadShaderBundle();

        //-------------------------------------------------------------------------------------------
        // RequestLoadShaderBundle
        //
        // Request a transactional ShaderBundle load through the reload coordinator.
        //
        // Parameters:
        //   meta - Metadata of the bundle to load
        //
        // Returns false when another reload/apply/frame-slot mutation is already active.
        //-------------------------------------------------------------------------------------------
        bool RequestLoadShaderBundle(const ShaderBundleMeta& meta);

        //-------------------------------------------------------------------------------------------
        // RequestUnloadShaderBundle
        //
        // Request a transactional unload back to the engine bundle.
        //
        // Returns false when another reload/apply/frame-slot mutation is already active.
        //-------------------------------------------------------------------------------------------
        bool RequestUnloadShaderBundle();

        //-------------------------------------------------------------------------------------------
        // GetCurrentShaderBundle
        //
        // Get the currently active ShaderBundle
        //
        // Returns:
        //   shared_ptr to current bundle (engine bundle if no user bundle loaded)
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderBundle> GetCurrentShaderBundle() { return m_currentBundle; }
        bool IsCurrentShaderBundle(const ShaderBundleMeta& meta) const;

        //-------------------------------------------------------------------------------------------
        // GetEngineShaderBundle
        //
        // Get the engine default ShaderBundle
        //
        // Returns:
        //   shared_ptr to engine bundle (always available after Startup)
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderBundle> GetEngineShaderBundle() { return m_engineBundle; }

        RenderPipelineReloadCoordinator& GetReloadCoordinator() noexcept { return m_reloadCoordinator; }
        const RenderPipelineReloadCoordinator& GetReloadCoordinator() const noexcept { return m_reloadCoordinator; }

#pragma endregion

    private:
        friend class ShaderBundleReloadAdapter;

        //-------------------------------------------------------------------------------------------
        // DiscoverUserBundles
        //
        // Internal helper to scan .enigma/shaderpacks for valid bundles
        //
        // Workflow:
        //   1. Clear m_discoveredListMeta
        //   2. List subdirectories in shaderBundleUserDiscoveryPath
        //   3. For each subdirectory, check IsValidShaderBundleDirectory
        //   4. If valid, create ShaderBundleMeta and add to list
        //-------------------------------------------------------------------------------------------
        void DiscoverUserBundles();

        ShaderBundleSubsystemConfiguration m_config;

        // [REFACTOR] Current Shader Bundle - shared_ptr for external access
        std::shared_ptr<ShaderBundle> m_currentBundle = nullptr;

        // [REFACTOR] Engine Shader Bundle - shared_ptr so user bundles can reference it
        // Located at .enigma/assets/engine/shaders
        std::shared_ptr<ShaderBundle> m_engineBundle = nullptr;

        // The discovered list of user bundles in .enigma/shaderpacks
        std::vector<ShaderBundleMeta> m_discoveredListMeta;

        // Owns request serialization and renderer lifecycle notifications for
        // transactional shader bundle reloads.
        RenderPipelineReloadCoordinator m_reloadCoordinator;
        std::unique_ptr<ShaderBundleReloadAdapter> m_reloadAdapter;
        std::unique_ptr<RendererFrontendReloadService> m_frontendReloadService;

        // Event subscription handle for MaterialIdMapper vertex event
        enigma::event::DelegateHandle m_onBuildVertexHandle = 0;

    };
}

// Global accessor for ShaderBundleSubsystem
extern enigma::graphic::ShaderBundleSubsystem* g_theShaderBundleSubsystem;
