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
#include "Engine/Graphic/Bundle/ShaderBundleEvents.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Integration/RendererEvents.hpp" // [NEW] For OnBeginFrame subscription
#include "Engine/Graphic/Bundle/Directive/PackRenderTargetDirectives.hpp"

using namespace enigma::graphic;

// [NEW] Global accessor definition
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

    // [NEW] Get path aliases from configuration for shader include resolution
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

    // Step 5: Fire OnShaderBundleLoaded event (EventSystem for backward compatibility)
    // Only fire if we're still using engine bundle (LoadShaderBundle already fires events)
    if (m_currentBundle == m_engineBundle)
    {
        EventArgs args;
        args.SetValue("bundleName", m_engineBundle->GetName());
        FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);

        // Step 6: Broadcast MulticastDelegate event
        ShaderBundleEvents::OnBundleLoaded.Broadcast(m_engineBundle.get());
    }

    if (g_theImGui)
    {
        g_theImGui->RegisterWindow("ShaderBundle", [this]() { ImguiShaderBundle::Show(this); });
    }

    // [NEW] Subscribe to RendererEvents::OnBeginFrame for deferred bundle switching
    // This ensures RT changes happen at frame boundaries (GPU idle), avoiding D3D12 ERROR #924
    m_onBeginFrameHandle = RendererEvents::OnBeginFrame.Add(this, &ShaderBundleSubsystem::OnRendererBeginFrame);
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Subscribed to OnBeginFrame event");

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

    // [NEW] Unsubscribe from RendererEvents::OnBeginFrame
    if (m_onBeginFrameHandle != 0)
    {
        RendererEvents::OnBeginFrame.Remove(m_onBeginFrameHandle);
        m_onBeginFrameHandle = 0;
        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unsubscribed from OnBeginFrame event");
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
// [REFACTOR] Per-frame update - pending requests now handled via OnBeginFrame event
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Update(float deltaTime)
{
    UNUSED(deltaTime)
    // [NOTE] Pending requests are now processed via RendererEvents::OnBeginFrame callback
    // This ensures RT changes happen after previous frame completes but before
    // new frame's CommandList starts recording
}

//-----------------------------------------------------------------------------------------------
// OnRendererBeginFrame
//
// [NEW] Event callback for RendererEvents::OnBeginFrame
// Called at the very beginning of each frame when GPU is idle
// Safe to modify RT resources here without causing D3D12 ERROR #924
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::OnRendererBeginFrame()
{
    // Skip if no pending requests
    if (!m_pendingLoad && !m_pendingUnload)
    {
        return;
    }

    // Process pending unload request first (if both are set, unload takes precedence)
    if (m_pendingUnload)
    {
        m_pendingUnload = false;
        m_pendingLoad   = false; // Cancel any pending load
        m_pendingMeta   = std::nullopt;

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Processing deferred unload request");
        UnloadShaderBundle();
        return;
    }

    // Process pending load request
    if (m_pendingLoad && m_pendingMeta.has_value())
    {
        m_pendingLoad         = false;
        ShaderBundleMeta meta = m_pendingMeta.value();
        m_pendingMeta         = std::nullopt;

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Processing deferred load request for: %s", meta.name.c_str());
        LoadShaderBundle(meta);
    }
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
// [NEW] Get list of all discovered user ShaderBundles
//-----------------------------------------------------------------------------------------------
std::vector<ShaderBundleMeta> ShaderBundleSubsystem::ListDiscoveredShaderBundles()
{
    return m_discoveredListMeta;
}

//-----------------------------------------------------------------------------------------------
// RefreshDiscoveredShaderBundles
//
// [NEW] Re-scan .enigma/shaderpacks directory and update discovered list
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
// [NEW] Load a user ShaderBundle and set as current
//-----------------------------------------------------------------------------------------------
ShaderBundleResult ShaderBundleSubsystem::LoadShaderBundle(const ShaderBundleMeta& meta)
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Loading bundle: %s from %s",
            meta.name.c_str(), meta.path.string().c_str());

    ShaderBundleResult result;

    try
    {
        // [NEW] Get path aliases from configuration for shader include resolution
        auto pathAliases = m_config.GetPathAliasMap();

        // [RAII] Create bundle with engine bundle reference for fallback
        // ShaderBundle initializes in constructor (load fallback rules, discover UserDefinedBundles, precompile)
        auto bundle = std::make_shared<ShaderBundle>(meta, m_engineBundle, pathAliases);

        // [NEW] Apply RT configs from bundle directives to providers
        auto* rtDirectives = bundle->GetRTDirectives();
        if (rtDirectives && g_theRendererSubsystem)
        {
            auto* colorProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ColorTex);
            auto* depthProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex);
            auto* shadowColorProvider = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowColor);
            auto* shadowTexProvider   = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex);

            // Apply colortex configs
            if (colorProvider)
            {
                for (int i = 0; i <= rtDirectives->GetMaxColorTexIndex(); ++i)
                {
                    if (rtDirectives->HasColorTexConfig(i))
                    {
                        colorProvider->SetRtConfig(i, rtDirectives->GetColorTexConfig(i));
                    }
                }
            }

            // Apply depthtex configs
            if (depthProvider)
            {
                for (int i = 0; i <= rtDirectives->GetMaxDepthTexIndex(); ++i)
                {
                    if (rtDirectives->HasDepthTexConfig(i))
                    {
                        depthProvider->SetRtConfig(i, rtDirectives->GetDepthTexConfig(i));
                    }
                }
            }

            // Apply shadowcolor configs
            if (shadowColorProvider)
            {
                for (int i = 0; i <= rtDirectives->GetMaxShadowColorIndex(); ++i)
                {
                    if (rtDirectives->HasShadowColorConfig(i))
                    {
                        shadowColorProvider->SetRtConfig(i, rtDirectives->GetShadowColorConfig(i));
                    }
                }
            }

            // Apply shadowtex configs
            if (shadowTexProvider)
            {
                for (int i = 0; i <= rtDirectives->GetMaxShadowTexIndex(); ++i)
                {
                    if (rtDirectives->HasShadowTexConfig(i))
                    {
                        shadowTexProvider->SetRtConfig(i, rtDirectives->GetShadowTexConfig(i));
                    }
                }
            }

            LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Applied RT configs from bundle directives");
        }

        // Set as current bundle
        m_currentBundle = bundle;

        // Fire OnShaderBundleLoaded event (EventSystem for backward compatibility)
        EventArgs args;
        args.SetValue("bundleName", bundle->GetName());
        FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);

        // Broadcast MulticastDelegate event
        ShaderBundleEvents::OnBundleLoaded.Broadcast(m_currentBundle.get());

        // [AUTO-SAVE] Save current loaded bundle name to config file
        m_config.SetCurrentLoadedBundle(meta.name);
        m_config.SaveToYaml(CONFIG_FILE_PATH);

        // Return success
        result.success = true;
        result.bundle  = bundle;

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

    return result;
}

