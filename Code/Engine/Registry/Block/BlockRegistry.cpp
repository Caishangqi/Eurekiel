#include "BlockRegistry.hpp"
#include "../../Core/Yaml.hpp"
#include "../../Core/FileUtils.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include <filesystem>

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::voxel::property;

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
            LogError("BlockRegistry", "Failed to register block from YAML: %s - %s", filePath.c_str(), e.what());
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
                LogError("BlockRegistry", "Failed to create block from YAML: %s", filePath.c_str());
                return false;
            }

            // Register the block
            RegisterBlock(namespaceName, blockName, block);
            LogInfo("BlockRegistry", "Successfully registered block: %s:%s", namespaceName.c_str(), blockName.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            LogError("BlockRegistry", "Failed to register block from YAML: %s - %s", filePath.c_str(), e.what());
            return false;
        }
    }

    void BlockRegistry::LoadNamespaceBlocks(const std::string& dataPath, const std::string& namespaceName)
    {
        std::filesystem::path blockDir = std::filesystem::path(dataPath) / namespaceName / "block";

        if (!std::filesystem::exists(blockDir) || !std::filesystem::is_directory(blockDir))
        {
            LogWarn("registry", "Block directory does not exist: %s", blockDir.string().c_str());
            return;
        }

        LogInfo("registry", "Loading blocks from directory: %s", blockDir.string().c_str());

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
            LogError("registry", "Error loading blocks from directory: %s - %s", directoryPath.c_str(), e.what());
        }
    }

    std::shared_ptr<Block> BlockRegistry::CreateBlockFromYaml(const std::string& blockName, const std::string& namespaceName, const YamlConfiguration& yaml)
    {
        try
        {
            // Create the block
            auto block = std::make_shared<Block>(blockName, namespaceName);

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
                LogDebug("registry", "Block %s:%s references blockstate: %s", namespaceName.c_str(), blockName.c_str(), blockstatePath.c_str());
            }
            else if (yaml.Contains("model"))
            {
                // Fallback for direct model reference (not recommended but supported)
                std::string modelPath = yaml.GetString("model");
                block->SetBlockstatePath(modelPath); // Store as blockstate path for now
                LogWarn("registry", "Block %s:%s directly references model: %s (consider using blockstate)", namespaceName.c_str(), blockName.c_str(), modelPath.c_str());
            }
            else
            {
                // Generate default blockstate path
                std::string defaultPath = namespaceName + ":block/" + blockName;
                block->SetBlockstatePath(defaultPath);
                LogDebug("registry", "Block %s:%s using default blockstate: %s", namespaceName.c_str(), blockName.c_str(), defaultPath.c_str());
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

            if (yaml.Contains("opaque"))
            {
                block->SetOpaque(yaml.GetBoolean("opaque", true));
            }

            if (yaml.Contains("full_block"))
            {
                block->SetFullBlock(yaml.GetBoolean("full_block", true));
            }

            return block;
        }
        catch (const std::exception& e)
        {
            LogError("registry", "Failed to create block %s:%s from YAML: %s", namespaceName.c_str(), blockName.c_str(), e.what());
            return nullptr;
        }
    }

    std::vector<std::shared_ptr<enigma::voxel::property::IProperty>> BlockRegistry::ParsePropertiesFromYaml(const YamlConfiguration& yaml)
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
            LogError("registry", "Failed to parse properties from YAML: %s", e.what());
        }

        return properties;
    }

    std::string BlockRegistry::ExtractBlockNameFromPath(const std::string& filePath)
    {
        std::filesystem::path path(filePath);
        return path.stem().string(); // Get filename without extension
    }
}
