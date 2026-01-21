#include "BlockStateDefinition.hpp"
#include "../../Core/StringUtils.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include "../../Renderer/Model/RenderMesh.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace enigma::resource::blockstate;
using namespace enigma::core;

/**
 * @brief Normalize a property string by sorting key=value pairs alphabetically
 * 
 * Converts "facing=east,half=bottom,shape=straight" to sorted order.
 * This ensures consistent lookup regardless of the order in JSON files.
 * 
 * @param propertyString Original property string (e.g., "facing=east,half=bottom")
 * @return Normalized property string with sorted keys
 */
static std::string NormalizePropertyString(const std::string& propertyString)
{
    if (propertyString.empty())
        return "";

    // Split by comma
    std::vector<std::string> pairs;
    std::stringstream        ss(propertyString);
    std::string              pair;
    while (std::getline(ss, pair, ','))
    {
        if (!pair.empty())
        {
            pairs.push_back(pair);
        }
    }

    // Sort alphabetically
    std::sort(pairs.begin(), pairs.end());

    // Rejoin
    std::string result;
    for (size_t i = 0; i < pairs.size(); ++i)
    {
        if (i > 0) result += ",";
        result += pairs[i];
    }

    return result;
}

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
            // [FIX] Normalize property string to ensure consistent lookup order
            std::string                    propertyString = NormalizePropertyString(it.key());
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
    std::string normalizedString = NormalizePropertyString(propertyString);

    auto it = m_variants.find(normalizedString);

    // Fallback to empty variant "" if exact match not found
    // Handles blocks like water where JSON defines "" but block has properties (e.g., "level=0")
    if (it == m_variants.end() && !normalizedString.empty())
    {
        it = m_variants.find("");
    }

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

/**
 * @brief Convert Minecraft-style model path to resource path
 *
 * Minecraft JSON uses short paths like "simpleminer:block/oak_stairs"
 * but ResourceSubsystem indexes with "simpleminer:models/block/oak_stairs"
 *
 * This function converts:
 *   "simpleminer:block/xxx" -> "simpleminer:models/block/xxx"
 *   "block/xxx" -> "models/block/xxx"
 */
static ResourceLocation NormalizeModelPath(const ResourceLocation& modelPath)
{
    std::string ns   = modelPath.GetNamespace();
    std::string path = modelPath.GetPath();

    // If path doesn't start with "models/", add it
    if (path.find("models/") != 0)
    {
        path = "models/" + path;
    }

    return ResourceLocation(ns, path);
}

void BlockStateDefinition::CompileModels(enigma::model::ModelSubsystem* modelSubsystem)
{
    if (!modelSubsystem)
    {
        enigma::core::LogWarn("BlockStateDefinition", "CompileModels called with null ModelSubsystem");
        return;
    }

    // Track compilation statistics
    int compiledCount = 0;
    int failedCount   = 0;
    int rotatedCount  = 0;

    // Check debug flag for detailed logging
    bool isDebug = (m_metadata.location.ToString().find("stairs") != std::string::npos ||
        m_metadata.location.ToString().find("slab") != std::string::npos);

    if (isDebug)
    {
        enigma::core::LogInfo("BlockStateDefinition", "[CompileModels] Starting for: %s (variants: %zu, multipart: %zu)",
                              m_metadata.location.ToString().c_str(), m_variants.size(), m_multipart.size());
    }

    if (m_isMultipart)
    {
        // Compile multipart variants
        for (auto& multipartCase : m_multipart)
        {
            for (auto& variant : multipartCase.apply)
            {
                // [FIX] Normalize model path: "block/xxx" -> "models/block/xxx"
                ResourceLocation normalizedPath = NormalizeModelPath(variant.model);

                // Compile the base model
                auto compiledMesh = modelSubsystem->CompileModel(normalizedPath);

                if (compiledMesh)
                {
                    // [NEW] Apply variant rotation if specified (following Minecraft's FaceBakery pattern)
                    if (variant.x != 0 || variant.y != 0)
                    {
                        if (isDebug)
                        {
                            enigma::core::LogInfo("BlockStateDefinition", "  [ROTATION] Applying (x=%d, y=%d) to model: %s",
                                                  variant.x, variant.y, variant.model.ToString().c_str());
                        }
                        compiledMesh->ApplyBlockRotation(variant.x, variant.y);
                        rotatedCount++;
                    }

                    variant.compiledMesh = compiledMesh;
                    compiledCount++;
                }
                else
                {
                    enigma::core::LogWarn("BlockStateDefinition", "  [FAILED] Could not compile model: %s",
                                          variant.model.ToString().c_str());
                    failedCount++;
                }
            }
        }
    }
    else
    {
        // Compile variants mode
        for (auto& [propertyString, variantList] : m_variants)
        {
            for (auto& variant : variantList)
            {
                // [FIX] Normalize model path: "block/xxx" -> "models/block/xxx"
                ResourceLocation normalizedPath = NormalizeModelPath(variant.model);

                // Compile the base model
                auto compiledMesh = modelSubsystem->CompileModel(normalizedPath);

                if (compiledMesh)
                {
                    // [NEW] Apply variant rotation if specified (following Minecraft's FaceBakery pattern)
                    if (variant.x != 0 || variant.y != 0)
                    {
                        if (isDebug)
                        {
                            enigma::core::LogInfo("BlockStateDefinition", "  [ROTATION] Applying (x=%d, y=%d) to variant '%s' model: %s",
                                                  variant.x, variant.y, propertyString.c_str(), variant.model.ToString().c_str());
                        }
                        compiledMesh->ApplyBlockRotation(variant.x, variant.y);
                        rotatedCount++;
                    }

                    variant.compiledMesh = compiledMesh;
                    compiledCount++;
                }
                else
                {
                    enigma::core::LogWarn("BlockStateDefinition", "  [FAILED] Could not compile model for variant '%s': %s",
                                          propertyString.c_str(), variant.model.ToString().c_str());
                    failedCount++;
                }
            }
        }
    }

    if (isDebug || failedCount > 0)
    {
        enigma::core::LogInfo("BlockStateDefinition", "[CompileModels] Complete for %s: compiled=%d, failed=%d, rotated=%d",
                              m_metadata.location.ToString().c_str(), compiledCount, failedCount, rotatedCount);
    }
}
