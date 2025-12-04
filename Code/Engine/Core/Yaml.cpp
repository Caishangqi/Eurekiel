#include "Yaml.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

#include "EngineCommon.hpp"

namespace enigma::core
{
    // YamlConfiguration Implementation

    YamlConfiguration::YamlConfiguration()
        : m_pathSeparator('.')
    {
        m_node = YAML::Node(YAML::NodeType::Map);
    }

    YamlConfiguration::YamlConfiguration(const std::string& yamlString)
        : m_pathSeparator('.')
    {
        try
        {
            m_node = YAML::Load(yamlString);
        }
        catch (const YAML::Exception& e)
        {
            throw YamlParseException("Failed to parse YAML string", static_cast<const YAML::ParserException&>(e));
        }
    }

    YamlConfiguration::YamlConfiguration(const YAML::Node& node)
        : m_node(node), m_pathSeparator('.')
    {
    }

    YamlConfiguration::YamlConfiguration(const YamlConfiguration& other)
        : m_node(YAML::Clone(other.m_node)), m_pathSeparator(other.m_pathSeparator)
    {
        // 正确的深拷贝 defaults
        if (other.m_defaults)
        {
            m_defaults = std::make_unique<YamlConfiguration>(*other.m_defaults);
        }
    }

    YamlConfiguration::YamlConfiguration(YamlConfiguration&& other) noexcept
        : m_node(std::move(other.m_node)), m_pathSeparator(other.m_pathSeparator), m_defaults(std::move(other.m_defaults))
    {
    }

    YamlConfiguration& YamlConfiguration::operator=(const YamlConfiguration& other)
    {
        if (this != &other)
        {
            m_node          = YAML::Clone(other.m_node);
            m_pathSeparator = other.m_pathSeparator;
            if (other.m_defaults)
            {
                m_defaults = std::make_unique<YamlConfiguration>(*other.m_defaults);
            }
            else
            {
                m_defaults = nullptr;
            }
        }
        return *this;
    }

    YamlConfiguration& YamlConfiguration::operator=(YamlConfiguration&& other) noexcept
    {
        if (this != &other)
        {
            m_node          = std::move(other.m_node);
            m_pathSeparator = other.m_pathSeparator;
            m_defaults      = std::move(other.m_defaults);
        }
        return *this;
    }

    YamlConfiguration YamlConfiguration::Parse(const std::string& yamlString)
    {
        return YamlConfiguration(yamlString);
    }

    YamlConfiguration YamlConfiguration::LoadFromFile(const std::string& filePath)
    {
        try
        {
            YAML::Node node = YAML::LoadFile(filePath);
            return YamlConfiguration(node);
        }
        catch (const YAML::Exception& e)
        {
            throw YamlException("Failed to load YAML file: " + filePath, e);
        }
    }

    std::optional<YamlConfiguration> YamlConfiguration::TryParse(const std::string& yamlString)
    {
        try
        {
            return std::make_optional(Parse(yamlString));
        }
        catch (const YamlException&)
        {
            return std::nullopt;
        }
    }

    std::optional<YamlConfiguration> YamlConfiguration::TryLoadFromFile(const std::string& filePath)
    {
        try
        {
            return std::make_optional(LoadFromFile(filePath));
        }
        catch (const YamlException&)
        {
            return std::nullopt;
        }
    }

    bool YamlConfiguration::SaveToFile(const std::string& filePath) const
    {
        try
        {
            std::ofstream file(filePath);
            if (!file.is_open())
            {
                return false;
            }
            file << m_node;
            file.close();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool YamlConfiguration::ReloadFromFile(const std::string& filePath)
    {
        try
        {
            auto newConfig = LoadFromFile(filePath);
            *this          = std::move(newConfig);
            return true;
        }
        catch (const YamlException&)
        {
            return false;
        }
    }

    bool YamlConfiguration::SaveDefaultConfig(const std::string& filePath) const
    {
        try
        {
            std::ifstream file(filePath);
            if (file.good())
            {
                file.close();
                return false; // File already exists
            }
            file.close();
            return SaveToFile(filePath);
        }
        catch (...)
        {
            return false;
        }
    }

    bool YamlConfiguration::IsNull() const
    {
        return !m_node || m_node.IsNull();
    }

    bool YamlConfiguration::IsMap() const
    {
        return m_node.IsMap();
    }

    bool YamlConfiguration::IsSequence() const
    {
        return m_node.IsSequence();
    }

    bool YamlConfiguration::IsScalar() const
    {
        return m_node.IsScalar();
    }

    bool YamlConfiguration::IsEmpty() const
    {
        return !m_node || m_node.size() == 0;
    }

    bool YamlConfiguration::Contains(const std::string& path) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            // 关键修复：使用 IsDefined() 而不是 operator bool()
            return node.IsDefined();
        }
        catch (...)
        {
            return false;
        }
    }

