#include "Block.hpp"
#include "../../Voxel/Block/BlockState.hpp"
#include <algorithm>

namespace enigma::registry::block
{
    /**
     * @brief Private implementation for Block class
     */
    class BlockImpl
    {
    public:
        std::vector<std::unique_ptr<enigma::voxel::block::BlockState>> allStates;
        enigma::voxel::block::BlockState*                              defaultState = nullptr;
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
            auto        state = std::make_unique<enigma::voxel::block::BlockState>(this, emptyMap, 0);
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
            auto state = std::make_unique<enigma::voxel::block::BlockState>(this, allCombinations[i], i);
            InitializeState(state.get(), allCombinations[i]);

            if (i == 0)
            {
                m_impl->defaultState = state.get();
            }

            m_impl->allStates.push_back(std::move(state));
        }
    }

    enigma::voxel::block::BlockState* Block::GetState(const PropertyMap& properties) const
    {
        // Linear search through states to find matching properties
        // This could be optimized with a hash map if needed
        for (const auto& state : m_impl->allStates)
        {
            if (state->GetProperties() == properties)
            {
                return state.get();
            }
        }

        // Return default state if not found
        return m_impl->defaultState;
    }

    enigma::voxel::block::BlockState* Block::GetDefaultState() const
    {
        return m_impl->defaultState;
    }

    enigma::voxel::block::BlockState* Block::GetStateByIndex(size_t index) const
    {
        return index < m_impl->allStates.size() ? m_impl->allStates[index].get() : nullptr;
    }

    size_t Block::GetStateCount() const
    {
        return m_impl->allStates.size();
    }

    std::vector<enigma::voxel::block::BlockState*> Block::GetAllStates() const
    {
        std::vector<enigma::voxel::block::BlockState*> states;
        states.reserve(m_impl->allStates.size());
        for (const auto& state : m_impl->allStates)
        {
            states.push_back(state.get());
        }
        return states;
    }

    std::string Block::GetModelPath(enigma::voxel::block::BlockState* state) const
    {
        UNUSED(state);

        // Use stored blockstate path if available
        if (!m_blockstatePath.empty())
        {
            return m_blockstatePath;
        }

        // Default implementation: namespace:block/registryname
        std::string path = m_namespace.empty() ? "minecraft" : m_namespace;
        path += ":block/" + m_registryName;
        return path;
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

            // We need to set the value in the map, but PropertyMap::Set is templated
            // For now, we'll store as std::any in a different structure
            // This is a limitation of the current design - we may need to refactor

            // For now, create a copy and recurse
            PropertyMap nextMap = currentMap;
            // TODO: Set the value in nextMap (requires template dispatch)

            GenerateStatesRecursive(propertyIndex + 1, nextMap, allCombinations);
        }
    }
}
