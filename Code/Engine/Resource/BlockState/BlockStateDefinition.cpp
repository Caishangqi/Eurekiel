#include "BlockStateDefinition.hpp"
#include "../../Core/StringUtils.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace enigma::resource::blockstate;
using namespace enigma::core;

bool BlockStateVariant::LoadFromJson(const JsonObject& json)
{
    if (!json.IsObject())
        return false;

    // Required: model
    if (!json.ContainsKey("model"))
        return false;

    model = ResourceLocation(json.GetString("model"));

    // Optional: rotations
    x = json.GetInt("x", 0);
    y = json.GetInt("y", 0);

    // Validate rotation values
    if (x % 90 != 0 || x < 0 || x >= 360) x = 0;
    if (y % 90 != 0 || y < 0 || y >= 360) y = 0;

    // Optional: uvlock
    uvlock = json.GetBool("uvlock", false);

    // Optional: weight
    weight = json.GetInt("weight", 1);
    if (weight < 1) weight = 1;

    return true;
}

bool MultipartCondition::LoadFromJson(const JsonObject& json)
{
    if (!json.IsObject())
        return false;

    // Each key-value pair is a property condition
    const auto& jsonObj = json.GetJson();
    for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it)
    {
        if (it->is_string())
        {
            properties[it.key()] = it->get<std::string>();
        }
    }

    return !properties.empty();
}

bool MultipartCondition::Matches(const std::map<std::string, std::string>& blockProperties) const
{
    for (const auto& condition : properties)
    {
        auto it = blockProperties.find(condition.first);
        if (it == blockProperties.end() || it->second != condition.second)
        {
            return false;
        }
    }
    return true;
}

bool MultipartCase::LoadFromJson(const JsonObject& json)
{
    if (!json.IsObject())
        return false;

    // Load "when" conditions (optional)
    if (json.ContainsKey("when"))
    {
        JsonObject whenObj = json.GetJsonObject("when");
        if (whenObj.IsArray())
        {
            // Array of conditions (OR logic)
            auto whenArray = json.GetJsonArray("when");
            for (const auto& conditionJson : whenArray)
            {
                MultipartCondition condition;
                if (condition.LoadFromJson(conditionJson))
                {
                    when.push_back(condition);
                }
            }
        }
        else if (whenObj.IsObject())
        {
            // Single condition
            MultipartCondition condition;
            if (condition.LoadFromJson(whenObj))
            {
                when.push_back(condition);
            }
        }
    }

    // Load "apply" variants (required)
    if (!json.ContainsKey("apply"))
        return false;

    JsonObject applyObj = json.GetJsonObject("apply");
    if (applyObj.IsArray())
    {
        // Array of variants
        auto applyArray = json.GetJsonArray("apply");
        for (const auto& variantJson : applyArray)
        {
            BlockStateVariant variant;
            if (variant.LoadFromJson(variantJson))
            {
                apply.push_back(variant);
            }
        }
    }
    else if (applyObj.IsObject())
    {
        // Single variant
        BlockStateVariant variant;
        if (variant.LoadFromJson(applyObj))
        {
            apply.push_back(variant);
        }
    }

    return !apply.empty();
}

bool MultipartCase::ShouldApply(const std::map<std::string, std::string>& blockProperties) const
{
    // If no conditions, always apply
    if (when.empty())
        return true;

    // OR logic between conditions
    for (const auto& condition : when)
    {
        if (condition.Matches(blockProperties))
            return true;
    }

    return false;
}

BlockStateDefinition::BlockStateDefinition(const ResourceLocation& location)
{
    m_metadata.location = location;
    m_metadata.type     = ResourceType::BLOCKSTATE;
    m_metadata.state    = ResourceState::NOT_LOADED;
}

