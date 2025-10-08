#include "BlockStateSerializer.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Voxel/Property/PropertyRegistry.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <cstring>
#include <algorithm>

namespace enigma::voxel
{
    using namespace enigma::registry::block;

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
        {
            core::LogError("state_mapping", "RegisterState failed: state=%p, stateID=%u", state, stateID);
            return;
        }

        m_stateToID[state]   = stateID;
        m_idToState[stateID] = state;

        core::LogError("state_mapping", "Registered state: stateID=%u -> state=%p, total states now: %zu",
                       stateID, state, m_stateToID.size());
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
                // Store full name format: "namespace:name" for proper registry lookup
                serialized.blockName = state->GetBlock()->GetNamespace() + ":" + state->GetBlock()->GetRegistryName();
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

            // Write serialized state to buffer - use correct mapping stateID instead of GetStateIndex()
            // First, manually write the correct stateID
            core::LogError("serialization", "SerializeStates: writing correct stateID=%u for state=%p", stateID, state);

            // Write state ID (using mapping ID, not state->GetStateIndex())
            std::memcpy(outputData + offset, &stateID, sizeof(uint32_t));
            size_t tempOffset = offset + sizeof(uint32_t);

            // Write block name
            std::string blockName  = serialized.blockName;
            uint16_t    nameLength = static_cast<uint16_t>(blockName.length());
            if (tempOffset + sizeof(uint16_t) + nameLength >= outputSize)
                return 0; // Buffer overflow

            std::memcpy(outputData + tempOffset, &nameLength, sizeof(uint16_t));
            tempOffset += sizeof(uint16_t);
            std::memcpy(outputData + tempOffset, blockName.c_str(), nameLength);
            tempOffset += nameLength;

            // Write properties
            uint16_t propCount = static_cast<uint16_t>(serialized.properties.size());
            if (tempOffset + sizeof(uint16_t) >= outputSize)
                return 0; // Buffer overflow

            std::memcpy(outputData + tempOffset, &propCount, sizeof(uint16_t));
            tempOffset += sizeof(uint16_t);

            // Write each property
            for (const auto& prop : serialized.properties)
            {
                // Write property name
                uint16_t propNameLength = static_cast<uint16_t>(prop.first.length());
                if (tempOffset + sizeof(uint16_t) + propNameLength >= outputSize)
                    return 0;

                std::memcpy(outputData + tempOffset, &propNameLength, sizeof(uint16_t));
                tempOffset += sizeof(uint16_t);
                std::memcpy(outputData + tempOffset, prop.first.c_str(), propNameLength);
                tempOffset += propNameLength;

                // Write property value
                uint16_t propValueLength = static_cast<uint16_t>(prop.second.length());
                if (tempOffset + sizeof(uint16_t) + propValueLength >= outputSize)
                    return 0;

                std::memcpy(outputData + tempOffset, &propValueLength, sizeof(uint16_t));
                tempOffset += sizeof(uint16_t);
                std::memcpy(outputData + tempOffset, prop.second.c_str(), propValueLength);
                tempOffset += propValueLength;
            }

