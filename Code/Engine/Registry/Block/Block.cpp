#include "Block.hpp"
#include "BlockRegistry.hpp"
#include "../../Voxel/Block/BlockState.hpp"
#include "../../Model/ModelSubsystem.hpp"
#include "../../Resource/Atlas/AtlasManager.hpp"
#include "../../Resource/BlockState/BlockStateDefinition.hpp"
#include <algorithm>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::registry::block
{
    /**
     * @brief Private implementation for Block class
     */
    class BlockImpl
    {
    public:
        std::vector<std::unique_ptr<enigma::voxel::BlockState>> allStates;
        enigma::voxel::BlockState*                              defaultState = nullptr;
    };

    Block::Block(const std::string& registryName, const std::string& namespaceName)
        : m_registryName(registryName), m_namespace(namespaceName), m_impl(std::make_unique<BlockImpl>())
    {
    }

    Block::~Block() = default;

    void Block::GenerateBlockStates()
    {
        if (m_properties.empty())
        {
            // No properties, create single default state
            PropertyMap emptyMap;
            auto        state = std::make_unique<enigma::voxel::BlockState>(this, emptyMap, 0);
            InitializeState(state.get(), emptyMap);
            m_impl->defaultState = state.get();
            m_impl->allStates.push_back(std::move(state));
            return;
        }

        // Generate all possible combinations of property values
        std::vector<PropertyMap> allCombinations;
        PropertyMap              currentMap;
        GenerateStatesRecursive(0, currentMap, allCombinations);

        // Create BlockState objects for each combination
        m_impl->allStates.reserve(allCombinations.size());
        for (size_t i = 0; i < allCombinations.size(); ++i)
        {
            auto state = std::make_unique<enigma::voxel::BlockState>(this, allCombinations[i], i);
            InitializeState(state.get(), allCombinations[i]);

            if (i == 0)
            {
                m_impl->defaultState = state.get();
            }

            m_impl->allStates.push_back(std::move(state));
        }
    }

    enigma::voxel::BlockState* Block::GetState(const PropertyMap& properties) const
    {
        // [DEBUG] Track slab/stairs state lookup
        bool isDebugBlock = (m_registryName.find("slab") != std::string::npos ||
            m_registryName.find("stairs") != std::string::npos);

        if (isDebugBlock)
        {
            core::LogInfo("Block", "[DEBUG] GetState() for %s:%s, looking for properties='%s'",
                          m_namespace.c_str(), m_registryName.c_str(), properties.ToString().c_str());
            core::LogInfo("Block", "  Total states available: %zu", m_impl->allStates.size());
        }

        // Linear search through states to find matching properties
        // This could be optimized with a hash map if needed
        for (size_t i = 0; i < m_impl->allStates.size(); ++i)
        {
            const auto& state = m_impl->allStates[i];

            if (isDebugBlock)
            {
                core::LogInfo("Block", "  State[%zu]: '%s' %s",
                              i,
                              state->GetProperties().ToString().c_str(),
                              (state->GetProperties() == properties) ? "[MATCH]" : "");
            }

            if (state->GetProperties() == properties)
            {
                if (isDebugBlock)
                {
                    core::LogInfo("Block", "  [FOUND] Returning state[%zu]", i);
                }
                return state.get();
            }
        }

        // Return default state if not found
        if (isDebugBlock)
        {
            core::LogWarn("Block", "  [NOT FOUND] Returning default state with properties='%s'",
                          m_impl->defaultState ? m_impl->defaultState->GetProperties().ToString().c_str() : "NULL");
        }
        return m_impl->defaultState;
    }

    enigma::voxel::BlockState* Block::GetDefaultState() const
    {
        return m_impl->defaultState;
    }

    enigma::voxel::BlockState* Block::GetStateByIndex(size_t index) const
    {
        return index < m_impl->allStates.size() ? m_impl->allStates[index].get() : nullptr;
    }

    size_t Block::GetStateCount() const
    {
        return m_impl->allStates.size();
    }

    std::vector<enigma::voxel::BlockState*> Block::GetAllStates() const
    {
        std::vector<enigma::voxel::BlockState*> states;
        states.reserve(m_impl->allStates.size());
        for (const auto& state : m_impl->allStates)
        {
            states.push_back(state.get());
        }
        return states;
    }

    std::string Block::GetModelPath(enigma::voxel::BlockState* state) const
    {
        UNUSED(state)
        // Use stored blockstate path if available
        if (!m_blockstatePath.empty())
        {
            return m_blockstatePath;
        }

        // Default implementation: namespace:models/block/registryname
        std::string path = m_namespace.empty() ? "minecraft" : m_namespace;
        path             += ":models/block/" + m_registryName;
        return path;
    }

    // [NEW] Default implementations for advanced placement and behavior methods

    enigma::voxel::BlockState* Block::GetStateForPlacement(const PlacementContext& ctx) const
    {
        UNUSED(ctx);
        // Default: ignore placement context, use default state
        return GetDefaultState();
    }

    bool Block::IsOpaque(enigma::voxel::BlockState* state) const
    {
        UNUSED(state);
        // Default: use block-level opaque flag
        return m_isOpaque;
    }

    bool Block::CanBeReplaced(enigma::voxel::BlockState* state, const PlacementContext& ctx) const
    {
        UNUSED(state);
        UNUSED(ctx);
        // Default: blocks cannot be replaced
        return false;
    }

    void Block::CompileModels(enigma::model::ModelSubsystem*        modelSubsystem,
                              const enigma::resource::AtlasManager* atlasManager) const
    {
        UNUSED(atlasManager);

        if (!modelSubsystem)
        {
            LogError("Block", "Cannot compile models: missing ModelSubsystem");
            return;
        }

        LogInfo("Block", "========================================");
        LogInfo("Block", "[CompileModels] Block: %s:%s (states: %zu)",
                m_namespace.c_str(), GetRegistryName().c_str(), m_impl->allStates.size());
        LogInfo("Block", "  blockstatePath: '%s'", m_blockstatePath.c_str());

        // [NEW] Delegate to BlockStateDefinition::CompileModels() which applies rotation
        // This follows Minecraft's FaceBakery pattern where rotation is applied during baking
        std::string fullName      = m_namespace + ":" + m_registryName;
        auto        blockStateDef = BlockRegistry::GetBlockStateDefinition(fullName);

        if (!blockStateDef)
        {
            LogWarn("Block", "  [WARN] No BlockStateDefinition found for: %s", fullName.c_str());
            LogInfo("Block", "========================================");
            return;
        }

        // Compile models via BlockStateDefinition (applies rotation from JSON)
        blockStateDef->CompileModels(modelSubsystem);

        LogInfo("Block", "[CompileModels] Complete for %s (delegated to BlockStateDefinition)",
                GetRegistryName().c_str());
        LogInfo("Block", "========================================");
    }

    void Block::GenerateStatesRecursive(size_t propertyIndex, PropertyMap& currentMap, std::vector<PropertyMap>& allCombinations)
    {
        if (propertyIndex >= m_properties.size())
        {
            // Reached the end, add this combination
            allCombinations.push_back(currentMap);
            return;
        }

        auto& property        = m_properties[propertyIndex];
        auto  possibleStrings = property->GetPossibleValuesAsStrings();

        for (const std::string& valueStr : possibleStrings)
        {
            // Convert string back to typed value and set in map
            std::any value = property->StringToValue(valueStr);

            // [FIX] Use SetAny() to set the value in the PropertyMap
            PropertyMap nextMap = currentMap;
            nextMap.SetAny(property, value);

            GenerateStatesRecursive(propertyIndex + 1, nextMap, allCombinations);
        }
    }
}
