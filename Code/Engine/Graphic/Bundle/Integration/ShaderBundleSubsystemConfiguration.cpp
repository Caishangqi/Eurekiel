//-----------------------------------------------------------------------------------------------
// ShaderBundleSubsystemConfiguration.cpp
//
// Implementation of ShaderBundleSubsystemConfiguration with YamlConfiguration backing.
//
//-----------------------------------------------------------------------------------------------

#include "ShaderBundleSubsystemConfiguration.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    //-----------------------------------------------------------------------------------------------
    // Constructors
    //-----------------------------------------------------------------------------------------------

    ShaderBundleSubsystemConfiguration::ShaderBundleSubsystemConfiguration()
        : m_yaml()
    {
        // Initialize with default values
        m_yaml.Set(KEY_USER_DISCOVERY_PATH, DEFAULT_USER_DISCOVERY_PATH);
        m_yaml.Set(KEY_ENGINE_PATH, DEFAULT_ENGINE_PATH);
        m_yaml.Set(KEY_CURRENT_LOADED_BUNDLE, DEFAULT_CURRENT_BUNDLE);
    }

    ShaderBundleSubsystemConfiguration::ShaderBundleSubsystemConfiguration(YamlConfiguration yaml)
        : m_yaml(std::move(yaml))
    {
    }

    //-----------------------------------------------------------------------------------------------
    // Static Factory Methods
    //-----------------------------------------------------------------------------------------------

    ShaderBundleSubsystemConfiguration ShaderBundleSubsystemConfiguration::LoadFromYaml(const std::string& yamlPath)
    {
        auto yamlOpt = YamlConfiguration::TryLoadFromFile(yamlPath);

        if (!yamlOpt.has_value())
        {
            LogWarn(LogShaderBundle,
                    "ShaderBundleSubsystemConfiguration:: Failed to load from: %s. Using defaults.",
                    yamlPath.c_str());
            return ShaderBundleSubsystemConfiguration();
        }

        ShaderBundleSubsystemConfiguration config(std::move(yamlOpt.value()));

        LogInfo(LogShaderBundle,
                "ShaderBundleSubsystemConfiguration:: Loaded from %s: userPath=%s, enginePath=%s, currentBundle=%s",
                yamlPath.c_str(),
                config.GetUserDiscoveryPath().c_str(),
                config.GetEnginePath().c_str(),
                config.GetCurrentLoadedBundle().empty() ? "(engine default)" : config.GetCurrentLoadedBundle().c_str());

        return config;
    }

    //-----------------------------------------------------------------------------------------------
    // Accessors
    //-----------------------------------------------------------------------------------------------

    std::string ShaderBundleSubsystemConfiguration::GetUserDiscoveryPath() const
    {
        return m_yaml.GetString(KEY_USER_DISCOVERY_PATH, DEFAULT_USER_DISCOVERY_PATH);
    }

    std::string ShaderBundleSubsystemConfiguration::GetEnginePath() const
    {
        return m_yaml.GetString(KEY_ENGINE_PATH, DEFAULT_ENGINE_PATH);
    }

    std::string ShaderBundleSubsystemConfiguration::GetCurrentLoadedBundle() const
    {
        return m_yaml.GetString(KEY_CURRENT_LOADED_BUNDLE, DEFAULT_CURRENT_BUNDLE);
    }

    //-----------------------------------------------------------------------------------------------
    // Mutators
    //-----------------------------------------------------------------------------------------------

    void ShaderBundleSubsystemConfiguration::SetUserDiscoveryPath(const std::string& path)
    {
        m_yaml.Set(KEY_USER_DISCOVERY_PATH, path);
    }

    void ShaderBundleSubsystemConfiguration::SetEnginePath(const std::string& path)
    {
        m_yaml.Set(KEY_ENGINE_PATH, path);
    }

    void ShaderBundleSubsystemConfiguration::SetCurrentLoadedBundle(const std::string& bundleName)
    {
        m_yaml.Set(KEY_CURRENT_LOADED_BUNDLE, bundleName);
    }

    //-----------------------------------------------------------------------------------------------
    // Persistence
    //-----------------------------------------------------------------------------------------------

    bool ShaderBundleSubsystemConfiguration::SaveToYaml(const std::string& yamlPath) const
    {
        bool success = m_yaml.SaveToFile(yamlPath);

        if (success)
        {
            LogInfo(LogShaderBundle,
                    "ShaderBundleSubsystemConfiguration:: Saved to %s: currentBundle=%s",
                    yamlPath.c_str(),
                    GetCurrentLoadedBundle().empty() ? "(engine default)" : GetCurrentLoadedBundle().c_str());
        }
        else
        {
            LogWarn(LogShaderBundle,
                    "ShaderBundleSubsystemConfiguration:: Failed to save to: %s",
                    yamlPath.c_str());
        }

        return success;
    }
} // namespace enigma::graphic
