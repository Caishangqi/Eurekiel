#include "BlockStateBuilder.hpp"
#include "../../Registry/Block/Block.hpp"
#include "../../Core/StringUtils.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include "../../Model/Compiler/BlockModelCompiler.hpp"
#include "../../Resource/Atlas/AtlasManager.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Renderer/Model/RenderMesh.hpp"
#include <sstream>
#include <algorithm>

using namespace enigma::resource::blockstate;
using namespace enigma::registry::block;
DEFINE_LOG_CATEGORY(LogBlockStateBuilder)
// ConditionBuilder implementation
std::map<std::string, std::string> BlockStateBuilder::ConditionBuilder::Build() const
{
    return m_conditions;
}

BlockStateBuilder::ConditionBuilder& BlockStateBuilder::ConditionBuilder::Property(const std::string& name, const std::string& value)
{
    m_conditions[name] = value;
    return *this;
}

BlockStateBuilder::ConditionBuilder& BlockStateBuilder::ConditionBuilder::Property(const std::string& name, bool value)
{
    m_conditions[name] = value ? "true" : "false";
    return *this;
}

BlockStateBuilder::ConditionBuilder& BlockStateBuilder::ConditionBuilder::Property(const std::string& name, int value)
{
    m_conditions[name] = std::to_string(value);
    return *this;
}

// VariantBuilder implementation
BlockStateBuilder::VariantBuilder::VariantBuilder(const std::string& modelPath)
{
    m_variant.model = ResourceLocation(modelPath);
}

BlockStateBuilder::VariantBuilder::VariantBuilder(const ResourceLocation& modelPath)
{
    m_variant.model = modelPath;
}

BlockStateVariant BlockStateBuilder::VariantBuilder::Build() const
{
    return m_variant;
}

BlockStateBuilder::VariantBuilder& BlockStateBuilder::VariantBuilder::RotationX(int degrees)
{
    // Validate rotation
    if (degrees % 90 == 0 && degrees >= 0 && degrees < 360)
    {
        m_variant.x = degrees;
    }
    return *this;
}

BlockStateBuilder::VariantBuilder& BlockStateBuilder::VariantBuilder::RotationY(int degrees)
{
    // Validate rotation
    if (degrees % 90 == 0 && degrees >= 0 && degrees < 360)
    {
        m_variant.y = degrees;
    }
    return *this;
}

BlockStateBuilder::VariantBuilder& BlockStateBuilder::VariantBuilder::UVLock(bool uvlock)
{
    m_variant.uvlock = uvlock;
    return *this;
}
#undef max
BlockStateBuilder::VariantBuilder& BlockStateBuilder::VariantBuilder::Weight(int weight)
{
    m_variant.weight = std::max(1, weight);
    return *this;
}

// MultipartCaseBuilder implementation
BlockStateBuilder::MultipartCaseBuilder& BlockStateBuilder::MultipartCaseBuilder::When(const ConditionBuilder& condition)
{
    MultipartCondition multipartCondition;
    multipartCondition.properties = condition.Build();
    m_conditions.push_back(multipartCondition);
    return *this;
}

BlockStateBuilder::MultipartCaseBuilder& BlockStateBuilder::MultipartCaseBuilder::When(std::function<ConditionBuilder()> conditionFunc)
{
    return When(conditionFunc());
}

BlockStateBuilder::MultipartCaseBuilder& BlockStateBuilder::MultipartCaseBuilder::Apply(const VariantBuilder& variant)
{
    m_variants.push_back(variant.Build());
    return *this;
}

BlockStateBuilder::MultipartCaseBuilder& BlockStateBuilder::MultipartCaseBuilder::Apply(std::function<VariantBuilder()> variantFunc)
{
    return Apply(variantFunc());
}

MultipartCase BlockStateBuilder::MultipartCaseBuilder::Build() const
{
    MultipartCase multipartCase;
    multipartCase.when  = m_conditions;
    multipartCase.apply = m_variants;
    return multipartCase;
}

