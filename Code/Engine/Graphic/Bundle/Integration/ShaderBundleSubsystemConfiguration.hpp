#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderBundleSubsystemConfiguration.hpp
//
// Configuration structure for ShaderBundleSubsystem with YAML persistence support.
// Holds a YamlConfiguration object directly for seamless read/write/save operations.
//
// Design:
//   - YamlConfiguration is the single source of truth for all config values
//   - Accessor methods provide type-safe access with default values
//   - Setter methods modify the yaml object directly
//   - SaveToYaml() persists changes to disk
//
// Usage:
//   // Load from YAML file
//   auto config = ShaderBundleSubsystemConfiguration::LoadFromYaml(".enigma/config/engine/shaderbundle.yml");
//
//   // Read values (with defaults if not present)
//   std::string userPath = config.GetUserDiscoveryPath();
//   std::string currentBundle = config.GetCurrentLoadedBundle();
//
//   // Modify and save
//   config.SetCurrentLoadedBundle("MyShaderPack");
//   config.SaveToYaml(".enigma/config/engine/shaderbundle.yml");
//
//-----------------------------------------------------------------------------------------------

#include <string>
#include "Engine/Core/Yaml.hpp"

namespace enigma::graphic
{
    struct ShaderBundleSubsystemConfiguration
    {
        //-------------------------------------------------------------------------------------------
        // YAML Key Constants
        //-------------------------------------------------------------------------------------------
        static constexpr const char* KEY_USER_DISCOVERY_PATH   = "shaderBundleUserDiscoveryPath";
        static constexpr const char* KEY_ENGINE_PATH           = "shaderBundleEnginePath";
        static constexpr const char* KEY_CURRENT_LOADED_BUNDLE = "currentLoadedShaderBundle";

        //-------------------------------------------------------------------------------------------
        // Default Value Constants
        //-------------------------------------------------------------------------------------------
        static constexpr const char* DEFAULT_USER_DISCOVERY_PATH = ".enigma\\shaderbundles";
        static constexpr const char* DEFAULT_ENGINE_PATH         = ".enigma\\assets\\engine\\shaders";
        static constexpr const char* DEFAULT_CURRENT_BUNDLE      = "";

        //-------------------------------------------------------------------------------------------
        // Constructors
        //-------------------------------------------------------------------------------------------

        /** Default constructor - creates config with default values */
        ShaderBundleSubsystemConfiguration();

        /** Construct from existing YamlConfiguration */
        explicit ShaderBundleSubsystemConfiguration(core::YamlConfiguration yaml);

        //-------------------------------------------------------------------------------------------
        // Static Factory Methods
        //-------------------------------------------------------------------------------------------

        /**
         * Load configuration from YAML file
         * If file doesn't exist or parse fails, returns config with default values
         *
         * @param yamlPath Path to the YAML configuration file
         * @return Configuration (always valid, uses defaults if file not found)
         */
        static ShaderBundleSubsystemConfiguration LoadFromYaml(const std::string& yamlPath);

        //-------------------------------------------------------------------------------------------
        // Accessors (Getters with default values)
        //-------------------------------------------------------------------------------------------

        /** Get user ShaderBundle discovery path (e.g., ".enigma/shaderbundles") */
        std::string GetUserDiscoveryPath() const;

        /** Get engine default ShaderBundle path (e.g., ".enigma/assets/engine/shaders") */
        std::string GetEnginePath() const;

        /** Get currently loaded ShaderBundle name (empty = use engine default) */
        std::string GetCurrentLoadedBundle() const;

        //-------------------------------------------------------------------------------------------
        // Mutators (Setters - modify yaml directly)
        //-------------------------------------------------------------------------------------------

        /** Set user ShaderBundle discovery path */
        void SetUserDiscoveryPath(const std::string& path);

        /** Set engine default ShaderBundle path */
        void SetEnginePath(const std::string& path);

        /** Set currently loaded ShaderBundle name (empty = use engine default) */
        void SetCurrentLoadedBundle(const std::string& bundleName);

        //-------------------------------------------------------------------------------------------
        // Persistence
        //-------------------------------------------------------------------------------------------

        /**
         * Save configuration to YAML file
         *
         * @param yamlPath Path to save the YAML configuration file
         * @return true if save successful, false otherwise
         */
        bool SaveToYaml(const std::string& yamlPath) const;

        //-------------------------------------------------------------------------------------------
        // Direct YAML Access (for advanced usage)
        //-------------------------------------------------------------------------------------------

        /** Get underlying YamlConfiguration (const) */
        const core::YamlConfiguration& GetYaml() const { return m_yaml; }

        /** Get underlying YamlConfiguration (mutable) */
        core::YamlConfiguration& GetYaml() { return m_yaml; }

    private:
        core::YamlConfiguration m_yaml;
    };
}
