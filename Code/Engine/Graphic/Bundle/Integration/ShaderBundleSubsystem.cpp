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

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"
#include "Engine/Graphic/Bundle/Helper/JsonHelper.hpp"
#include "Engine/Graphic/Bundle/Helper/ShaderBundleFileHelper.hpp"

using namespace enigma::graphic;

// [NEW] Global accessor definition
enigma::graphic::ShaderBundleSubsystem* g_theShaderBundleSubsystem = nullptr;

//-----------------------------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------------------------
using namespace enigma::core;

ShaderBundleSubsystem::ShaderBundleSubsystem(ShaderBundleSubsystemConfiguration& config)
    : m_config(config)
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

    // Step 1: Load Engine ShaderBundle
    ShaderBundleMeta engineMeta;
    engineMeta.name           = "Engine Default";
    engineMeta.author         = "Enigma Engine";
    engineMeta.description    = "Default engine shaders";
    engineMeta.path           = m_config.shaderBundleEnginePath;
    engineMeta.isEngineBundle = true;

    try
    {
        // [RAII] ShaderBundle initializes in constructor
        m_engineBundle = std::make_shared<ShaderBundle>(engineMeta, nullptr);
        LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Engine bundle loaded from: %s", m_config.shaderBundleEnginePath.c_str());
    }
    catch (const ShaderBundleException& e)
    {
        ERROR_AND_DIE(Stringf("ShaderBundleSubsystem:: Failed to load engine bundle: %s", e.what()))
    }

    // Step 2: Set Engine bundle as current initially
    m_currentBundle = m_engineBundle;

    // Step 3: Discover user bundles
    DiscoverUserBundles();

    // Step 4: Fire OnShaderBundleLoaded event
    EventArgs args;
    args.SetValue("bundleName", m_engineBundle->GetName());
    FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);
    g_theShaderBundleSubsystem = this;
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Startup complete. Engine bundle active.");
}

//-----------------------------------------------------------------------------------------------
// Shutdown
//
// [REFACTOR] Clean shutdown with resource release
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Shutdown()
{
    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Shutting down...");

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
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::Update(float deltaTime)
{
    UNUSED(deltaTime)
}

//-----------------------------------------------------------------------------------------------
// DiscoverUserBundles
//
// [NEW] Internal helper to scan .enigma/shaderpacks for valid bundles
//-----------------------------------------------------------------------------------------------
void ShaderBundleSubsystem::DiscoverUserBundles()
{
    m_discoveredListMeta.clear();

    std::filesystem::path userPath = m_config.shaderBundleUserDiscoveryPath;

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
            // [FIX] Parse bundle.json for metadata (name, author, description)
            std::filesystem::path bundleJsonPath = FileSystemHelper::CombinePath(subdir, "shaders/bundle.json");
            auto                  metaOpt        = JsonHelper::ParseBundleJson(bundleJsonPath);

            if (metaOpt)
            {
                // Use parsed metadata from bundle.json
                ShaderBundleMeta meta = metaOpt.value();
                meta.path             = subdir;
                meta.isEngineBundle   = false;

                m_discoveredListMeta.push_back(meta);

                LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Discovered user bundle: %s (by %s) at %s",
                        meta.name.c_str(), meta.author.c_str(), meta.path.string().c_str());
            }
            else
            {
                // Fallback: use directory name if bundle.json parsing fails
                ShaderBundleMeta meta;
                meta.name           = subdir.filename().string();
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
        // [RAII] Create bundle with engine bundle reference for fallback
        // ShaderBundle initializes in constructor (load fallback rules, discover UserDefinedBundles, precompile)
        auto bundle = std::make_shared<ShaderBundle>(meta, m_engineBundle);

        // Set as current bundle
        m_currentBundle = bundle;

        // Fire OnShaderBundleLoaded event
        EventArgs args;
        args.SetValue("bundleName", bundle->GetName());
        FireEvent(EVENT_SHADER_BUNDLE_LOADED, args);

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

    // Reset to engine bundle
    m_currentBundle = m_engineBundle;

    // Fire OnShaderBundleUnloaded event
    EventArgs args;
    args.SetValue("bundleName", previousBundleName);
    FireEvent(EVENT_SHADER_BUNDLE_UNLOADED, args);

    // Always succeeds
    result.success = true;
    result.bundle  = m_engineBundle;

    LogInfo(LogShaderBundle, "ShaderBundleSubsystem:: Unloaded bundle. Reset to engine default.");

    return result;
}
