//----------------------------------------------------------------------------------------------
// ShaderBundleSubsystem.cpp
//
// [REFACTOR] Complete implementation of ShaderBundle lifecycle management
//
// Implementation Notes:
//   - Engine bundle loaded first in Startup(), always available
//   - User bundles discovered from .enigma/shaderpacks
//   - Events fired after successful operations
//   - Exception handling with ERROR_RECOVERABLE for graceful degradation
//
//-----------------------------------------------------------------------------------------------

#include "ShaderBundleSubsystem.hpp"

#include <utility>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Event/EventSubsystem.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"
#include "Engine/Graphic/Bundle/Helper/JsonHelper.hpp"
#include "Engine/Graphic/Bundle/Helper/ShaderBundleFileHelper.hpp"
#include "Engine/Graphic/Bundle/Imgui/ImguiShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleReloadAdapter.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleEvents.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Reload/RendererFrontendReloadService.hpp"
#include "Engine/Graphic/Bundle/Directive/PackRenderTargetDirectives.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"
#include "Engine/Graphic/Target/ShadowColorProvider.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp" // For OnBuildVertexLayout event

using namespace enigma::graphic;

// Global accessor definition
enigma::graphic::ShaderBundleSubsystem* g_theShaderBundleSubsystem = nullptr;

//-----------------------------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------------------------
using namespace enigma::core;

ShaderBundleSubsystem::ShaderBundleSubsystem(ShaderBundleSubsystemConfiguration config)
    : m_config(std::move(config))
{
}

//-----------------------------------------------------------------------------------------------
// Startup
//
// [REFACTOR] Complete initialization with engine bundle and discovery
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Startup()
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Starting up...");

    // Step 1: Load Engine ShaderBundle metadata from bundle.json
    auto engineMetaOpt = ShaderBundleMeta::FromBundlePath(m_config.GetEnginePath(), true);

    if (!engineMetaOpt)
    {
        ERROR_AND_DIE("Can not find Engine Builtin shader bundle meta.")
    }

    ShaderBundleMeta engineMeta = engineMetaOpt.value();

    // Get path aliases from configuration for shader include resolution
    auto pathAliases = m_config.GetPathAliasMap();

    try
    {
        // [RAII] ShaderBundle initializes in constructor
        m_engineBundle = std::make_shared<ShaderBundle>(engineMeta, nullptr, pathAliases);
        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Engine bundle '%s' loaded from: %s",
                engineMeta.name.c_str(), m_config.GetEnginePath().c_str());
    }
    catch (const ShaderBundleException& e)
    {
        ERROR_AND_DIE(Stringf("ShaderBundleSubsystem:: Failed to load engine bundle: %s", e.what()))
    }

    // Step 2: Set Engine bundle as current initially
    m_currentBundle = m_engineBundle;

    // Step 2.5: Apply engine bundle's RT directives to providers
    // Same pattern as LoadShaderBundle() - ensures textures are recreated with
    // correct optimizedClearValue, enabling DX12 Fast Clear and eliminating
    // CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE / CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE warnings
    auto* rtDirectives = m_engineBundle->GetRTDirectives();
    if (rtDirectives && g_theRendererSubsystem)
    {
        auto* colorProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ColorTex);
        auto* depthProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex);
        auto* shadowColorProvider = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowColor);
        auto* shadowTexProvider   = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex);

        if (colorProvider)
        {
            for (int i = 0; i <= rtDirectives->GetMaxColorTexIndex(); ++i)
            {
                if (rtDirectives->HasColorTexConfig(i))
                    colorProvider->SetRtConfig(i, rtDirectives->GetColorTexConfig(i));
            }
        }
        if (depthProvider)
        {
            for (int i = 0; i <= rtDirectives->GetMaxDepthTexIndex(); ++i)
            {
                if (rtDirectives->HasDepthTexConfig(i))
                    depthProvider->SetRtConfig(i, rtDirectives->GetDepthTexConfig(i));
            }
        }
        if (shadowColorProvider)
        {
            for (int i = 0; i <= rtDirectives->GetMaxShadowColorIndex(); ++i)
            {
                if (rtDirectives->HasShadowColorConfig(i))
                    shadowColorProvider->SetRtConfig(i, rtDirectives->GetShadowColorConfig(i));
            }
        }
        if (shadowTexProvider)
        {
            for (int i = 0; i <= rtDirectives->GetMaxShadowTexIndex(); ++i)
            {
                if (rtDirectives->HasShadowTexConfig(i))
                    shadowTexProvider->SetRtConfig(i, rtDirectives->GetShadowTexConfig(i));
            }
        }

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Applied engine bundle RT directives to providers");
    }

    // Step 3: Discover user bundles
    DiscoverUserBundles();

    // Step 4: [AUTO-LOAD] Check if there's a previously loaded bundle to restore
    std::string savedBundleName = m_config.GetCurrentLoadedBundle();
    if (!savedBundleName.empty())
    {
        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Found saved bundle to restore: %s", savedBundleName.c_str());

        // Find the bundle in discovered list by name
        bool bundleFound = false;
        for (const auto& meta : m_discoveredListMeta)
        {
            if (meta.name == savedBundleName)
            {
                // Try to load the saved bundle
                ShaderBundleResult result = LoadShaderBundle(meta);
                if (result.success)
                {
                    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Successfully restored bundle: %s", savedBundleName.c_str());
                    bundleFound = true;
                }
                else
                {
                    LogWarn(LogShaderBundle, "ShaderBundleSubsystem:: Failed to restore bundle '%s': %s. Using engine default.",
                            savedBundleName.c_str(), result.errorMessage.c_str());
                    // Clear the saved bundle name since it failed to load
                    m_config.SetCurrentLoadedBundle("");
                    m_config.SaveToYaml(CONFIG_FILE_PATH);
                }
                break;
            }
        }

        if (!bundleFound && !savedBundleName.empty())
        {
            LogWarn(LogShaderBundle, "ShaderBundleSubsystem:: Saved bundle '%s' not found in discovered bundles. Using engine default.",
                    savedBundleName.c_str());
            // Clear the saved bundle name since it's no longer available
            m_config.SetCurrentLoadedBundle("");
            m_config.SaveToYaml(CONFIG_FILE_PATH);
        }
    }

    // Step 5: Fire committed engine-bundle startup events.
    // Only fire if we're still using engine bundle (LoadShaderBundle already fires events).
    if (m_currentBundle == m_engineBundle)
    {
        EventArgs args;
        args.SetValue("bundleName", m_engineBundle->GetName());
        FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);

        // Step 6: Broadcast committed lifecycle event.
        ShaderBundleEvents::OnBundleLoaded.Broadcast(m_engineBundle.get());
    }

    if (g_theImGui)
    {
        g_theImGui->RegisterWindow("ShaderBundle", [this]() { ImguiShaderBundle::Show(this); });
    }

    m_reloadAdapter = std::make_unique<ShaderBundleReloadAdapter>(*this);
    if (g_theRendererSubsystem)
    {
        m_frontendReloadService = std::make_unique<RendererFrontendReloadService>(*g_theRendererSubsystem);
    }
    m_reloadCoordinator.SetServices(m_reloadAdapter.get(), m_frontendReloadService.get());
    m_reloadCoordinator.SubscribeRendererEvents();
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Subscribed reload coordinator to renderer frame lifecycle events");

    g_theShaderBundleSubsystem = this;
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Startup complete. %s bundle active.", m_currentBundle == m_engineBundle ? "Engine" : m_currentBundle->GetName().c_str());
}

