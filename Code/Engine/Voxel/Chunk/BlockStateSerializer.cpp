#include "BlockStateSerializer.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Voxel/Property/PropertyRegistry.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <cstring>
#include <algorithm>

namespace enigma::voxel::chunk
{
    using namespace enigma::registry::block;
    using namespace enigma::voxel::property;

    // StateMapping implementation
    uint32_t BlockStateSerializer::StateMapping::GetStateID(BlockState* state)
    {
        if (!state)
            return 0; // Air/null state

        auto it = m_stateToID.find(state);
        if (it != m_stateToID.end())
            return it->second;

        // Use BlockState's inherent stateIndex if available
        uint32_t stateID = static_cast<uint32_t>(state->GetStateIndex());

        // If stateIndex is 0 or conflicts, generate temporary ID
        if (stateID == 0 || m_idToState.find(stateID) != m_idToState.end())
        {
            stateID = m_nextTempID++;
        }

        RegisterState(state, stateID);
        return stateID;
    }

    BlockState* BlockStateSerializer::StateMapping::GetState(uint32_t stateID)
    {
        if (stateID == 0)
            return nullptr; // Air/null state

        auto it = m_idToState.find(stateID);
        return (it != m_idToState.end()) ? it->second : nullptr;
    }

    void BlockStateSerializer::StateMapping::RegisterState(BlockState* state, uint32_t stateID)
    {
        if (!state || stateID == 0)
            return;

        m_stateToID[state]   = stateID;
        m_idToState[stateID] = state;
    }

