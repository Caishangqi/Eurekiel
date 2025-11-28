#pragma once
#include "../Block/BlockState.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace enigma::voxel
{
    /**
     * @brief Serializer for BlockState objects to/from binary format
     *
     * Converts BlockState pointers to stable IDs for storage and reconstruction.
     * Designed for chunk serialization where BlockState objects need to be
     * saved and restored across sessions.
     *
     * Serialization Strategy:
     * 1. Use BlockState stateIndex as primary ID (stable across sessions)
     * 2. Store Block registry name as fallback identifier
     * 3. Serialize PropertyMap as key-value pairs with property names
     * 4. Support reconstruction from registry during deserialization
     */
    class BlockStateSerializer
    {
    public:
        /**
         * @brief Serialized block state entry (variable size)
         *
         * Binary format:
         * [StateID: uint32_t] [BlockNameLen: uint16_t] [BlockName: string]
         * [PropertyCount: uint16_t] [Properties: PropertyEntry[]]
         */
        struct SerializedBlockState
        {
            uint32_t                                         stateID = 0; // BlockState index for fast lookup
            std::string                                      blockName; // Block registry name (fallback)
            std::vector<std::pair<std::string, std::string>> properties; // Property name-value pairs
        };

        /**
         * @brief Block state mapping for chunk serialization
         *
         * Maps BlockState pointers to stable IDs and vice versa.
         * Maintains consistency within a chunk's serialization context.
         */
        class StateMapping
        {
        private:
            std::unordered_map<BlockState*, uint32_t> m_stateToID;
            std::unordered_map<uint32_t, BlockState*> m_idToState;
            std::vector<SerializedBlockState>         m_serializedStates;
            uint32_t                                  m_nextTempID = 0x80000000; // Temporary IDs start at high range

        public:
            /**
             * @brief Get stable ID for a BlockState
             * @param state Pointer to BlockState
             * @return Stable ID for serialization
             */
            uint32_t GetStateID(BlockState* state);

            /**
             * @brief Get BlockState from stable ID
             * @param stateID Stable ID from serialization
             * @return Pointer to BlockState, or nullptr if not found
             */
            BlockState* GetState(uint32_t stateID);

            /**
             * @brief Register a BlockState with explicit ID
             * @param state Pointer to BlockState
             * @param stateID Explicit ID to use
             */
            void RegisterState(BlockState* state, uint32_t stateID);

            /**
             * @brief Serialize all registered states to binary format
             * @param outputData Output buffer
             * @param outputSize Maximum output size
             * @return Actual bytes written, or 0 if failed
             */
            size_t SerializeStates(uint8_t* outputData, size_t outputSize);

            /**
             * @brief Deserialize states from binary format
             * @param inputData Input binary data
             * @param inputSize Size of input data
             * @return True if successful
             */
            bool DeserializeStates(const uint8_t* inputData, size_t inputSize);

            /**
             * @brief Get number of registered states
             */
            size_t GetStateCount() const { return m_stateToID.size(); }

            /**
             * @brief Clear all mappings
             */
            void Clear();

            /**
             * @brief Get all registered states
             */
            std::vector<BlockState*> GetAllStates() const;
        };

    public:
        /**
         * @brief Convert BlockState array to ID array
         * @param states Array of BlockState pointers
         * @param stateCount Number of states
         * @param mapping State mapping context
         * @param outputIDs Output array for state IDs
         * @return True if successful
         */
        static bool StatesToIDs(BlockState* const* states, size_t     stateCount,
                                StateMapping&      mapping, uint32_t* outputIDs);

        /**
         * @brief Convert ID array to BlockState array
         * @param stateIDs Array of state IDs
         * @param idCount Number of IDs
         * @param mapping State mapping context
         * @param outputStates Output array for BlockState pointers
         * @return True if successful
         */
        static bool IDsToStates(const uint32_t* stateIDs, size_t      idCount,
                                StateMapping&   mapping, BlockState** outputStates);

        /**
         * @brief Serialize a single BlockState to binary
         * @param state BlockState to serialize
         * @param outputData Output buffer
         * @param outputSize Maximum output size
         * @return Bytes written, or 0 if failed
         */
        static size_t SerializeState(const BlockState* state, uint8_t* outputData, size_t outputSize);

        /**
         * @brief Deserialize a single BlockState from binary
         * @param inputData Input binary data
         * @param inputSize Size of input data
         * @param bytesRead Output: bytes consumed from input
         * @return Deserialized BlockState, or nullptr if failed
         */
        static BlockState* DeserializeState(const uint8_t* inputData, size_t inputSize, size_t& bytesRead);

        /**
         * @brief Calculate maximum serialized size for state array
         * @param stateCount Number of unique states
         * @return Maximum bytes needed
         */
        static size_t CalculateMaxSerializedSize(size_t stateCount);

        /**
         * @brief Validate serialized state data
         * @param data Serialized data
         * @param dataSize Size of data
         * @return True if data appears valid
         */
        static bool ValidateSerializedData(const uint8_t* data, size_t dataSize);

    public:
        // Helper methods for property serialization
        static size_t SerializePropertyMap(const PropertyMap& properties, uint8_t* outputData, size_t outputSize);
        static bool   DeserializePropertyMap(const uint8_t* inputData, size_t       inputSize,
                                           size_t&          bytesRead, PropertyMap& properties);

        // String serialization helpers
        static size_t WriteString(const std::string& str, uint8_t* outputData, size_t outputSize);
        static bool   ReadString(const uint8_t* inputData, size_t       inputSize,
                               size_t&          bytesRead, std::string& str);

        // BlockState reconstruction from serialized data
        static BlockState* ReconstructBlockState(const SerializedBlockState& serialized);
    };

    /**
     * @brief Block state serialization statistics
     */
    struct BlockStateSerializationStats
    {
        size_t uniqueStates     = 0; // Number of unique block states
        size_t totalBlocks      = 0; // Total blocks in chunk
        size_t serializedSize   = 0; // Size of serialized state data
        size_t mappingSize      = 0; // Size of state mapping data
        float  compressionRatio = 1.0f; // Overall compression ratio

        float GetCompressionPercent() const
        {
            return (1.0f - compressionRatio) * 100.0f;
        }
    };
}