// BlockStateBuilder implementation
BlockStateBuilder::BlockStateBuilder(const ResourceLocation& location)
    : m_location(location)
{
}

BlockStateBuilder::BlockStateBuilder(const std::string& location)
    : m_location(ResourceLocation(location))
{
}

BlockStateBuilder& BlockStateBuilder::Variant(const std::string& propertyString, const VariantBuilder& variant)
{
    m_isMultipart = false;
    m_variants[propertyString].push_back(variant.Build());
    return *this;
}

BlockStateBuilder& BlockStateBuilder::Variant(const std::string& propertyString, std::function<VariantBuilder()> variantFunc)
{
    return Variant(propertyString, variantFunc());
}

BlockStateBuilder& BlockStateBuilder::Variant(const PropertyMap& properties, const VariantBuilder& variant)
{
    std::string propertyString = MapPropertiesToString(properties);
    return Variant(propertyString, variant);
}

BlockStateBuilder& BlockStateBuilder::Variants(const std::string& propertyString, const std::vector<VariantBuilder>& variants)
{
    m_isMultipart = false;
    for (const auto& variant : variants)
    {
        m_variants[propertyString].push_back(variant.Build());
    }
    return *this;
}

BlockStateBuilder& BlockStateBuilder::DefaultVariant(const VariantBuilder& variant)
{
    return Variant("", variant);
}

BlockStateBuilder& BlockStateBuilder::DefaultVariant(std::function<VariantBuilder()> variantFunc)
{
    return DefaultVariant(variantFunc());
}

BlockStateBuilder& BlockStateBuilder::Multipart(const MultipartCaseBuilder& caseBuilder)
{
    m_isMultipart = true;
    m_multipartCases.push_back(caseBuilder.Build());
    return *this;
}

BlockStateBuilder& BlockStateBuilder::Multipart(std::function<MultipartCaseBuilder()> caseFunc)
{
    return Multipart(caseFunc());
}

BlockStateBuilder& BlockStateBuilder::AutoGenerateVariants(
    Block*                                         block,
    const std::string&                             baseModelPath,
    std::function<std::string(const PropertyMap&)> modelPathMapper)
{
    if (!block)
        return *this;

    m_isMultipart = false;

    // Generate all property combinations
    std::vector<PropertyMap> combinations;
    GeneratePropertyCombinations(block, combinations);

    // Create variants for each combination
    for (const auto& propertyMap : combinations)
    {
        std::string modelPath = baseModelPath;
        if (modelPathMapper)
        {
            modelPath = modelPathMapper(propertyMap);
        }

        std::string    propertyString = MapPropertiesToString(propertyMap);
        VariantBuilder variant(modelPath);

        m_variants[propertyString].push_back(variant.Build());
    }

    return *this;
}

std::shared_ptr<BlockStateDefinition> BlockStateBuilder::Build()
{
    auto definition = std::make_shared<BlockStateDefinition>(m_location);

    // Get ModelSubsystem for model compilation
    auto* modelSubsystem = GEngine->GetSubsystem<enigma::model::ModelSubsystem>();
    if (!modelSubsystem)
    {
        LogWarn(LogBlockStateBuilder, "ModelSubsystem not found - models will not be compiled");
    }

    if (m_isMultipart)
    {
        // Build multipart definition
        definition->m_isMultipart = true;
        definition->m_multipart   = m_multipartCases;

        // Compile models for multipart cases
        if (modelSubsystem)
        {
            for (auto& multipartCase : definition->m_multipart)
            {
                for (auto& variant : multipartCase.apply)
                {
                    CompileVariantModel(variant, modelSubsystem);
                }
            }
        }
    }
    else
    {
        // Build variants definition
        definition->m_isMultipart = false;
        definition->m_variants    = m_variants;

        // Compile models for variants
        if (modelSubsystem)
        {
            for (auto& [propertyString, variantList] : definition->m_variants)
            {
                for (auto& variant : variantList)
                {
                    CompileVariantModel(variant, modelSubsystem);
                }
            }
        }
    }

    definition->m_metadata.location = m_location;
    definition->m_metadata.type     = ResourceType::BLOCKSTATE;
    definition->m_metadata.state    = ResourceState::LOADED;

    return definition;
}