//-----------------------------------------------------------------------------------------------
// UnloadShaderBundle
//
// [NEW] Unload current user bundle and reset to engine bundle
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

    // Broadcast MulticastDelegate unload event (before reset)
    ShaderBundleEvents::OnBundleUnloaded.Broadcast();

    // [NEW] Reset RT configs to engine defaults before switching bundles
    if (g_theRendererSubsystem)
    {
        const auto& config = g_theRendererSubsystem->GetConfiguration();

        auto* colorProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ColorTex);
        auto* depthProvider       = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::DepthTex);
        auto* shadowColorProvider = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowColor);
        auto* shadowTexProvider   = g_theRendererSubsystem->GetRenderTargetProvider(RenderTargetType::ShadowTex);

        if (colorProvider) colorProvider->ResetToDefault(config.GetColorTexConfigs());
        if (depthProvider) depthProvider->ResetToDefault(config.GetDepthTexConfigs());
        if (shadowColorProvider) shadowColorProvider->ResetToDefault(config.GetShadowColorConfigs());
        if (shadowTexProvider) shadowTexProvider->ResetToDefault(config.GetShadowTexConfigs());

        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Reset RT configs to engine defaults");
    }

    // Reset to engine bundle
    m_currentBundle = m_engineBundle;

    // Fire OnShaderBundleUnloaded event (EventSystem for backward compatibility)
    EventArgs args;
    args.SetValue("bundleName", previousBundleName);
    FireEvent(EVENT_SHADER_BUNDLE_UNLOADED, args);

    // Broadcast MulticastDelegate load event (engine bundle now active)
    ShaderBundleEvents::OnBundleLoaded.Broadcast(m_engineBundle.get());

    // [AUTO-SAVE] Clear current loaded bundle name and save to config file
    m_config.SetCurrentLoadedBundle("");
    m_config.SaveToYaml(CONFIG_FILE_PATH);

    // Always succeeds
    result.success = true;
    result.bundle  = m_engineBundle;

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unloaded bundle. Reset to engine default.");

    return result;
}

//-----------------------------------------------------------------------------------------------
// RequestLoadShaderBundle
//
// [NEW] Request to load a ShaderBundle at the start of next frame
// This is the safe way to switch bundles from ImGui or mid-frame code
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::RequestLoadShaderBundle(const ShaderBundleMeta& meta)
{
    m_pendingLoad   = true;
    m_pendingUnload = false; // Cancel any pending unload
    m_pendingMeta   = meta;

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Queued load request for: %s (will execute next frame)",
            meta.name.c_str());
}

//-----------------------------------------------------------------------------------------------
// RequestUnloadShaderBundle
//
// [NEW] Request to unload current bundle at the start of next frame
// This is the safe way to unload bundles from ImGui or mid-frame code
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::RequestUnloadShaderBundle()
{
    m_pendingUnload = true;
    m_pendingLoad   = false; // Cancel any pending load
    m_pendingMeta   = std::nullopt;

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Queued unload request (will execute next frame)");
}
