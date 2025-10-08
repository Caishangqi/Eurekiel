#pragma once
#include "../Core/Registry.hpp"
#include "../Core/RegisterSubsystem.hpp"
#include "../../Core/Engine.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Resource/BlockState/BlockStateBuilder.hpp"
#include "../../Resource/BlockState/BlockStateDefinition.hpp"
#include "../../Resource/ResourceMapper.hpp"
#include "Block.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::resource::blockstate;

    /**
     * @brief Specialized registry for Block types
     * 
     * Uses the engine's RegisterSubsystem to manage Block registrations.
     * Provides convenient methods for registering and accessing blocks.
     * Automatically handles block state generation and BlockStateDefinition creation.
     */
    class BlockRegistry
    {
    private:
        static inline std::unordered_map<std::string, std::shared_ptr<BlockStateDefinition>> s_blockStateDefinitions;

        // Get the underlying typed registry from RegisterSubsystem
        static Registry<Block>* GetTypedRegistry();

        // Get IRegistry pointer for type-erased operations
        static IRegistry* GetIRegistry();


        /**
         * @brief Generate BlockStateDefinition for a block using BlockStateBuilder
         */
        static std::shared_ptr<BlockStateDefinition> GenerateBlockStateDefinition(std::shared_ptr<Block> block);

    public:
        /**
         * @brief Register a block type
         * Automatically generates BlockStates, BlockStateDefinitions, and ResourceMappings
         */
        static void RegisterBlock(const std::string& name, std::shared_ptr<Block> block);

        /**
         * @brief Register a block type with namespace
         * Automatically generates BlockStates, BlockStateDefinitions, and ResourceMappings
         */
        static void RegisterBlock(const std::string& namespaceName, const std::string& name, std::shared_ptr<Block> block);

        /**
         * @brief Get a block by name
         */
        static std::shared_ptr<Block> GetBlock(const std::string& name);

        /**
         * @brief Get a block by namespace and name
         */
        static std::shared_ptr<Block> GetBlock(const std::string& namespaceName, const std::string& name);

        // High-performance numeric ID access methods

        /**
         * @brief Get a block by numeric ID (O(1) performance)
         */
        static std::shared_ptr<Block> GetBlockById(int id);

        /**
         * @brief Get multiple blocks by numeric IDs (batch operation)
         */
        static std::vector<std::shared_ptr<Block>> GetBlocksByIds(const std::vector<int>& ids);

        /**
         * @brief Get numeric ID for a block by name
         */
        static int GetBlockId(const std::string& name);

        /**
         * @brief Get numeric ID for a block by namespace and name
         */
        static int GetBlockId(const std::string& namespaceName, const std::string& name);

        /**
         * @brief Get RegistrationKey for a block ID
         */
        static RegistrationKey GetBlockKey(int id);

        /**
         * @brief Check if a numeric ID exists
         */
        static bool HasBlockId(int id);

        /**
         * @brief Get all valid block IDs
         */
        static std::vector<int> GetAllBlockIds();

        /**
         * @brief Get all registered blocks
         */
        static std::vector<std::shared_ptr<Block>> GetAllBlocks();

        /**
         * @brief Get BlockStateDefinition for a registered block
         */
        static std::shared_ptr<BlockStateDefinition> GetBlockStateDefinition(const std::string& name);

        /**
         * @brief Get BlockStateDefinition for a registered block with namespace
         */
        static std::shared_ptr<BlockStateDefinition> GetBlockStateDefinition(const std::string& namespaceName, const std::string& name);

        /**
         * @brief Get all BlockStateDefinitions
         */
        static std::unordered_map<std::string, std::shared_ptr<BlockStateDefinition>> GetAllBlockStateDefinitions();

        /**
         * @brief Get ResourceMapper instance for block resources
         */
        static enigma::resource::ResourceMapper& GetResourceMapper();

        /**
         * @brief Get resource mapping for a block
         */
        static const enigma::resource::ResourceMapper::ResourceMapping* GetBlockResourceMapping(const std::string& name);

        /**
         * @brief Get resource mapping for a block with namespace
         */
        static const enigma::resource::ResourceMapper::ResourceMapping* GetBlockResourceMapping(const std::string& namespaceName, const std::string& name);

        /**
         * @brief Get model location for a block
         */
        static enigma::resource::ResourceLocation GetBlockModelLocation(const std::string& name);

        /**
         * @brief Get model location for a block with namespace
         */
        static enigma::resource::ResourceLocation GetBlockModelLocation(const std::string& namespaceName, const std::string& name);

        /**
         * @brief Get blocks from a specific namespace
         */
        static std::vector<std::shared_ptr<Block>> GetBlocksByNamespace(const std::string& namespaceName);

        /**
         * @brief Check if a block is registered
         */
        static bool IsBlockRegistered(const std::string& name);

        /**
         * @brief Check if a block is registered with namespace
         */
        static bool IsBlockRegistered(const std::string& namespaceName, const std::string& name);

        /**
         * @brief Get the total number of registered blocks
         */
        static size_t GetBlockCount();

        /**
         * @brief Clear all registered blocks (mainly for testing)
         */
        static void Clear();

        /**
         * @brief Get the underlying registry (for advanced usage)
         */
        static Registry<Block>* GetRegistry();

        // Type-erased registry access methods

        /**
         * @brief Get registry type name
         */
        static std::string GetRegistryType();

        /**
         * @brief Get registration count
         */
        static size_t GetRegistrationCount();

        /**
         * @brief Check if a key is registered (type-erased)
         */
        static bool HasRegistration(const RegistrationKey& key);

        /**
         * @brief Get all registration keys (type-erased)
         */
        static std::vector<RegistrationKey> GetAllKeys();

        // Convenience methods for common operations

        /**
         * @brief Register multiple blocks from a namespace
         */
        static void RegisterBlocks(const std::string& namespaceName, const std::vector<std::pair<std::string, std::shared_ptr<Block>>>& blocks);

        /**
         * @brief Get all block names from a namespace
         */
        static std::vector<std::string> GetBlockNames(const std::string& namespaceName = "");

        // YAML loading methods

        /**
         * @brief Register a block from YAML file
         */
        static bool RegisterBlockFromYaml(const std::string& filePath);

        /**
         * @brief Register a block from YAML file with explicit namespace and name
         */
        static bool RegisterBlockFromYaml(const std::string& namespaceName, const std::string& blockName, const std::string& filePath);

        /**
         * @brief Load all blocks from a namespace data directory
         * Expected structure: dataPath/namespace/block/*.yml
         */
        static void LoadNamespaceBlocks(const std::string& dataPath, const std::string& namespaceName);

        /**
         * @brief Load all blocks from a directory recursively
         */
        static void LoadBlocksFromDirectory(const std::string& directoryPath, const std::string& namespaceName = "");

    private:
        /**
         * @brief Create a Block from YAML configuration
         */
        static std::shared_ptr<Block> CreateBlockFromYaml(const std::string& blockName, const std::string& namespaceName, const enigma::core::YamlConfiguration& yaml);

        /**
         * @brief Parse properties from YAML
         */
        static std::vector<std::shared_ptr<enigma::voxel::IProperty>> ParsePropertiesFromYaml(const enigma::core::YamlConfiguration& yaml);

        /**
         * @brief Extract block name from file path
         */
        static std::string ExtractBlockNameFromPath(const std::string& filePath);
    };
}
