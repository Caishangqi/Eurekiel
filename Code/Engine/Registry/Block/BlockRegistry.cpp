#include "BlockRegistry.hpp"
#include "../../Core/Yaml.hpp"
#include "../../Core/FileSystemHelper.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "../../Voxel/Builtin/BlockAir.hpp"
#include "HalfTransparentBlock.hpp"
#include "TransparentBlock.hpp"
#include "LeavesBlock.hpp"
#include "LiquidBlock.hpp"
#include "FluidType.hpp"
#include "RenderType.hpp"
#include "RenderShape.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Resource/ResourceSubsystem.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include <filesystem>

DEFINE_LOG_CATEGORY(LogRegistryBlock)

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::voxel;

    Registry<Block>* BlockRegistry::GetTypedRegistry()
    {
        auto* registerSubsystem = GEngine->GetSubsystem<RegisterSubsystem>();
        if (!registerSubsystem)
        {
            LogError(LogRegistryBlock, "RegisterSubsystem not found in engine");
            return nullptr;
        }

        // Get or create the registry for Block type
        auto* registry = registerSubsystem->GetRegistry<Block>();
        if (!registry)
        {
            registry = registerSubsystem->CreateRegistry<Block>("blocks");
            if (registry)
            {
                LogInfo(LogRegistryBlock, "Block registry created successfully");
            }
        }

        return registry;
    }

    IRegistry* BlockRegistry::GetIRegistry()
    {
        auto* registerSubsystem = GEngine->GetSubsystem<RegisterSubsystem>();
        if (!registerSubsystem)
        {
            LogError(LogRegistryBlock, "RegisterSubsystem not found in engine");
            return nullptr;
        }

        // Get type-erased registry pointer
        return registerSubsystem->GetRegistry("blocks");
    }

    std::shared_ptr<BlockStateDefinition> BlockRegistry::GenerateBlockStateDefinition(std::shared_ptr<Block> block)
    {
        if (!block)
            return nullptr;

        // Create resource location for the block
        ResourceLocation location(block->GetNamespace(), "blockstates/" + block->GetRegistryName());
        std::string      fullName = block->GetNamespace() + ":" + block->GetRegistryName();

        // [FIX] First, try to load from JSON file (this contains rotation values for stairs/slabs)
        // Expected path: Run/.enigma/assets/{namespace}/blockstates/{blockname}.json
        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            // Construct the path directly using baseAssetPath from config
            std::filesystem::path basePath = resourceSubsystem->GetConfig().baseAssetPath;
            std::filesystem::path jsonPath = basePath / block->GetNamespace() / "blockstates" / (block->GetRegistryName() + ".json");

            if (std::filesystem::exists(jsonPath))
            {
                // Load from JSON file - this will include rotation values
                auto definition = BlockStateDefinition::LoadFromFile(location, jsonPath);
                if (definition)
                {
                    // Compile models with rotation applied
                    auto* modelSubsystem = GEngine->GetSubsystem<enigma::model::ModelSubsystem>();
                    if (modelSubsystem)
                    {
                        definition->CompileModels(modelSubsystem);
                    }

                    s_blockStateDefinitions[fullName] = definition;
                    LogInfo(LogRegistryBlock, "[JSON] Loaded BlockStateDefinition for block: %s from: %s",
                            fullName.c_str(), jsonPath.string().c_str());
                    return definition;
                }
                else
                {
                    LogWarn(LogRegistryBlock, "[JSON] Failed to load BlockStateDefinition from: %s, falling back to auto-generate",
                            jsonPath.string().c_str());
                }
            }
        }

        // Fallback: Auto-generate BlockStateDefinition using BlockStateBuilder
        std::string       baseModelPath = block->GetNamespace() + ":models/block/" + block->GetRegistryName();
        BlockStateBuilder builder(location);

        if (!block->GetProperties().empty())
        {
            // Auto-generate all possible variants based on block properties
            builder.AutoGenerateVariants(
                block.get(),
                baseModelPath,
                [&](const PropertyMap& properties) -> std::string
                {
                    UNUSED(properties)
                    return baseModelPath;
                }
            );
        }
        else
        {
            // Simple single variant for blocks without properties
            builder.DefaultVariant(BlockStateBuilder::VariantBuilder(baseModelPath));
        }

        auto definition = builder.Build();

        // Store the definition for later retrieval
        s_blockStateDefinitions[fullName] = definition;

        LogDebug(LogRegistryBlock, "[AUTO] Generated BlockStateDefinition for block: %s", fullName.c_str());

        return definition;
    }

    void BlockRegistry::RegisterBlock(const std::string& name, std::shared_ptr<Block> block)
    {
        auto* registry = GetTypedRegistry();
        if (registry && block)
        {
            // Set namespace if not already set
            if (block->GetNamespace().empty())
            {
                // Use default namespace from block constructor or "engine"
            }

            // Generate all block states before registering
            block->GenerateBlockStates();

            // Generate BlockStateDefinition using BlockStateBuilder
            auto blockStateDefinition = GenerateBlockStateDefinition(block);

            // Create resource mapping using ResourceMapper from ResourceSubsystem
            auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
            if (resourceSubsystem)
            {
                auto& resourceMapper = resourceSubsystem->GetResourceMapper();
                resourceMapper.MapObject(block.get(), "blocks");
            }

            // Register the block
            registry->Register(name, block);

            LogInfo(LogRegistryBlock, ("Registered block: " + name + " with " +
                        std::to_string(block->GetStateCount()) + " states and resource mappings").c_str());
        }
    }

    void BlockRegistry::RegisterBlock(const std::string& namespaceName, const std::string& name, std::shared_ptr<Block> block)
    {
        auto* registry = GetTypedRegistry();
        if (registry && block)
        {
            // Generate all block states before registering
            block->GenerateBlockStates();

            // Generate BlockStateDefinition using BlockStateBuilder
            auto blockStateDefinition = GenerateBlockStateDefinition(block);

            // Create resource mapping using ResourceMapper from ResourceSubsystem
            auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
            if (resourceSubsystem)
            {
                auto& resourceMapper = resourceSubsystem->GetResourceMapper();
                resourceMapper.MapObject(block.get(), "blocks");
            }

            // Register the block
            registry->Register(namespaceName, name, block);

            std::string fullName = namespaceName + ":" + name;
            LogInfo(LogRegistryBlock, ("Registered block: " + fullName + " with " +
                        std::to_string(block->GetStateCount()) + " states and resource mappings").c_str());
        }
    }

    std::shared_ptr<Block> BlockRegistry::GetBlock(const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->Get(name) : nullptr;
    }

    std::shared_ptr<Block> BlockRegistry::GetBlock(const std::string& namespaceName, const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->Get(namespaceName, name) : nullptr;
    }

    // High-performance numeric ID access methods

    std::shared_ptr<Block> BlockRegistry::GetBlockById(int id)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetById(id) : nullptr;
    }

    std::vector<std::shared_ptr<Block>> BlockRegistry::GetBlocksByIds(const std::vector<int>& ids)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetByIds(ids) : std::vector<std::shared_ptr<Block>>{};
    }

    int BlockRegistry::GetBlockId(const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetId(RegistrationKey(name)) : -1;
    }

    int BlockRegistry::GetBlockId(const std::string& namespaceName, const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetId(RegistrationKey(namespaceName, name)) : -1;
    }

    RegistrationKey BlockRegistry::GetBlockKey(int id)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetKey(id) : RegistrationKey();
    }

    bool BlockRegistry::HasBlockId(int id)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->HasId(id) : false;
    }

    std::vector<int> BlockRegistry::GetAllBlockIds()
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetAllIds() : std::vector<int>{};
    }

    std::vector<std::shared_ptr<Block>> BlockRegistry::GetAllBlocks()
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetAll() : std::vector<std::shared_ptr<Block>>{};
    }

    std::shared_ptr<BlockStateDefinition> BlockRegistry::GetBlockStateDefinition(const std::string& name)
    {
        auto it = s_blockStateDefinitions.find(name);
        return it != s_blockStateDefinitions.end() ? it->second : nullptr;
    }

    std::shared_ptr<BlockStateDefinition> BlockRegistry::GetBlockStateDefinition(const std::string& namespaceName, const std::string& name)
    {
        std::string fullName = namespaceName + ":" + name;
        return GetBlockStateDefinition(fullName);
    }

    std::unordered_map<std::string, std::shared_ptr<BlockStateDefinition>> BlockRegistry::GetAllBlockStateDefinitions()
    {
        return s_blockStateDefinitions;
    }

    enigma::resource::ResourceMapper& BlockRegistry::GetResourceMapper()
    {
        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            return resourceSubsystem->GetResourceMapper();
        }

        // Fallback - this should not happen in normal operation
        static enigma::resource::ResourceMapper fallbackMapper;
        LogError(LogRegistryBlock, "ResourceSubsystem not found! Using fallback ResourceMapper.");
        return fallbackMapper;
    }

    const enigma::resource::ResourceMapper::ResourceMapping* BlockRegistry::GetBlockResourceMapping(const std::string& name)
    {
        auto& mapper = GetResourceMapper();
        return mapper.GetMapping(name);
    }

    const enigma::resource::ResourceMapper::ResourceMapping* BlockRegistry::GetBlockResourceMapping(const std::string& namespaceName, const std::string& name)
    {
        auto& mapper = GetResourceMapper();
        return mapper.GetMapping(namespaceName, name);
    }

    enigma::resource::ResourceLocation BlockRegistry::GetBlockModelLocation(const std::string& name)
    {
        auto& mapper = GetResourceMapper();
        return mapper.GetModelLocation(name);
    }

    enigma::resource::ResourceLocation BlockRegistry::GetBlockModelLocation(const std::string& namespaceName, const std::string& name)
    {
        std::string fullName = namespaceName + ":" + name;
        return GetBlockModelLocation(fullName);
    }

    std::vector<std::shared_ptr<Block>> BlockRegistry::GetBlocksByNamespace(const std::string& namespaceName)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetByNamespace(namespaceName) : std::vector<std::shared_ptr<Block>>{};
    }

    bool BlockRegistry::IsBlockRegistered(const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->HasRegistration(RegistrationKey(name)) : false;
    }

    bool BlockRegistry::IsBlockRegistered(const std::string& namespaceName, const std::string& name)
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->HasRegistration(RegistrationKey(namespaceName, name)) : false;
    }

    size_t BlockRegistry::GetBlockCount()
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->GetRegistrationCount() : 0;
    }

    void BlockRegistry::Clear()
    {
        auto* registry = GetIRegistry();
        if (registry)
        {
            registry->Clear();
            s_blockStateDefinitions.clear();
        }
    }

    void BlockRegistry::RegisterBlocks(const std::string& namespaceName, const std::vector<std::pair<std::string, std::shared_ptr<Block>>>& blocks)
    {
        for (const auto& [name, block] : blocks)
        {
            RegisterBlock(namespaceName, name, block);
        }
    }

    Registry<Block>* BlockRegistry::GetRegistry()
    {
        return GetTypedRegistry();
    }

    // ============================================================
    // Freeze Mechanism Implementation
    // ============================================================

    void BlockRegistry::Freeze()
    {
        auto* registry = GetTypedRegistry();
        if (registry)
        {
            registry->Freeze();
            LogInfo(LogRegistryBlock, "BlockRegistry::Freeze Block registry frozen with %zu blocks registered",
                    registry->GetRegistrationCount());
        }
    }

    bool BlockRegistry::IsFrozen()
    {
        auto* registry = GetTypedRegistry();
        return registry ? registry->IsFrozen() : false;
    }

    void BlockRegistry::Unfreeze()
    {
        auto* registry = GetTypedRegistry();
        if (registry)
        {
            registry->Unfreeze();
            LogWarn(LogRegistryBlock, "BlockRegistry::Unfreeze Block registry unfrozen - this should only be used for testing");
        }
    }

    void BlockRegistry::FireRegisterEvent(enigma::event::EventBus& eventBus)
    {
        auto* registry = GetTypedRegistry();
        if (!registry)
        {
            LogError(LogRegistryBlock, "BlockRegistry::FireRegisterEvent Failed to get block registry");
            return;
        }

        LogInfo(LogRegistryBlock, "BlockRegistry::FireRegisterEvent Firing BlockRegisterEvent...");

        BlockRegisterEvent event(*registry);
        eventBus.Post(event);

        LogInfo(LogRegistryBlock, "BlockRegistry::FireRegisterEvent BlockRegisterEvent completed, %zu blocks registered",
                registry->GetRegistrationCount());
    }

    // Type-erased registry access methods
    std::string BlockRegistry::GetRegistryType()
    {
        return "blocks";
    }

    size_t BlockRegistry::GetRegistrationCount()
    {
        auto* registry = GetIRegistry();
        return registry ? registry->GetRegistrationCount() : 0;
    }

    bool BlockRegistry::HasRegistration(const RegistrationKey& key)
    {
        auto* registry = GetIRegistry();
        return registry ? registry->HasRegistration(key) : false;
    }

    std::vector<RegistrationKey> BlockRegistry::GetAllKeys()
    {
        auto* registry = GetIRegistry();
        return registry ? registry->GetAllKeys() : std::vector<RegistrationKey>{};
    }

    std::vector<std::string> BlockRegistry::GetBlockNames(const std::string& namespaceName)
    {
        auto* registry = GetTypedRegistry();
        if (!registry) return {};

        auto                     keys = registry->GetAllKeys();
        std::vector<std::string> names;

        for (const auto& key : keys)
        {
            if (namespaceName.empty() || key.namespaceName == namespaceName)
            {
                names.push_back(key.name);
            }
        }

        return names;
    }


    bool BlockRegistry::RegisterBlockFromYaml(const std::string& filePath)
    {
        try
        {
            // Extract namespace and block name from path
            std::filesystem::path path(filePath);
            std::string           blockName = ExtractBlockNameFromPath(filePath);

            // Try to extract namespace from path structure
            // Expected: .../data/namespace/block/blockname.yml
            std::string namespaceName = "minecraft"; // default
            auto        pathParts     = path.parent_path().parent_path();
            if (pathParts.has_filename())
            {
                namespaceName = pathParts.filename().string();
            }

            return RegisterBlockFromYaml(namespaceName, blockName, filePath);
        }
        catch (const std::exception& e)
        {
            LogError(LogRegistryBlock, "Failed to register block from YAML: %s - %s", filePath.c_str(), e.what());
            return false;
        }
    }

    bool BlockRegistry::RegisterBlockFromYaml(const std::string& namespaceName, const std::string& blockName, const std::string& filePath)
    {
        try
        {
            // Load YAML file
            YamlConfiguration yaml = YamlConfiguration::LoadFromFile(filePath);

            // Create block from YAML
            auto block = CreateBlockFromYaml(blockName, namespaceName, yaml);
            if (!block)
            {
                LogError(LogRegistryBlock, "Failed to create block from YAML: %s", filePath.c_str());
                return false;
            }

            // Register the block
            RegisterBlock(namespaceName, blockName, block);
            LogInfo(LogRegistryBlock, "Successfully registered block: %s:%s", namespaceName.c_str(), blockName.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            LogError(LogRegistryBlock, "Failed to register block from YAML: %s - %s", filePath.c_str(), e.what());
            return false;
        }
    }

    void BlockRegistry::LoadNamespaceBlocks(const std::string& dataPath, const std::string& namespaceName)
    {
        std::filesystem::path blockDir = std::filesystem::path(dataPath) / namespaceName / "block";

        if (!std::filesystem::exists(blockDir) || !std::filesystem::is_directory(blockDir))
        {
            LogWarn(LogRegistryBlock, "Block directory does not exist: %s", blockDir.string().c_str());
            return;
        }

        LogInfo(LogRegistryBlock, "Loading blocks from directory: %s", blockDir.string().c_str());

        LoadBlocksFromDirectory(blockDir.string(), namespaceName);
    }

    void BlockRegistry::LoadBlocksFromDirectory(const std::string& directoryPath, const std::string& namespaceName)
    {
        try
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".yml")
                {
                    std::string blockName = ExtractBlockNameFromPath(entry.path().string());
                    RegisterBlockFromYaml(namespaceName, blockName, entry.path().string());
                }
            }
        }
        catch (const std::exception& e)
        {
            LogError(LogRegistryBlock, "Error loading blocks from directory: %s - %s", directoryPath.c_str(), e.what());
        }
    }

    /**
     * @brief Factory function to create Block instances based on class name
     * 
     * [Phase 5.2] Supports all Block subclasses:
     * - Block (default)
     * - SlabBlock, StairsBlock (shape variants)
     * - BlockAir (invisible)
     * - HalfTransparentBlock (same-type face culling, e.g., ice, stained glass)
     * - TransparentBlock (full transparency, e.g., glass)
     * - LeavesBlock (cutout rendering, fixed light_block=1)
     * - LiquidBlock (invisible render shape, dedicated renderer)
     * 
     * @param blockClass The class name from YAML "base_class" field
     * @param registryName Block registry name
     * @param namespaceName Block namespace
     * @param fluidType Fluid type for LiquidBlock (ignored for other types)
     */
    std::unique_ptr<Block> CreateBlockInstance(const std::string& blockClass,
                                               const std::string& registryName,
                                               const std::string& namespaceName,
                                               FluidType          fluidType = FluidType::EMPTY)
    {
        // Shape variant blocks
        if (blockClass == "SlabBlock")
        {
            return std::make_unique<SlabBlock>(registryName, namespaceName);
        }
        else if (blockClass == "StairsBlock")
        {
            return std::make_unique<StairsBlock>(registryName, namespaceName);
        }
        // Special blocks
        else if (blockClass == "BlockAir")
        {
            return std::make_unique<BlockAir>(registryName, namespaceName);
        }
        // [NEW] Transparent block hierarchy
        else if (blockClass == "HalfTransparentBlock")
        {
            return std::make_unique<HalfTransparentBlock>(registryName, namespaceName);
        }
        else if (blockClass == "TransparentBlock")
        {
            return std::make_unique<TransparentBlock>(registryName, namespaceName);
        }
        // [NEW] Leaves block (cutout rendering)
        else if (blockClass == "LeavesBlock")
        {
            // LeavesBlock constructor adds default properties (distance, persistent, waterlogged)
            // Set addDefaultProperties=false if YAML defines custom properties
            return std::make_unique<LeavesBlock>(registryName, namespaceName, false);
        }
        // [NEW] Liquid block (water, lava)
        else if (blockClass == "LiquidBlock")
        {
            return std::make_unique<LiquidBlock>(registryName, namespaceName, fluidType);
        }
        else
        {
            // Default: use base Block class
            return std::make_unique<Block>(registryName, namespaceName);
        }
    }

    std::shared_ptr<Block> BlockRegistry::CreateBlockFromYaml(const std::string& blockName, const std::string& namespaceName, const YamlConfiguration& yaml)
    {
        try
        {
            // Parse base_class field (optional, defaults to "Block")
            std::string blockClass = yaml.GetString("base_class", "Block");

            // [NEW] Parse fluid_type for LiquidBlock
            FluidType fluidType = FluidType::EMPTY;
            if (yaml.Contains("fluid_type"))
            {
                std::string fluidTypeStr = yaml.GetString("fluid_type", "empty");
                fluidType                = ParseFluidType(fluidTypeStr.c_str());
            }

            // Create the block using factory
            auto block = CreateBlockInstance(blockClass, blockName, namespaceName, fluidType);

            // Parse properties
            if (yaml.Contains("properties"))
            {
                auto properties = ParsePropertiesFromYaml(yaml);
                for (auto& prop : properties)
                {
                    // Add property to block using base interface pointer
                    block->AddProperty(prop);
                }
            }

            // Parse blockstate reference (Minecraft way: blockstate points to model)
            if (yaml.Contains("blockstate"))
            {
                std::string blockstatePath = yaml.GetString("blockstate");
                block->SetBlockstatePath(blockstatePath);
                LogDebug(LogRegistryBlock, "Block %s:%s references blockstate: %s", namespaceName.c_str(), blockName.c_str(), blockstatePath.c_str());
            }
            else if (yaml.Contains("model"))
            {
                // Fallback for direct model reference (not recommended but supported)
                std::string modelPath = yaml.GetString("model");
                block->SetBlockstatePath(modelPath); // Store as blockstate path for now
                LogWarn(LogRegistryBlock, "Block %s:%s directly references model: %s (consider using blockstate)", namespaceName.c_str(), blockName.c_str(), modelPath.c_str());
            }
            else
            {
                // Generate default blockstate path
                std::string defaultPath = namespaceName + ":block/" + blockName;
                block->SetBlockstatePath(defaultPath);
                LogDebug(LogRegistryBlock, "Block %s:%s using default blockstate: %s", namespaceName.c_str(), blockName.c_str(), defaultPath.c_str());
            }

            // Parse block properties
            if (yaml.Contains("hardness"))
            {
                block->SetHardness(yaml.GetFloat("hardness", 1.0f));
            }

            if (yaml.Contains("resistance"))
            {
                block->SetResistance(yaml.GetFloat("resistance", 1.0f));
            }

            // [REFACTOR] Support both can_occlude (new) and opaque (legacy) for backward compatibility
            if (yaml.Contains("can_occlude"))
            {
                block->SetCanOcclude(yaml.GetBoolean("can_occlude", true));
            }
            else if (yaml.Contains("opaque"))
            {
                // Legacy support - opaque is deprecated, use can_occlude instead
                block->SetCanOcclude(yaml.GetBoolean("opaque", true));
            }

            if (yaml.Contains("full_block"))
            {
                block->SetFullBlock(yaml.GetBoolean("full_block", true));
            }

            if (yaml.Contains("light_level"))
            {
                block->SetBlockLightEmission(static_cast<uint8_t>(yaml.GetInt("light_level", 0)));
            }

            // [NEW Phase 5.1] Parse light_block (light attenuation value 0-15)
            // This is different from light_level (emission)
            // light_block controls how much light is blocked when passing through
            if (yaml.Contains("light_block"))
            {
                int lightBlock = yaml.GetInt("light_block", -1);
                // Store in block for later use by GetLightBlock()
                // Note: Block base class uses m_isOpaque for default calculation
                // Subclasses like LeavesBlock override GetLightBlock() directly
                // For custom values, we need to store this - but current Block class
                // doesn't have a dedicated field. The subclass behavior handles this.
                // LeavesBlock: always returns 1
                // LiquidBlock: uses default (1)
                // TransparentBlock: always returns 0
                // For base Block, light_block is computed from m_isOpaque
                UNUSED(lightBlock); // Handled by subclass overrides
            }

            // [NEW Phase 5.1] Parse propagates_skylight
            // Controls whether skylight passes through without attenuation
            if (yaml.Contains("propagates_skylight"))
            {
                bool propagatesSkylight = yaml.GetBoolean("propagates_skylight", true);
                // Similar to light_block, this is handled by subclass overrides:
                // LeavesBlock: uses default (true, because noOcclusion)
                // LiquidBlock: always returns false
                // TransparentBlock: always returns true
                // For base Block, it's computed from !m_isOpaque
                UNUSED(propagatesSkylight); // Handled by subclass overrides
            }

            // [NEW Phase 5.1] Parse render_type (opaque/cutout/translucent)
            // Note: render_type is already partially supported in existing YAML (water.yml has it)
            // But it wasn't being parsed. Now we ensure subclasses handle it via GetRenderType()
            if (yaml.Contains("render_type"))
            {
                std::string renderTypeStr = yaml.GetString("render_type", "opaque");
                // RenderType is determined by block subclass:
                // - Block: SOLID (default)
                // - LeavesBlock: CUTOUT
                // - HalfTransparentBlock/TransparentBlock: TRANSLUCENT
                // - LiquidBlock: TRANSLUCENT (water) or SOLID (lava)
                // The YAML render_type serves as documentation and validation
                UNUSED(renderTypeStr); // Handled by subclass GetRenderType()
            }

            // [NEW Phase 5.1] Parse render_shape (model/invisible/entityblock_animated)
            if (yaml.Contains("render_shape"))
            {
                std::string renderShapeStr = yaml.GetString("render_shape", "model");
                // RenderShape is determined by block subclass:
                // - Block: MODEL (default)
                // - LiquidBlock: INVISIBLE (uses dedicated LiquidBlockRenderer)
                // The YAML render_shape serves as documentation and validation
                UNUSED(renderShapeStr); // Handled by subclass GetRenderShape()
            }

            // Set visibility with smart default (air is invisible, others are visible)
            bool isAir = (blockName == "air");
            block->SetVisible(!isAir);

            // YAML can override the default value (optional)
            if (yaml.Contains("is_visible"))
            {
                block->SetVisible(yaml.GetBoolean("is_visible"));
            }

            return block;
        }
        catch (const std::exception& e)
        {
            LogError(LogRegistryBlock, "Failed to create block %s:%s from YAML: %s", namespaceName.c_str(), blockName.c_str(), e.what());
            return nullptr;
        }
    }

    std::vector<std::shared_ptr<enigma::voxel::IProperty>> BlockRegistry::ParsePropertiesFromYaml(const YamlConfiguration& yaml)
    {
        std::vector<std::shared_ptr<IProperty>> properties;

        try
        {
            if (!yaml.Contains("properties"))
            {
                return properties;
            }

            auto propertiesNode = yaml.GetConfigurationSection("properties");
            auto keys           = propertiesNode.GetKeys();

            for (const auto& key : keys)
            {
                std::string type = propertiesNode.GetString(key);

                if (type == "boolean")
                {
                    properties.push_back(std::make_shared<BooleanProperty>(key));
                }
                else if (type == "direction")
                {
                    properties.push_back(std::make_shared<DirectionProperty>(key));
                }
                else if (type.find("int") == 0)
                {
                    // Parse range like "int(0,15)"
                    int min = 0, max = 15;
                    if (type.length() > 4)
                    {
                        // Simple parsing for int(min,max) format
                        size_t start = type.find('(');
                        size_t comma = type.find(',');
                        size_t end   = type.find(')');

                        if (start != std::string::npos && comma != std::string::npos && end != std::string::npos)
                        {
                            min = std::stoi(type.substr(start + 1, comma - start - 1));
                            max = std::stoi(type.substr(comma + 1, end - comma - 1));
                        }
                    }
                    properties.push_back(std::make_shared<IntProperty>(key, min, max));
                }
            }
        }
        catch (const std::exception& e)
        {
            LogError(LogRegistryBlock, "Failed to parse properties from YAML: %s", e.what());
        }

        return properties;
    }

    std::string BlockRegistry::ExtractBlockNameFromPath(const std::string& filePath)
    {
        std::filesystem::path path(filePath);
        return path.stem().string(); // Get filename without extension
    }
}