void ShaderBundleSubsystem::Initialize()
{
}

//-----------------------------------------------------------------------------------------------
// Shutdown
//
// [REFACTOR] Clean shutdown with resource release
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Shutdown()
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Shutting down...");

    m_reloadCoordinator.UnsubscribeRendererEvents();
    m_reloadCoordinator.SetServices(nullptr, nullptr);
    m_frontendReloadService.reset();
    m_reloadAdapter.reset();
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unsubscribed reload coordinator from renderer frame lifecycle events");

    // Remove MaterialIdMapper subscription
    if (m_onBuildVertexHandle != 0)
    {
        TerrainVertexLayout::OnBuildVertexLayout.Remove(m_onBuildVertexHandle);
        m_onBuildVertexHandle = 0;
    }

    // Clear current bundle reference
    m_currentBundle.reset();

    // Clear engine bundle
    m_engineBundle.reset();

    // Clear discovered list
    m_discoveredListMeta.clear();

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Shutdown complete.");
}

//-----------------------------------------------------------------------------------------------
// Update
//
// [REFACTOR] Per-frame update - pending requests are resolved across the renderer
// lifecycle callbacks so queue synchronization stays pre-frame and resource
// mutation stays post-acquire.
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Update(float deltaTime)
{
    UNUSED(deltaTime)
    // Transactional reload requests are driven by RenderPipelineReloadCoordinator.
}

