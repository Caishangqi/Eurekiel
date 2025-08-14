#pragma once
#pragma warning(push)
#pragma warning(disable: 4244 4275 4251 4702 4217)
#include <ThirdParty/yaml-cpp/yaml.h>
#pragma warning(pop)

#if defined(_DEBUG)
#pragma comment(lib, "ThirdParty/yaml-cpp/yaml-cppd.lib")
#else
#pragma comment(lib, "ThirdParty/yaml-cpp/yaml-cpp.lib")
#endif
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace enigma::core
{
    /**
     * YamlConfiguration - YAML configuration wrapper inspired by SpigotAPI
     * Provides hierarchical configuration management with path-based access
     * Supports loading, saving, and manipulation of YAML configuration files
     */
    class YamlConfiguration
    {
    public:
        // Constructors
        YamlConfiguration();
        explicit YamlConfiguration(const std::string& yamlString);
        explicit YamlConfiguration(const YAML::Node& node);
        YamlConfiguration(const YamlConfiguration& other);
        YamlConfiguration(YamlConfiguration&& other) noexcept;

        // Assignment operators
        YamlConfiguration& operator=(const YamlConfiguration& other);
        YamlConfiguration& operator=(YamlConfiguration&& other) noexcept;

        // Static factory methods
        static YamlConfiguration                Parse(const std::string& yamlString);
        static YamlConfiguration                LoadFromFile(const std::string& filePath);
        static std::optional<YamlConfiguration> TryParse(const std::string& yamlString);
        static std::optional<YamlConfiguration> TryLoadFromFile(const std::string& filePath);

        // File operations (SpigotAPI-style)
        bool SaveToFile(const std::string& filePath) const;
        bool ReloadFromFile(const std::string& filePath);
        bool SaveDefaultConfig(const std::string& filePath) const;

        // Type checking
        bool IsNull() const;
        bool IsMap() const;
        bool IsSequence() const;
        bool IsScalar() const;
        bool IsEmpty() const;

        // Path-based access (SpigotAPI-style with dot notation)
        bool Contains(const std::string& path) const;
        bool IsSet(const std::string& path) const;

        // Getters with default values (SpigotAPI-style)
        std::string GetString(const std::string& path, const std::string& defaultValue = "") const;
        int         GetInt(const std::string& path, int defaultValue = 0) const;
        long        GetLong(const std::string& path, long defaultValue = 0) const;
        float       GetFloat(const std::string& path, float defaultValue = 0.0f) const;
        double      GetDouble(const std::string& path, double defaultValue = 0.0) const;
        bool        GetBoolean(const std::string& path, bool defaultValue = false) const;

        // List getters
        std::vector<std::string>       GetStringList(const std::string& path) const;
        std::vector<int>               GetIntList(const std::string& path) const;
        std::vector<float>             GetFloatList(const std::string& path) const;
        std::vector<bool>              GetBooleanList(const std::string& path) const;
        std::vector<YamlConfiguration> GetConfigurationList(const std::string& path) const;

        // Optional getters (return std::nullopt if path doesn't exist)
        std::optional<std::string> GetStringOpt(const std::string& path) const;
        std::optional<int>         GetIntOpt(const std::string& path) const;
        std::optional<bool>        GetBooleanOpt(const std::string& path) const;
        std::optional<double>      GetDoubleOpt(const std::string& path) const;

        // Setters (SpigotAPI-style)
        YamlConfiguration& Set(const std::string& path, const std::string& value);
        YamlConfiguration& Set(const std::string& path, const char* value);
        YamlConfiguration& Set(const std::string& path, int value);
        YamlConfiguration& Set(const std::string& path, long value);
        YamlConfiguration& Set(const std::string& path, float value);
        YamlConfiguration& Set(const std::string& path, double value);
        YamlConfiguration& Set(const std::string& path, bool value);
        YamlConfiguration& Set(const std::string& path, const YamlConfiguration& value);
        YamlConfiguration& Set(const std::string& path, const std::vector<std::string>& value);
        YamlConfiguration& Set(const std::string& path, const std::vector<int>& value);
        YamlConfiguration& Set(const std::string& path, const std::vector<YamlConfiguration>& value);

        // Configuration section access
        YamlConfiguration GetConfigurationSection(const std::string& path) const;
        YamlConfiguration CreateSection(const std::string& path);

        // Key operations
        std::vector<std::string> GetKeys() const;
        std::vector<std::string> GetKeys(const std::string& path) const;
        std::vector<std::string> GetKeys(bool deep) const;
        std::vector<std::string> GetKeys(const std::string& path, bool deep) const;

        // Remove operations
        YamlConfiguration& Remove(const std::string& path);
        YamlConfiguration& Clear();

        // Default configuration management
        YamlConfiguration& AddDefault(const std::string& path, const std::string& value);
        YamlConfiguration& AddDefault(const std::string& path, int value);
        YamlConfiguration& AddDefault(const std::string& path, bool value);
        YamlConfiguration& AddDefaults(const YamlConfiguration& defaults);
        void               SetDefaults(const YamlConfiguration& defaults);
        YamlConfiguration  GetDefaults() const;

        // Configuration options
        char GetPathSeparator() const { return m_pathSeparator; }
        void SetPathSeparator(char separator) { m_pathSeparator = separator; }

        // Conversion methods
        std::string ToString() const;
        std::string ToYamlString() const;

        // Access to underlying YAML node
        const YAML::Node& GetNode() const { return m_node; }
        YAML::Node&       GetNode() { return m_node; }

        // Comments support (YAML-specific feature)
        YamlConfiguration& SetComment(const std::string& path, const std::string& comment);
        std::string        GetComment(const std::string& path) const;

    private:
        YAML::Node                         m_node;
        char                               m_pathSeparator;
        std::unique_ptr<YamlConfiguration> m_defaults;

        // Helper methods
        YAML::Node               GetNodeByPath(const std::string& path) const;
        YAML::Node               GetOrCreateNodeByPath(const std::string& path);
        void                     SetNodeByPath(const std::string& path, const YAML::Node& value);
        std::vector<std::string> SplitPath(const std::string& path) const;
    };


    /**
     * YamlObject - Simple YAML object wrapper for basic YAML operations
     * Similar to JsonObject but for YAML data structures
     */
    class YamlObject
    {
    public:
        // Constructors
        YamlObject();
        explicit YamlObject(const std::string& yamlString);
        explicit YamlObject(const YAML::Node& node);

        // Static factory methods
        static YamlObject                Parse(const std::string& yamlString);
        static std::optional<YamlObject> TryParse(const std::string& yamlString);

        // Type checking
        bool IsNull() const;
        bool IsMap() const;
        bool IsSequence() const;
        bool IsScalar() const;

        // Basic access (similar to JsonObject)
        bool Has(const std::string& key) const;

        std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
        int         GetInt(const std::string& key, int defaultValue = 0) const;
        float       GetFloat(const std::string& key, float defaultValue = 0.0f) const;
        double      GetDouble(const std::string& key, double defaultValue = 0.0) const;
        bool        GetBool(const std::string& key, bool defaultValue = false) const;

        YamlObject              GetYamlObject(const std::string& key) const;
        std::vector<YamlObject> GetYamlArray(const std::string& key) const;

        // Setters
        YamlObject& Set(const std::string& key, const std::string& value);
        YamlObject& Set(const std::string& key, int value);
        YamlObject& Set(const std::string& key, float value);
        YamlObject& Set(const std::string& key, double value);
        YamlObject& Set(const std::string& key, bool value);
        YamlObject& Set(const std::string& key, const YamlObject& value);

        // Conversion
        std::string ToString() const;

        // Access to underlying node
        const YAML::Node& GetNode() const { return m_node; }
        YAML::Node&       GetNode() { return m_node; }

    private:
        YAML::Node m_node;
    };

    /**
     * YamlException - YAML operation exceptions
     * Provides detailed error messages for YAML parsing and manipulation issues
     */
    class YamlException : public std::runtime_error
    {
    public:
        explicit YamlException(const std::string& message);
        YamlException(const std::string& message, const YAML::Exception& yamlError);
    };

    /**
     * YamlParseException - Specific exception for YAML parsing errors
     */
    class YamlParseException : public YamlException
    {
    public:
        explicit YamlParseException(const std::string& message);
        YamlParseException(const std::string& message, const YAML::ParserException& parseError);
    };
}