bool BlockStateDefinition::LoadFromFile(const std::filesystem::path& filePath)
{
    try
    {
        if (!std::filesystem::exists(filePath))
        {
            m_metadata.state = ResourceState::LOAD_ERROR;
            return false;
        }

        std::ifstream file(filePath);
        if (!file.is_open())
        {
            m_metadata.state = ResourceState::LOAD_ERROR;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        JsonObject json   = JsonObject::Parse(buffer.str());
        bool       result = LoadFromJson(json);

        if (result)
        {
            m_metadata.state = ResourceState::LOADED;
        }
        else
        {
            m_metadata.state = ResourceState::LOAD_ERROR;
        }

        return result;
    }
    catch (const std::exception&)
    {
        m_metadata.state = ResourceState::LOAD_ERROR;
        return false;
    }
}

void BlockStateDefinition::Unload()
{
    m_variants.clear();
    m_multipart.clear();
    m_isMultipart    = false;
    m_metadata.state = ResourceState::NOT_LOADED;
}

bool BlockStateDefinition::LoadFromJson(const JsonObject& json)
{
    if (!json.IsObject())
        return false;

    // Clear existing data
    m_variants.clear();
    m_multipart.clear();
    m_isMultipart = false;

    // Check if this is multipart format
    if (json.ContainsKey("multipart"))
    {
        m_isMultipart       = true;
        auto multipartArray = json.GetJsonArray("multipart");

        for (const auto& caseJson : multipartArray)
        {
            MultipartCase multipartCase;
            if (multipartCase.LoadFromJson(caseJson))
            {
                m_multipart.push_back(multipartCase);
            }
        }

        return !m_multipart.empty();
    }
    // Variants format
    else if (json.ContainsKey("variants"))
    {
        m_isMultipart            = false;
        JsonObject  variantsObj  = json.GetJsonObject("variants");
        const auto& variantsJson = variantsObj.GetJson();

        for (auto it = variantsJson.begin(); it != variantsJson.end(); ++it)
        {
            std::string                    propertyString = it.key();
            std::vector<BlockStateVariant> variants;

            if (it->is_array())
            {
                // Array of variants for this property combination
                for (const auto& variantJson : *it)
                {
                    JsonObject        variantObj(variantJson);
                    BlockStateVariant variant;
                    if (variant.LoadFromJson(variantObj))
                    {
                        variants.push_back(variant);
                    }
                }
            }
            else if (it->is_object())
            {
                // Single variant for this property combination
                JsonObject        variantObj(*it);
                BlockStateVariant variant;
                if (variant.LoadFromJson(variantObj))
                {
                    variants.push_back(variant);
                }
            }

            if (!variants.empty())
            {
                m_variants[propertyString] = variants;
            }
        }

        return !m_variants.empty();
    }

    return false;
}

const std::vector<BlockStateVariant>* BlockStateDefinition::GetVariants(const std::string& propertyString) const
{
    auto it = m_variants.find(propertyString);
    return (it != m_variants.end()) ? &it->second : nullptr;
}

const std::vector<BlockStateVariant>* BlockStateDefinition::GetVariants(const std::map<std::string, std::string>& properties) const
{
    std::string propertyString = MapToPropertyString(properties);
    return GetVariants(propertyString);
}

std::vector<BlockStateVariant> BlockStateDefinition::GetApplicableVariants(const std::map<std::string, std::string>& properties) const
{
    std::vector<BlockStateVariant> result;

    if (!m_isMultipart)
        return result;

    for (const auto& multipartCase : m_multipart)
    {
        if (multipartCase.ShouldApply(properties))
        {
            for (const auto& variant : multipartCase.apply)
            {
                result.push_back(variant);
            }
        }
    }

    return result;
}

bool BlockStateDefinition::HasVariant(const std::string& propertyString) const
{
    return m_variants.find(propertyString) != m_variants.end();
}

std::vector<std::string> BlockStateDefinition::GetAllVariantKeys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_variants.size());

    for (const auto& pair : m_variants)
    {
        keys.push_back(pair.first);
    }

    return keys;
}

std::shared_ptr<BlockStateDefinition> BlockStateDefinition::Create(const ResourceLocation& location)
{
    return std::make_shared<BlockStateDefinition>(location);
}

std::shared_ptr<BlockStateDefinition> BlockStateDefinition::LoadFromFile(const ResourceLocation& location, const std::filesystem::path& filePath)
{
    auto definition = Create(location);
    if (definition->LoadFromFile(filePath))
    {
        return definition;
    }
    return nullptr;
}

std::string BlockStateDefinition::MapToPropertyString(const std::map<std::string, std::string>& properties) const
{
    if (properties.empty())
        return "";

    std::stringstream ss;
    bool              first = true;

    for (const auto& prop : properties)
    {
        if (!first)
            ss << ",";
        ss << prop.first << "=" << prop.second;
        first = false;
    }

    return ss.str();
}