//-----------------------------------------------------------------------------------------------
// DiscoverUserBundles
//
// [REFACTOR] Internal helper to scan .enigma/shaderbundles for valid bundles
// Uses ShaderBundleMeta::FromBundlePath for consistent metadata loading
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::DiscoverUserBundles()
{
    m_discoveredListMeta.clear();

    std::filesystem::path userPath = m_config.GetUserDiscoveryPath();

    // Check if discovery path exists
    if (!FileSystemHelper::DirectoryExists(userPath))
    {
        LogWarn(LogShaderBundle, "ShaderBundleSubsystem:: User bundle discovery path not found: %s",
                userPath.string().c_str());
        return;
    }

    // List all subdirectories
    auto subdirs = FileSystemHelper::ListSubdirectories(userPath);

    for (const auto& subdir : subdirs)
    {
        // Validate each subdirectory as a potential ShaderBundle
        if (ShaderBundleFileHelper::IsValidShaderBundleDirectory(subdir))
        {
            // [REFACTOR] Use ShaderBundleMeta::FromBundlePath for consistent metadata loading
            auto metaOpt = ShaderBundleMeta::FromBundlePath(subdir, false);

            if (metaOpt)
            {
                // Use parsed metadata from bundle.json
                m_discoveredListMeta.push_back(metaOpt.value());

                LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Discovered user bundle: %s (by %s) at %s",
                        metaOpt->name.c_str(), metaOpt->author.c_str(), metaOpt->path.string().c_str());
            }
            else
            {
                // Fallback: use directory name if bundle.json parsing fails
                ShaderBundleMeta meta;
                meta.name           = subdir.filename().string();
                meta.author         = "Unknown";
                meta.description    = "";
                meta.path           = subdir;
                meta.isEngineBundle = false;

                m_discoveredListMeta.push_back(meta);

                LogWarn(LogShaderBundle, "ShaderBundleSubsystem:: Failed to parse bundle.json, using directory name: %s",
                        meta.name.c_str());
            }
        }
    }

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Discovered %zu user bundles",
            m_discoveredListMeta.size());
}

//-----------------------------------------------------------------------------------------------
// ListDiscoveredShaderBundles
//
// Get list of all discovered user ShaderBundles
//-----------------------------------------------------------------------------------------------
std::vector<ShaderBundleMeta> ShaderBundleSubsystem::ListDiscoveredShaderBundles()
{
    return m_discoveredListMeta;
}

//-----------------------------------------------------------------------------------------------
// RefreshDiscoveredShaderBundles
//
// Re-scan .enigma/shaderpacks directory and update discovered list
//-----------------------------------------------------------------------------------------------
bool ShaderBundleSubsystem::RefreshDiscoveredShaderBundles()
{
    // Store old list for comparison
    std::vector<ShaderBundleMeta> oldList = m_discoveredListMeta;

    // Re-discover
    DiscoverUserBundles();

    // Check if list changed (simple size comparison + name comparison)
    if (oldList.size() != m_discoveredListMeta.size())
    {
        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Bundle list changed: %zu -> %zu",
                oldList.size(), m_discoveredListMeta.size());
        return true;
    }

    // Compare names
    for (size_t i = 0; i < oldList.size(); ++i)
    {
        if (oldList[i].name != m_discoveredListMeta[i].name ||
            oldList[i].path != m_discoveredListMeta[i].path)
        {
            LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Bundle list content changed");
            return true;
        }
    }

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Bundle list unchanged");
    return false;
}