    bool YamlConfiguration::IsSet(const std::string& path) const
    {
        return Contains(path);
    }

    std::vector<std::string> YamlConfiguration::SplitPath(const std::string& path) const
    {
        std::vector<std::string> result;
        std::stringstream        ss(path);
        std::string              segment;

        while (std::getline(ss, segment, m_pathSeparator))
        {
            if (!segment.empty())
            {
                result.push_back(segment);
            }
        }

        return result;
    }

    // [IMPORTANT] Helper function to find a child node by key using iterator
    // This avoids the yaml-cpp bug where operator[] modifies the parent node
    // Returns a CLONED node to ensure complete independence from the original
    static YAML::Node FindChildByKey(const YAML::Node& parent, const std::string& key)
    {
        if (!parent.IsDefined() || !parent.IsMap())
        {
            return YAML::Node();
        }

        for (auto it = parent.begin(); it != parent.end(); ++it)
        {
            if (it->first.as<std::string>() == key)
            {
                // [FIX] Clone the node to break reference semantics
                return YAML::Clone(it->second);
            }
        }

        return YAML::Node(); // Key not found
    }

    YAML::Node YamlConfiguration::GetNodeByPath(const std::string& path) const
    {
        if (path.empty())
        {
            return m_node;
        }

        auto pathParts = SplitPath(path);

        // [FIX] Use YAML::Clone to create a completely independent copy
        // yaml-cpp's YAML::Node uses reference semantics (like shared_ptr),
        // so even assignment like "current = node" shares the underlying data.
        // We must clone to ensure m_node is never modified.

        YAML::Node current = YAML::Clone(m_node);

        for (const auto& part : pathParts)
        {
            // Use iterator-based lookup instead of operator[]
            current = FindChildByKey(current, part);
            if (!current.IsDefined())
            {
                return YAML::Node(); // Path not found
            }
        }

        return current;
    }

    YAML::Node YamlConfiguration::GetOrCreateNodeByPath(const std::string& path)
    {
        if (path.empty())
        {
            return m_node;
        }

        auto       pathParts = SplitPath(path);
        YAML::Node current   = m_node;

        for (const auto& part : pathParts)
        {
            if (!current[part])
            {
                current[part] = YAML::Node(YAML::NodeType::Map);
            }
            current = current[part];
        }

        return current;
    }

    void YamlConfiguration::SetNodeByPath(const std::string& path, const YAML::Node& value)
    {
        if (path.empty())
        {
            m_node = value;
            return;
        }

        auto       pathParts = SplitPath(path);
        YAML::Node current   = m_node;

        for (size_t i = 0; i < pathParts.size() - 1; ++i)
        {
            if (!current[pathParts[i]])
            {
                current[pathParts[i]] = YAML::Node(YAML::NodeType::Map);
            }
            current = current[pathParts[i]];
        }

        current[pathParts.back()] = value;
    }

