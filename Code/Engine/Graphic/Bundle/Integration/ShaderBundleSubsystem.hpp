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
#include <optional>

#include "ShaderBundleSubsystemConfiguration.hpp"
#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Event/MulticastDelegate.hpp" // [NEW] For DelegateHandle
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"

namespace enigma::graphic
{
    class ShaderBundleSubsystem : public enigma::core::EngineSubsystem
    {
        DECLARE_SUBSYSTEM(ShaderBundleSubsystem, "ShaderBundleSubsystem", 300)

#pragma region SUBSYSTEM_LIFECYCLE

    public:
        //-------------------------------------------------------------------------------------------
        // Constructor
        //
        // [NEW] Initialize with configuration containing paths
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
        // [NEW] Get list of all discovered user ShaderBundles
        //
        // Returns:
        //   Vector of ShaderBundleMeta for all discovered bundles in .enigma/shaderpacks
        //-------------------------------------------------------------------------------------------
        std::vector<ShaderBundleMeta> ListDiscoveredShaderBundles();

        //-------------------------------------------------------------------------------------------
        // RefreshDiscoveredShaderBundles
        //
        // [NEW] Re-scan .enigma/shaderpacks directory and update discovered list
        //
        // Returns:
        //   true if the list changed (bundles added or removed)
        //   false if the list is unchanged
        //-------------------------------------------------------------------------------------------
        bool RefreshDiscoveredShaderBundles();

        //-------------------------------------------------------------------------------------------
        // LoadShaderBundle
        //
        // [NEW] Load a user ShaderBundle and set as current
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
        // [NEW] Unload current user bundle and reset to engine bundle
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
        // [NEW] Request to load a ShaderBundle at the start of next frame
        // This is the safe way to switch bundles from ImGui or mid-frame code
        //
        // Parameters:
        //   meta - Metadata of the bundle to load
        //
        // Note: The actual load happens in Update() at frame start, avoiding
        //       D3D12 ERROR #924 (render target deleted while in use)
        //-------------------------------------------------------------------------------------------
        void RequestLoadShaderBundle(const ShaderBundleMeta& meta);

        //-------------------------------------------------------------------------------------------
        // RequestUnloadShaderBundle
        //
        // [NEW] Request to unload current bundle at the start of next frame
        // This is the safe way to unload bundles from ImGui or mid-frame code
        //
        // Note: The actual unload happens via OnRendererBeginFrame callback
        //-------------------------------------------------------------------------------------------
        void RequestUnloadShaderBundle();

        //-------------------------------------------------------------------------------------------
        // GetCurrentShaderBundle
        //
        // [NEW] Get the currently active ShaderBundle
        //
        // Returns:
        //   shared_ptr to current bundle (engine bundle if no user bundle loaded)
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderBundle> GetCurrentShaderBundle() { return m_currentBundle; }

        //-------------------------------------------------------------------------------------------
        // GetEngineShaderBundle
        //
        // [NEW] Get the engine default ShaderBundle
        //
        // Returns:
        //   shared_ptr to engine bundle (always available after Startup)
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderBundle> GetEngineShaderBundle() { return m_engineBundle; }

#pragma endregion

    private:
        //-------------------------------------------------------------------------------------------
        // DiscoverUserBundles
        //
        // [NEW] Internal helper to scan .enigma/shaderpacks for valid bundles
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

        // [NEW] Pending request state for deferred bundle switching
        // This ensures RT changes happen at frame boundaries, not mid-frame
        bool                            m_pendingLoad   = false;
        bool                            m_pendingUnload = false;
        std::optional<ShaderBundleMeta> m_pendingMeta   = std::nullopt;

        // [NEW] Event subscription handle for cleanup
        enigma::event::DelegateHandle m_onBeginFrameHandle = 0;

        // [NEW] Event callback - called by RendererEvents::OnBeginFrame
        void OnRendererBeginFrame();
    };
}

// [NEW] Global accessor for ShaderBundleSubsystem
extern enigma::graphic::ShaderBundleSubsystem* g_theShaderBundleSubsystem;