    size_t BlockStateSerializer::StateMapping::SerializeStates(uint8_t* outputData, size_t outputSize)
    {
        if (!outputData || outputSize < sizeof(uint32_t))
            return 0;

        size_t offset = 0;

        // Write state count
        uint32_t stateCount = static_cast<uint32_t>(m_stateToID.size());
        std::memcpy(outputData + offset, &stateCount, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Serialize each state
        for (const auto& pair : m_stateToID)
        {
            BlockState* state   = pair.first;
            uint32_t    stateID = pair.second;

            if (offset >= outputSize)
                return 0; // Buffer overflow

            // Create serialized state entry
            SerializedBlockState serialized;
            serialized.stateID = stateID;

            if (state->GetBlock())
            {
                serialized.blockName = state->GetBlock()->GetRegistryName();
            }

            // Serialize properties
            const PropertyMap& properties   = state->GetProperties();
            auto               propertyList = properties.GetProperties();

            for (auto property : propertyList)
            {
                if (property)
                {
                    std::string name          = property->GetName();
                    std::any    propertyValue = properties.GetAny(property);
                    std::string value         = property->ValueToString(propertyValue);
                    serialized.properties.emplace_back(name, value);
                }
            }

            // Write serialized state to buffer
            size_t stateSize = SerializeState(state, outputData + offset, outputSize - offset);
            if (stateSize == 0)
                return 0; // Serialization failed

            offset += stateSize;
        }

        return offset;
    }

    bool BlockStateSerializer::StateMapping::DeserializeStates(const uint8_t* inputData, size_t inputSize)
    {
        if (!inputData || inputSize < sizeof(uint32_t))
            return false;

        Clear();

        size_t offset = 0;

        // Read state count
        uint32_t stateCount;
        std::memcpy(&stateCount, inputData + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Deserialize each state
        for (uint32_t i = 0; i < stateCount; ++i)
        {
            if (offset >= inputSize)
                return false;

            size_t      bytesRead = 0;
            BlockState* state     = DeserializeState(inputData + offset, inputSize - offset, bytesRead);

            if (!state || bytesRead == 0)
                return false;

            // Register the state (using its inherent stateIndex)
            uint32_t stateID = static_cast<uint32_t>(state->GetStateIndex());
            RegisterState(state, stateID);

            offset += bytesRead;
        }

        return true;
    }

    void BlockStateSerializer::StateMapping::Clear()
    {
        m_stateToID.clear();
        m_idToState.clear();
        m_serializedStates.clear();
        m_nextTempID = 0x80000000;
    }

    std::vector<BlockState*> BlockStateSerializer::StateMapping::GetAllStates() const
    {
        std::vector<BlockState*> states;
        states.reserve(m_stateToID.size());

        for (const auto& pair : m_stateToID)
        {
            states.push_back(pair.first);
        }

        return states;
    }

    // BlockStateSerializer implementation
    bool BlockStateSerializer::StatesToIDs(BlockState* const* states, size_t     stateCount,
                                           StateMapping&      mapping, uint32_t* outputIDs)
    {
        if (!states || !outputIDs || stateCount == 0)
            return false;

        for (size_t i = 0; i < stateCount; ++i)
        {
            outputIDs[i] = mapping.GetStateID(states[i]);
        }

        return true;
    }

    bool BlockStateSerializer::IDsToStates(const uint32_t* stateIDs, size_t      idCount,
                                           StateMapping&   mapping, BlockState** outputStates)
    {
        if (!stateIDs || !outputStates || idCount == 0)
            return false;

        for (size_t i = 0; i < idCount; ++i)
        {
            outputStates[i] = mapping.GetState(stateIDs[i]);
        }

        return true;
    }

    size_t BlockStateSerializer::SerializeState(const BlockState* state, uint8_t* outputData, size_t outputSize)
    {
        if (!state || !outputData || outputSize < sizeof(uint32_t) * 2)
            return 0;

        size_t offset = 0;

        // Write state ID
        uint32_t stateID = static_cast<uint32_t>(state->GetStateIndex());
        std::memcpy(outputData + offset, &stateID, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Write block name
        std::string blockName = state->GetBlock() ? state->GetBlock()->GetRegistryName() : "";
        size_t      nameSize  = WriteString(blockName, outputData + offset, outputSize - offset);
        if (nameSize == 0)
            return 0;
        offset += nameSize;

        // Write properties
        size_t propSize = SerializePropertyMap(state->GetProperties(), outputData + offset, outputSize - offset);
        if (propSize == 0 && !state->GetProperties().Empty())
            return 0;
        offset += propSize;

        return offset;
    }

    BlockState* BlockStateSerializer::DeserializeState(const uint8_t* inputData, size_t inputSize, size_t& bytesRead)
    {
        if (!inputData || inputSize < sizeof(uint32_t) * 2)
        {
            bytesRead = 0;
            return nullptr;
        }

        size_t offset = 0;

        // Read state ID
        uint32_t stateID;
        std::memcpy(&stateID, inputData + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Read block name
        std::string blockName;
        size_t      nameBytes = 0;
        if (!ReadString(inputData + offset, inputSize - offset, nameBytes, blockName))
        {
            bytesRead = 0;
            return nullptr;
        }
        offset += nameBytes;

        // Read properties
        PropertyMap properties;
        size_t      propBytes = 0;
        if (!DeserializePropertyMap(inputData + offset, inputSize - offset, propBytes, properties))
        {
            bytesRead = 0;
            return nullptr;
        }
        offset += propBytes;

        bytesRead = offset;

        // Reconstruct BlockState
        SerializedBlockState serialized;
        serialized.stateID   = stateID;
        serialized.blockName = blockName;
        // TODO: Convert PropertyMap to property pairs for reconstruction

        return ReconstructBlockState(serialized);
    }

    size_t BlockStateSerializer::CalculateMaxSerializedSize(size_t stateCount)
    {
        // Rough estimate: header + (stateID + max block name + max properties) per state
        const size_t MAX_BLOCK_NAME_LENGTH = 64;
        const size_t MAX_PROPERTIES_SIZE   = 256;
        const size_t PER_STATE_SIZE        = sizeof(uint32_t) + sizeof(uint16_t) + MAX_BLOCK_NAME_LENGTH + MAX_PROPERTIES_SIZE;

        return sizeof(uint32_t) + (stateCount * PER_STATE_SIZE);
    }

    bool BlockStateSerializer::ValidateSerializedData(const uint8_t* data, size_t dataSize)
    {
        if (!data || dataSize < sizeof(uint32_t))
            return false;

        // Check if we can at least read the state count
        uint32_t stateCount;
        std::memcpy(&stateCount, data, sizeof(uint32_t));

        // Reasonable bounds check
        return stateCount <= 10000; // Arbitrary but reasonable limit
    }

    // Private helper methods
    size_t BlockStateSerializer::SerializePropertyMap(const PropertyMap& properties, uint8_t* outputData, size_t outputSize)
    {
        if (!outputData || outputSize < sizeof(uint16_t))
            return 0;

        size_t offset = 0;

        // Write property count
        auto     propertyList = properties.GetProperties();
        uint16_t propCount    = static_cast<uint16_t>(propertyList.size());
        std::memcpy(outputData + offset, &propCount, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        // Write each property
        for (auto property : propertyList)
        {
            if (!property || offset >= outputSize)
                return 0;

            // Write property name
            std::string name     = property->GetName();
            size_t      nameSize = WriteString(name, outputData + offset, outputSize - offset);
            if (nameSize == 0)
                return 0;
            offset += nameSize;

            // Write property value as string
            std::any    propertyValue = properties.GetAny(property);
            std::string value         = property->ValueToString(propertyValue);
            size_t      valueSize     = WriteString(value, outputData + offset, outputSize - offset);
            if (valueSize == 0)
                return 0;
            offset += valueSize;
        }

        return offset;
    }

    bool BlockStateSerializer::DeserializePropertyMap(const uint8_t* inputData, size_t       inputSize,
                                                      size_t&        bytesRead, PropertyMap& properties)
    {
        if (!inputData || inputSize < sizeof(uint16_t))
        {
            bytesRead = 0;
            return false;
        }

        size_t offset = 0;

        // Read property count
        uint16_t propCount;
        std::memcpy(&propCount, inputData + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        properties.Clear();

        // Read each property
        for (uint16_t i = 0; i < propCount; ++i)
        {
            if (offset >= inputSize)
            {
                bytesRead = 0;
                return false;
            }

            // Read property name
            std::string name;
            size_t      nameBytes = 0;
            if (!ReadString(inputData + offset, inputSize - offset, nameBytes, name))
            {
                bytesRead = 0;
                return false;
            }
            offset += nameBytes;

            // Read property value
            std::string value;
            size_t      valueBytes = 0;
            if (!ReadString(inputData + offset, inputSize - offset, valueBytes, value))
            {
                bytesRead = 0;
                return false;
            }
            offset += valueBytes;

            // TODO: Reconstruct property from name and value string
            // This requires access to PropertyRegistry to look up property by name
            // and convert string value back to typed value
        }

        bytesRead = offset;
        return true;
    }

    size_t BlockStateSerializer::WriteString(const std::string& str, uint8_t* outputData, size_t outputSize)
    {
        if (!outputData || outputSize < sizeof(uint16_t))
            return 0;

        uint16_t length = static_cast<uint16_t>(std::min(str.length(), static_cast<size_t>(UINT16_MAX)));

        if (outputSize < sizeof(uint16_t) + length)
            return 0;

        // Write length
        std::memcpy(outputData, &length, sizeof(uint16_t));

        // Write string data
        if (length > 0)
        {
            std::memcpy(outputData + sizeof(uint16_t), str.data(), length);
        }

        return sizeof(uint16_t) + length;
    }

    bool BlockStateSerializer::ReadString(const uint8_t* inputData, size_t       inputSize,
                                          size_t&        bytesRead, std::string& str)
    {
        if (!inputData || inputSize < sizeof(uint16_t))
        {
            bytesRead = 0;
            return false;
        }

        // Read length
        uint16_t length;
        std::memcpy(&length, inputData, sizeof(uint16_t));

        if (inputSize < sizeof(uint16_t) + length)
        {
            bytesRead = 0;
            return false;
        }

        // Read string data
        if (length > 0)
        {
            str.assign(reinterpret_cast<const char*>(inputData + sizeof(uint16_t)), length);
        }
        else
        {
            str.clear();
        }

        bytesRead = sizeof(uint16_t) + length;
        return true;
    }

    BlockState* BlockStateSerializer::ReconstructBlockState(const SerializedBlockState& serialized)
    {
        // TODO: Implement BlockState reconstruction
        // This requires:
        // 1. Look up Block by registry name using BlockRegistry
        // 2. Reconstruct PropertyMap from property name-value pairs
        // 3. Find or create BlockState with those properties
        // 4. Return the BlockState pointer

        // For now, return nullptr as placeholder
        return nullptr;
    }
}