std::shared_ptr<BlockStateDefinition> BlockStateBuilder::Simple(
    const ResourceLocation& location,
    const std::string&      modelPath)
{
    return BlockStateBuilder(location)
           .DefaultVariant(VariantBuilder(modelPath))
           .Build();
}

std::shared_ptr<BlockStateDefinition> BlockStateBuilder::WithVariants(
    const ResourceLocation&                   location,
    const std::map<std::string, std::string>& variantMap)
{
    BlockStateBuilder builder(location);

    for (const auto& pair : variantMap)
    {
        builder.Variant(pair.first, VariantBuilder(pair.second));
    }

    return builder.Build();
}

std::string BlockStateBuilder::MapPropertiesToString(const PropertyMap& properties) const
{
    if (properties.Empty())
        return "";

    // Use PropertyMap's ToString method and parse it
    std::string propertyMapString = properties.ToString();

    // Remove the braces {} and return the content
    if (propertyMapString.size() >= 2 &&
        propertyMapString.front() == '{' &&
        propertyMapString.back() == '}')
    {
        return propertyMapString.substr(1, propertyMapString.size() - 2);
    }

    return propertyMapString;
}

void BlockStateBuilder::GeneratePropertyCombinations(
    Block*                    block,
    std::vector<PropertyMap>& combinations) const
{
    const auto& properties = block->GetProperties();

    if (properties.empty())
    {
        combinations.push_back(PropertyMap());
        return;
    }

    // Generate all combinations recursively
    std::function<void(size_t, PropertyMap)> generateRecursive =
        [&](size_t propertyIndex, PropertyMap currentMap)
    {
        if (propertyIndex >= properties.size())
        {
            combinations.push_back(currentMap);
            return;
        }

        auto property                = properties[propertyIndex];
        auto possibleValuesAsStrings = property->GetPossibleValuesAsStrings();

        for (const auto& valueStr : possibleValuesAsStrings)
        {
            PropertyMap newMap = currentMap;
            // Convert string back to the proper type using StringToValue
            std::any value = property->StringToValue(valueStr);

            // Use the new SetAny method to set the property value
            newMap.SetAny(property, value);
            generateRecursive(propertyIndex + 1, newMap);
        }
    };

    generateRecursive(0, PropertyMap());
}

void BlockStateBuilder::CompileVariantModel(BlockStateVariant& variant, enigma::model::ModelSubsystem* modelSubsystem) const
{
    if (!modelSubsystem)
    {
        return;
    }

    try
    {
        // Use ModelSubsystem to compile the block model
        // This will trigger the BlockModelCompiler compilation pipeline
        auto compiledMesh = modelSubsystem->CompileModel(variant.model);

        if (compiledMesh)
        {
            // [FIX] Apply variant rotation if specified
            // Minecraft blockstate variants can have x and y rotation values (0, 90, 180, 270)
            if (variant.x != 0 || variant.y != 0)
            {
                LogInfo(LogBlockStateBuilder, "Applying rotation (x=%d, y=%d) to model: %s",
                        variant.x, variant.y, variant.model.ToString().c_str());

                // Apply rotation to the compiled mesh
                // This rotates all vertices around block center (0.5, 0.5, 0.5)
                compiledMesh->ApplyBlockRotation(variant.x, variant.y);
            }

            LogInfo(LogBlockStateBuilder, "Successfully compiled model: %s (faces=%zu)",
                    variant.model.ToString().c_str(), compiledMesh->faces.size());

            // Store the compiled mesh in the variant for BlockState to access
            variant.compiledMesh = compiledMesh;
        }
        else
        {
            LogWarn(LogBlockStateBuilder, "Failed to compile model: %s",
                    variant.model.ToString().c_str());
        }
    }
    catch (const std::exception& e)
    {
        LogError(LogBlockStateBuilder, "Exception while compiling model %s: %s",
                 variant.model.ToString().c_str(), e.what());
    }
}