            offset = tempOffset;
        }

        return offset;
    }

    bool BlockStateSerializer::StateMapping::DeserializeStates(const uint8_t* inputData, size_t inputSize)
    {
        if (!inputData || inputSize < sizeof(uint32_t))
        {
            core::LogError("state_mapping", "Invalid input data: inputData=%p, inputSize=%zu", inputData, inputSize);
            return false;
        }

        Clear();

        size_t offset = 0;

        // Read state count
        uint32_t stateCount;
        std::memcpy(&stateCount, inputData + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        core::LogError("state_mapping", "Starting deserialization: %u states from %zu bytes, m_stateToID.size()=%zu",
                       stateCount, inputSize, m_stateToID.size());

        // Deserialize each state
        for (uint32_t i = 0; i < stateCount; ++i)
        {
            if (offset >= inputSize)
            {
                core::LogError("state_mapping", "Offset %zu >= inputSize %zu at state %u", offset, inputSize, i);
                return false;
            }

            // Read state ID first to get the serialized ID
            uint32_t serializedStateID;
            std::memcpy(&serializedStateID, inputData + offset, sizeof(uint32_t));

            core::LogError("state_mapping", "Deserializing state %u: stateID=%u, offset=%zu", i, serializedStateID, offset);

            size_t      bytesRead = 0;
            BlockState* state     = DeserializeState(inputData + offset, inputSize - offset, bytesRead);

            if (!state || bytesRead == 0)
            {
                core::LogError("state_mapping", "Failed to deserialize state %u: state=%p, bytesRead=%zu", i, state, bytesRead);
                return false;
            }

            core::LogDebug("state_mapping", "Successfully deserialized state %u: state=%p, bytesRead=%zu", i, state, bytesRead);

            // Register the state using the serialized stateID (not the block's stateIndex)
            RegisterState(state, serializedStateID);

            offset += bytesRead;
        }

        core::LogError("state_mapping", "Finished deserialization loop: m_stateToID.size()=%zu, m_idToState.size()=%zu",
                       m_stateToID.size(), m_idToState.size());

        core::LogInfo("state_mapping", "Successfully deserialized %u states", stateCount);
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
        core::LogError("serialization", "SerializeState: writing stateID=%u for state=%p", stateID, state);
        std::memcpy(outputData + offset, &stateID, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Write block name - use full format "namespace:name" for proper registry lookup
        std::string blockName = state->GetBlock() ? (state->GetBlock()->GetNamespace() + ":" + state->GetBlock()->GetRegistryName()) : "";
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
        core::LogDebug("state_deserialize", "DeserializeState called: inputSize=%zu", inputSize);

        if (!inputData || inputSize < sizeof(uint32_t) * 2)
        {
            core::LogError("state_deserialize", "Invalid input: inputData=%p, inputSize=%zu", inputData, inputSize);
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

        // Convert PropertyMap to property pairs for reconstruction
        auto propertyList = properties.GetProperties();
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
        using namespace enigma::registry::block;
        using namespace enigma::voxel;

        core::LogDebug("block_serialization", "Reconstructing BlockState for block '%s' with stateID=%u",
                       serialized.blockName.c_str(), serialized.stateID);

        // 1. Look up Block by registry name using BlockRegistry
        // Parse namespace:name format
        std::shared_ptr<Block> block;
        size_t                 colonPos = serialized.blockName.find(':');
        if (colonPos != std::string::npos)
        {
            // Split namespace:name format
            std::string namespaceName = serialized.blockName.substr(0, colonPos);
            std::string blockName     = serialized.blockName.substr(colonPos + 1);
            block                     = BlockRegistry::GetBlock(namespaceName, blockName);
            core::LogDebug("block_serialization", "Looking up block with namespace='%s', name='%s'",
                           namespaceName.c_str(), blockName.c_str());
        }
        else
        {
            // Fallback to simple name lookup (no namespace)
            block = BlockRegistry::GetBlock(serialized.blockName);
            core::LogDebug("block_serialization", "Looking up block with simple name='%s'",
                           serialized.blockName.c_str());
        }

        if (!block)
        {
            core::LogError("block_serialization", "Failed to find block '%s' in registry", serialized.blockName.c_str());

            // Debug: List all registered blocks (using ERROR level to ensure visibility)
            auto allBlocks = BlockRegistry::GetAllBlocks();
            core::LogError("block_serialization", "Registry currently has %zu blocks:", allBlocks.size());
            for (size_t i = 0; i < std::min(allBlocks.size(), size_t(10)); ++i)
            {
                if (allBlocks[i])
                {
                    std::string fullName = allBlocks[i]->GetNamespace() + ":" + allBlocks[i]->GetRegistryName();
                    core::LogError("block_serialization", "  Block %zu: '%s'", i, fullName.c_str());
                }
            }

            return nullptr;
        }

        core::LogDebug("block_serialization", "Found block '%s' in registry", serialized.blockName.c_str());

        // 2. Reconstruct PropertyMap from property name-value pairs
        PropertyMap properties;
        for (const auto& propPair : serialized.properties)
        {
            const std::string& propName  = propPair.first;
            const std::string& propValue = propPair.second;

            // Find the property in the block's property list
            bool foundProperty = false;
            for (auto property : block->GetProperties())
            {
                if (property && property->GetName() == propName)
                {
                    // Convert string value back to typed value using property's parser
                    std::any typedValue = property->StringToValue(propValue);

                    // We need to set the property using the correct typed template method
                    // For now, skip property reconstruction as it requires template specialization
                    foundProperty = true;
                    break;
                }
            }

            if (!foundProperty)
            {
                core::LogWarn("block_serialization", "Property '%s' not found in block '%s', ignoring",
                              propName.c_str(), serialized.blockName.c_str());
            }
        }

        // 3. Find or create BlockState with those properties
        // For now, use default state since property reconstruction is complex
        BlockState* state = block->GetDefaultState();
        if (!state)
        {
            core::LogError("block_serialization", "Failed to get default state for block '%s'", serialized.blockName.c_str());
        }
        else
        {
            core::LogInfo("block_serialization", "Using default state for block '%s' (property reconstruction needs improvement)", serialized.blockName.c_str());
        }

        return state;
    }
}