    std::string YamlConfiguration::GetString(const std::string& path, const std::string& defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<std::string>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    int YamlConfiguration::GetInt(const std::string& path, int defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<int>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    long YamlConfiguration::GetLong(const std::string& path, long defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<long>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    float YamlConfiguration::GetFloat(const std::string& path, float defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<float>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    double YamlConfiguration::GetDouble(const std::string& path, double defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<double>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    bool YamlConfiguration::GetBoolean(const std::string& path, bool defaultValue) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return node.as<bool>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    std::vector<std::string> YamlConfiguration::GetStringList(const std::string& path) const
    {
        std::vector<std::string> result;
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && node.IsSequence())
            {
                for (const auto& item : node)
                {
                    result.push_back(item.as<std::string>());
                }
            }
        }
        catch (...)
        {
            // Return empty list on error
        }
        return result;
    }

    std::vector<int> YamlConfiguration::GetIntList(const std::string& path) const
    {
        std::vector<int> result;
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && node.IsSequence())
            {
                for (const auto& item : node)
                {
                    result.push_back(item.as<int>());
                }
            }
        }
        catch (...)
        {
            // Return empty list on error
        }
        return result;
    }

    std::vector<float> YamlConfiguration::GetFloatList(const std::string& path) const
    {
        std::vector<float> result;
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && node.IsSequence())
            {
                for (const auto& item : node)
                {
                    result.push_back(item.as<float>());
                }
            }
        }
        catch (...)
        {
            // Return empty list on error
        }
        return result;
    }

    std::vector<bool> YamlConfiguration::GetBooleanList(const std::string& path) const
    {
        std::vector<bool> result;
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && node.IsSequence())
            {
                for (const auto& item : node)
                {
                    result.push_back(item.as<bool>());
                }
            }
        }
        catch (...)
        {
            // Return empty list on error
        }
        return result;
    }

    std::vector<YamlConfiguration> YamlConfiguration::GetConfigurationList(const std::string& path) const
    {
        std::vector<YamlConfiguration> result;
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && node.IsSequence())
            {
                for (const auto& item : node)
                {
                    result.emplace_back(item);
                }
            }
        }
        catch (...)
        {
            // Return empty list on error
        }
        return result;
    }

    std::optional<std::string> YamlConfiguration::GetStringOpt(const std::string& path) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return std::make_optional(node.as<std::string>());
            }
        }
        catch (...)
        {
            // Fall through to nullopt
        }
        return std::nullopt;
    }

    std::optional<int> YamlConfiguration::GetIntOpt(const std::string& path) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return std::make_optional(node.as<int>());
            }
        }
        catch (...)
        {
            // Fall through to nullopt
        }
        return std::nullopt;
    }

    std::optional<bool> YamlConfiguration::GetBooleanOpt(const std::string& path) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return std::make_optional(node.as<bool>());
            }
        }
        catch (...)
        {
            // Fall through to nullopt
        }
        return std::nullopt;
    }

    std::optional<double> YamlConfiguration::GetDoubleOpt(const std::string& path) const
    {
        try
        {
            YAML::Node node = GetNodeByPath(path);
            if (node && !node.IsNull())
            {
                return std::make_optional(node.as<double>());
            }
        }
        catch (...)
        {
            // Fall through to nullopt
        }
        return std::nullopt;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const std::string& value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const char* value)
    {
        SetNodeByPath(path, YAML::Node(std::string(value)));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, int value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, long value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, float value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, double value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, bool value)
    {
        SetNodeByPath(path, YAML::Node(value));
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const YamlConfiguration& value)
    {
        SetNodeByPath(path, value.m_node);
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const std::vector<std::string>& value)
    {
        YAML::Node listNode(YAML::NodeType::Sequence);
        for (const auto& item : value)
        {
            listNode.push_back(item);
        }
        SetNodeByPath(path, listNode);
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const std::vector<int>& value)
    {
        YAML::Node listNode(YAML::NodeType::Sequence);
        for (const auto& item : value)
        {
            listNode.push_back(item);
        }
        SetNodeByPath(path, listNode);
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Set(const std::string& path, const std::vector<YamlConfiguration>& value)
    {
        YAML::Node listNode(YAML::NodeType::Sequence);
        for (const auto& item : value)
        {
            listNode.push_back(item.m_node);
        }
        SetNodeByPath(path, listNode);
        return *this;
    }

    YamlConfiguration YamlConfiguration::GetConfigurationSection(const std::string& path) const
    {
        YAML::Node node = GetNodeByPath(path);
        if (node && !node.IsNull())
        {
            return YamlConfiguration(node);
        }
        return YamlConfiguration();
    }

    YamlConfiguration YamlConfiguration::CreateSection(const std::string& path)
    {
        YAML::Node node = GetOrCreateNodeByPath(path);
        if (!node.IsMap())
        {
            SetNodeByPath(path, YAML::Node(YAML::NodeType::Map));
            node = GetNodeByPath(path);
        }
        return YamlConfiguration(node);
    }

    std::vector<std::string> YamlConfiguration::GetKeys() const
    {
        return GetKeys("", false);
    }

    std::vector<std::string> YamlConfiguration::GetKeys(const std::string& path) const
    {
        return GetKeys(path, false);
    }

    std::vector<std::string> YamlConfiguration::GetKeys(bool deep) const
    {
        return GetKeys("", deep);
    }

    std::vector<std::string> YamlConfiguration::GetKeys(const std::string& path, bool deep) const
    {
        std::vector<std::string> keys;
        YAML::Node               node = path.empty() ? m_node : GetNodeByPath(path);

        if (!node || !node.IsMap())
        {
            return keys;
        }

        for (const auto& pair : node)
        {
            std::string key      = pair.first.as<std::string>();
            std::string fullPath = path.empty() ? key : path + m_pathSeparator + key;

            keys.push_back(fullPath);

            if (deep && pair.second.IsMap())
            {
                auto subKeys = GetKeys(fullPath, true);
                keys.insert(keys.end(), subKeys.begin(), subKeys.end());
            }
        }

        return keys;
    }

    YamlConfiguration& YamlConfiguration::Remove(const std::string& path)
    {
        auto pathParts = SplitPath(path);
        if (pathParts.empty())
        {
            return *this;
        }

        YAML::Node current = m_node;
        for (size_t i = 0; i < pathParts.size() - 1; ++i)
        {
            if (!current[pathParts[i]])
            {
                return *this; // Path doesn't exist
            }
            current = current[pathParts[i]];
        }

        current.remove(pathParts.back());
        return *this;
    }

    YamlConfiguration& YamlConfiguration::Clear()
    {
        m_node = YAML::Node(YAML::NodeType::Map);
        return *this;
    }

    YamlConfiguration& YamlConfiguration::AddDefault(const std::string& path, const std::string& value)
    {
        if (!Contains(path))
        {
            Set(path, value);
        }
        return *this;
    }

    YamlConfiguration& YamlConfiguration::AddDefault(const std::string& path, int value)
    {
        if (!Contains(path))
        {
            Set(path, value);
        }
        return *this;
    }

    YamlConfiguration& YamlConfiguration::AddDefault(const std::string& path, bool value)
    {
        if (!Contains(path))
        {
            Set(path, value);
        }
        return *this;
    }

    YamlConfiguration& YamlConfiguration::AddDefaults(const YamlConfiguration& defaults)
    {
        auto keys = defaults.GetKeys(true);
        for (const auto& key : keys)
        {
            if (!Contains(key))
            {
                YAML::Node node = defaults.GetNodeByPath(key);
                SetNodeByPath(key, node);
            }
        }
        return *this;
    }

    void YamlConfiguration::SetDefaults(const YamlConfiguration& defaults)
    {
        m_defaults = std::make_unique<YamlConfiguration>(defaults);
    }

    YamlConfiguration YamlConfiguration::GetDefaults() const
    {
        if (m_defaults)
        {
            return *m_defaults;
        }
        return YamlConfiguration();
    }

    std::string YamlConfiguration::ToString() const
    {
        return ToYamlString();
    }

    std::string YamlConfiguration::ToYamlString() const
    {
        std::stringstream ss;
        ss << m_node;
        return ss.str();
    }

    YamlConfiguration& YamlConfiguration::SetComment(const std::string& path, const std::string& comment)
    {
        UNUSED(path)
        UNUSED(comment)
        // Note: yaml-cpp doesn't directly support comments in the API, 
        // but we can store them as metadata if needed
        return *this;
    }

    std::string YamlConfiguration::GetComment(const std::string& path) const
    {
        UNUSED(path)
        // Note: yaml-cpp doesn't directly support comments in the API
        return "";
    }

    // YamlObject Implementation

    YamlObject::YamlObject()
    {
        m_node = YAML::Node(YAML::NodeType::Map);
    }

    YamlObject::YamlObject(const std::string& yamlString)
    {
        try
        {
            m_node = YAML::Load(yamlString);
        }
        catch (const YAML::Exception& e)
        {
            throw YamlParseException("Failed to parse YAML string", static_cast<const YAML::ParserException&>(e));
        }
    }

    YamlObject::YamlObject(const YAML::Node& node) : m_node(node)
    {
    }

    YamlObject YamlObject::Parse(const std::string& yamlString)
    {
        return YamlObject(yamlString);
    }

    std::optional<YamlObject> YamlObject::TryParse(const std::string& yamlString)
    {
        try
        {
            return std::make_optional(Parse(yamlString));
        }
        catch (const YamlException&)
        {
            return std::nullopt;
        }
    }

    bool YamlObject::IsNull() const
    {
        return !m_node || m_node.IsNull();
    }

    bool YamlObject::IsMap() const
    {
        return m_node.IsMap();
    }

    bool YamlObject::IsSequence() const
    {
        return m_node.IsSequence();
    }

    bool YamlObject::IsScalar() const
    {
        return m_node.IsScalar();
    }

    bool YamlObject::Has(const std::string& key) const
    {
        return m_node[key] && !m_node[key].IsNull();
    }

    std::string YamlObject::GetString(const std::string& key, const std::string& defaultValue) const
    {
        try
        {
            if (m_node[key])
            {
                return m_node[key].as<std::string>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    int YamlObject::GetInt(const std::string& key, int defaultValue) const
    {
        try
        {
            if (m_node[key])
            {
                return m_node[key].as<int>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    float YamlObject::GetFloat(const std::string& key, float defaultValue) const
    {
        try
        {
            if (m_node[key])
            {
                return m_node[key].as<float>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    double YamlObject::GetDouble(const std::string& key, double defaultValue) const
    {
        try
        {
            if (m_node[key])
            {
                return m_node[key].as<double>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    bool YamlObject::GetBool(const std::string& key, bool defaultValue) const
    {
        try
        {
            if (m_node[key])
            {
                return m_node[key].as<bool>();
            }
        }
        catch (...)
        {
            // Fall through to default
        }
        return defaultValue;
    }

    YamlObject YamlObject::GetYamlObject(const std::string& key) const
    {
        if (m_node[key])
        {
            return YamlObject(m_node[key]);
        }
        return YamlObject();
    }

    std::vector<YamlObject> YamlObject::GetYamlArray(const std::string& key) const
    {
        std::vector<YamlObject> result;
        if (m_node[key] && m_node[key].IsSequence())
        {
            for (const auto& item : m_node[key])
            {
                result.emplace_back(item);
            }
        }
        return result;
    }

    YamlObject& YamlObject::Set(const std::string& key, const std::string& value)
    {
        m_node[key] = value;
        return *this;
    }

    YamlObject& YamlObject::Set(const std::string& key, int value)
    {
        m_node[key] = value;
        return *this;
    }

    YamlObject& YamlObject::Set(const std::string& key, float value)
    {
        m_node[key] = value;
        return *this;
    }

    YamlObject& YamlObject::Set(const std::string& key, double value)
    {
        m_node[key] = value;
        return *this;
    }

    YamlObject& YamlObject::Set(const std::string& key, bool value)
    {
        m_node[key] = value;
        return *this;
    }

    YamlObject& YamlObject::Set(const std::string& key, const YamlObject& value)
    {
        m_node[key] = value.m_node;
        return *this;
    }

    std::string YamlObject::ToString() const
    {
        std::stringstream ss;
        ss << m_node;
        return ss.str();
    }

    // Exception Implementations

    YamlException::YamlException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    YamlException::YamlException(const std::string& message, const YAML::Exception& yamlError)
        : std::runtime_error(message + ": " + yamlError.what())
    {
    }

    YamlParseException::YamlParseException(const std::string& message)
        : YamlException(message)
    {
    }

    YamlParseException::YamlParseException(const std::string& message, const YAML::ParserException& parseError)
        : YamlException(message + ": " + parseError.what())
    {
    }
}
