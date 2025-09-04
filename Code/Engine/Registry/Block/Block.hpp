#pragma once
#include "../Core/IRegistrable.hpp"
#include "../../Voxel/Property/Property.hpp"
#include "../../Voxel/Property/PropertyMap.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::model
{
    class ModelSubsystem;
}

namespace enigma::resource
{
    class AtlasManager;
}

namespace enigma::voxel::block
{
    class BlockState;
    struct BlockPos;
}

namespace enigma::voxel::world
{
    class World;
}

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::voxel::property;
    /**
     * @brief Private implementation class for Block
     * This allows us to hide the BlockState vector from the header
     */
    class BlockImpl;

    /**
     * @brief Base class for all block types
     * 
     * Similar to Minecraft's Block class, this defines the behavior and properties
     * of a block type. Individual BlockState instances represent specific configurations.
     */
    class Block : public IRegistrable
    {
    protected:
        std::string                             m_registryName;
        std::string                             m_namespace;
        std::vector<std::shared_ptr<IProperty>> m_properties;

        // Use Pimpl to hide BlockState vector
        std::unique_ptr<BlockImpl> m_impl;

        // Block behavior properties
        float       m_hardness    = 1.0f;
        float       m_resistance  = 1.0f;
        bool        m_isOpaque    = true;
        bool        m_isFullBlock = true;
        std::string m_blockstatePath; // Path to blockstate definition

    public:
        explicit Block(const std::string& registryName, const std::string& namespaceName = "");
        ~Block() override;

        // IRegistrable interface
        const std::string& GetRegistryName() const override { return m_registryName; }
        const std::string& GetNamespace() const override { return m_namespace; }

        // Property management
        template <typename T>
        void AddProperty(std::shared_ptr<Property<T>> property)
        {
            m_properties.push_back(property);
        }

        void AddProperty(std::shared_ptr<IProperty> property)
        {
            m_properties.push_back(property);
        }

        const std::vector<std::shared_ptr<IProperty>>& GetProperties() const { return m_properties; }

        /**
         * @brief Generate all possible BlockState combinations
         * Must be called after all properties are added
         */
        void GenerateBlockStates();

        /**
         * @brief Get all possible states for this block
         */
        std::vector<enigma::voxel::block::BlockState*> GetAllStates() const;

        /**
         * @brief Get the default state (usually first state)
         */
        enigma::voxel::block::BlockState* GetDefaultState() const;

        /**
         * @brief Get a specific state by property values
         */
        enigma::voxel::block::BlockState* GetState(const PropertyMap& properties) const;

        /**
         * @brief Get state by index (for fast lookup)
         */
        enigma::voxel::block::BlockState* GetStateByIndex(size_t index) const;

        size_t GetStateCount() const;

        // Block behavior properties
        float GetHardness() const { return m_hardness; }
        void  SetHardness(float hardness) { m_hardness = hardness; }

        float GetResistance() const { return m_resistance; }
        void  SetResistance(float resistance) { m_resistance = resistance; }

        bool IsOpaque() const { return m_isOpaque; }
        void SetOpaque(bool opaque) { m_isOpaque = opaque; }

        bool IsFullBlock() const { return m_isFullBlock; }
        void SetFullBlock(bool fullBlock) { m_isFullBlock = fullBlock; }

        // Blockstate path management
        const std::string& GetBlockstatePath() const { return m_blockstatePath; }
        void               SetBlockstatePath(const std::string& path) { m_blockstatePath = path; }

        // Virtual methods for block behavior
        virtual void OnPlaced(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
        }

        virtual void OnBroken(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
        }

        virtual void OnNeighborChanged(enigma::voxel::world::World* world, const enigma::voxel::block::BlockPos& pos, enigma::voxel::block::BlockState* state, Block* neighborBlock)
        {
            UNUSED(world);
            UNUSED(pos);
            UNUSED(state);
            UNUSED(neighborBlock);
        }

        /**
         * @brief Compile models for all block states
         * 
         * Should be called after GenerateBlockStates() and after ModelRegistry
         * and AtlasManager are initialized. This compiles the model for each
         * BlockState and caches the result.
         * 
         * @param modelRegistry Registry for loading models
         * @param atlasManager Manager for texture atlas access  
         */
        void CompileModels(enigma::model::ModelSubsystem*        modelSubsystem,
                           const enigma::resource::AtlasManager* atlasManager) const;

        /**
         * @brief Get the model path for a specific state
         * Override this to provide custom model selection logic
         */
        virtual std::string GetModelPath(enigma::voxel::block::BlockState* state) const;

    protected:
        /**
         * @brief Called during state generation to initialize each state
         * Override to add custom state initialization
         */
        virtual void InitializeState(enigma::voxel::block::BlockState* state, const PropertyMap& properties)
        {
            UNUSED(state);
            UNUSED(properties);
        }

    private:
        void GenerateStatesRecursive(size_t propertyIndex, PropertyMap& currentMap, std::vector<PropertyMap>& allCombinations);
    };
}
