#pragma once
#include "../Core/Registry.hpp"
#include "../Core/RegisterSubsystem.hpp"
#include "../../Core/Engine.hpp"
#include "Block.hpp"
#include <memory>
#include <string>

namespace enigma::registry::block
{
    using namespace enigma::core;
    
    /**
     * @brief Specialized registry for Block types
     * 
     * Provides convenient methods for registering and accessing blocks.
     * Automatically handles block state generation.
     */
    class BlockRegistry
    {
    private:
        static inline Registry<Block>* s_registry = nullptr;
        
        // Ensure registry is initialized
        static Registry<Block>* GetOrCreateRegistry()
        {
            if (!s_registry)
            {
                auto* registerSubsystem = GEngine->GetSubsystem<RegisterSubsystem>();
                if (registerSubsystem)
                {
                    s_registry = registerSubsystem->CreateRegistry<Block>("blocks");
                }
            }
            return s_registry;
        }
        
    public:
        /**
         * @brief Register a block type
         */
        static void RegisterBlock(const std::string& name, std::shared_ptr<Block> block)
        {
            auto* registry = GetOrCreateRegistry();
            if (registry && block)
            {
                // Generate all block states before registering
                block->GenerateBlockStates();
                registry->Register(name, block);
            }
        }
        
        /**
         * @brief Register a block type with namespace
         */
        static void RegisterBlock(const std::string& namespaceName, const std::string& name, std::shared_ptr<Block> block)
        {
            auto* registry = GetOrCreateRegistry();
            if (registry && block)
            {
                // Generate all block states before registering
                block->GenerateBlockStates();
                registry->Register(namespaceName, name, block);
            }
        }
        
        /**
         * @brief Get a block by name
         */
        static std::shared_ptr<Block> GetBlock(const std::string& name)
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->Get(name) : nullptr;
        }
        
        /**
         * @brief Get a block by namespace and name
         */
        static std::shared_ptr<Block> GetBlock(const std::string& namespaceName, const std::string& name)
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->Get(namespaceName, name) : nullptr;
        }
        
        /**
         * @brief Get all registered blocks
         */
        static std::vector<std::shared_ptr<Block>> GetAllBlocks()
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->GetAll() : std::vector<std::shared_ptr<Block>>{};
        }
        
        /**
         * @brief Get blocks from a specific namespace
         */
        static std::vector<std::shared_ptr<Block>> GetBlocksByNamespace(const std::string& namespaceName)
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->GetByNamespace(namespaceName) : std::vector<std::shared_ptr<Block>>{};
        }
        
        /**
         * @brief Check if a block is registered
         */
        static bool IsBlockRegistered(const std::string& name)
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->HasRegistration(RegistrationKey(name)) : false;
        }
        
        /**
         * @brief Check if a block is registered with namespace
         */
        static bool IsBlockRegistered(const std::string& namespaceName, const std::string& name)
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->HasRegistration(RegistrationKey(namespaceName, name)) : false;
        }
        
        /**
         * @brief Get the total number of registered blocks
         */
        static size_t GetBlockCount()
        {
            auto* registry = GetOrCreateRegistry();
            return registry ? registry->GetRegistrationCount() : 0;
        }
        
        /**
         * @brief Clear all registered blocks (mainly for testing)
         */
        static void Clear()
        {
            auto* registry = GetOrCreateRegistry();
            if (registry)
            {
                registry->Clear();
            }
        }
        
        /**
         * @brief Get the underlying registry (for advanced usage)
         */
        static Registry<Block>* GetRegistry()
        {
            return GetOrCreateRegistry();
        }
        
        // Convenience methods for common operations
        
        /**
         * @brief Register multiple blocks from a namespace
         */
        static void RegisterBlocks(const std::string& namespaceName, const std::vector<std::pair<std::string, std::shared_ptr<Block>>>& blocks)
        {
            for (const auto& [name, block] : blocks)
            {
                RegisterBlock(namespaceName, name, block);
            }
        }
        
        /**
         * @brief Get all block names from a namespace
         */
        static std::vector<std::string> GetBlockNames(const std::string& namespaceName = "")
        {
            auto* registry = GetOrCreateRegistry();
            if (!registry) return {};
            
            auto keys = registry->GetAllKeys();
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
    };
}