//-----------------------------------------------------------------------------------------------
// LoadShaderBundle
//
// Load a user ShaderBundle and set as current
//-----------------------------------------------------------------------------------------------
ShaderBundleResult ShaderBundleSubsystem::LoadShaderBundle(const ShaderBundleMeta& meta)
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Loading bundle: %s from %s",
            meta.name.c_str(), meta.path.string().c_str());

    ShaderBundleResult result;

    try
    {
        ShaderBundleReloadAdapter adapter(*this);
        auto prepared = adapter.PrepareLoad(meta);

        if (g_theRendererSubsystem)
        {
            RendererFrontendReloadService frontendReloadService(*g_theRendererSubsystem);
            frontendReloadService.ApplyReloadScope(adapter.BuildFrontendReloadScope(prepared));
        }

        adapter.ApplyPreparedBundle(std::move(prepared));

        // Fire committed bundle-loaded event.
        EventArgs args;
        args.SetValue("bundleName", m_currentBundle->GetName());
        FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);

        // Broadcast committed lifecycle event.
        ShaderBundleEvents::OnBundleLoaded.Broadcast(m_currentBundle.get());

        // [AUTO-SAVE] Save current loaded bundle name to config file
        m_config.SetCurrentLoadedBundle(meta.name);
        m_config.SaveToYaml(CONFIG_FILE_PATH);

        // Return success
        result.success = true;
        result.bundle  = m_currentBundle;

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Bundle loaded successfully: %s",
                meta.name.c_str());
    }
    catch (const ShaderBundleException& e)
    {
        // Log error but do not crash
        ERROR_RECOVERABLE(Stringf("ShaderBundleSubsystem:: Failed to load bundle '%s': %s", meta.name.c_str(), e.what()))

        result.success      = false;
        result.errorMessage = e.what();
        result.bundle       = nullptr;
    }
    catch (const std::exception& e)
    {
        ERROR_RECOVERABLE(Stringf("ShaderBundleSubsystem:: Failed to apply bundle '%s': %s", meta.name.c_str(), e.what()))

        result.success      = false;
        result.errorMessage = e.what();
        result.bundle       = nullptr;
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// UnloadShaderBundle
//
// Unload current user bundle and reset to engine bundle
//-----------------------------------------------------------------------------------------------
ShaderBundleResult ShaderBundleSubsystem::UnloadShaderBundle()
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unloading current bundle...");

    ShaderBundleResult result;

    // Get current bundle name for event (before reset)
    std::string previousBundleName;
    if (m_currentBundle)
    {
        previousBundleName = m_currentBundle->GetName();
    }

    try
    {
        ShaderBundleReloadAdapter adapter(*this);
        auto prepared = adapter.PrepareUnloadToEngine();

        if (g_theRendererSubsystem)
        {
            RendererFrontendReloadService frontendReloadService(*g_theRendererSubsystem);
            frontendReloadService.ApplyReloadScope(adapter.BuildFrontendReloadScope(prepared));
        }

        adapter.ApplyPreparedBundle(std::move(prepared));

        // Broadcast MulticastDelegate unload event after the engine bundle is stable.
        ShaderBundleEvents::OnBundleUnloaded.Broadcast();

        // Fire committed bundle-unloaded event.
        EventArgs args;
        args.SetValue("bundleName", previousBundleName);
        FireEvent(EVENT_SHADER_BUNDLE_UNLOADED, args);

        // Broadcast MulticastDelegate load event (engine bundle now active)
        ShaderBundleEvents::OnBundleLoaded.Broadcast(m_engineBundle.get());

        // [AUTO-SAVE] Clear current loaded bundle name and save to config file
        m_config.SetCurrentLoadedBundle("");
        m_config.SaveToYaml(CONFIG_FILE_PATH);

        result.success = true;
        result.bundle  = m_engineBundle;

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unloaded bundle. Reset to engine default.");
    }
    catch (const std::exception& e)
    {
        ERROR_RECOVERABLE(Stringf("ShaderBundleSubsystem:: Failed to unload bundle: %s", e.what()))

        result.success      = false;
        result.errorMessage = e.what();
        result.bundle       = m_currentBundle;
    }

    return result;
}

//-----------------------------------------------------------------------------------------------
// RequestLoadShaderBundle
//
// Request to load a ShaderBundle at the start of next frame
// This is the safe way to switch bundles from ImGui or mid-frame code
//-----------------------------------------------------------------------------------------------
bool ShaderBundleSubsystem::RequestLoadShaderBundle(const ShaderBundleMeta& meta)
{
    const bool accepted = m_reloadCoordinator.RequestLoadShaderBundle(meta);
    if (!accepted)
    {
        LogWarn(LogShaderBundle,
                "ShaderBundleSubsystem:: Ignored load request for '%s': %s",
                meta.name.c_str(),
                ToString(m_reloadCoordinator.GetRequestGate().GetLastIgnoredReason()));
        return false;
    }

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Accepted reload load request for: %s",
            meta.name.c_str());
    return true;
}

bool ShaderBundleSubsystem::IsCurrentShaderBundle(const ShaderBundleMeta& meta) const
{
    return m_currentBundle != nullptr && IsSameShaderBundleMeta(m_currentBundle->GetMeta(), meta);
}

//-----------------------------------------------------------------------------------------------
// RequestUnloadShaderBundle
//
// Request to unload current bundle at the start of next frame
// This is the safe way to unload bundles from ImGui or mid-frame code
//-----------------------------------------------------------------------------------------------
bool ShaderBundleSubsystem::RequestUnloadShaderBundle()
{
    const bool accepted = m_reloadCoordinator.RequestUnloadShaderBundle();
    if (!accepted)
    {
        LogWarn(LogShaderBundle,
                "ShaderBundleSubsystem:: Ignored unload request: %s",
                ToString(m_reloadCoordinator.GetRequestGate().GetLastIgnoredReason()));
        return false;
    }

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Accepted reload unload request");
    return true;
}
