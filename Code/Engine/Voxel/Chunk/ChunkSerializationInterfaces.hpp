#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include <vector>
#include <cstdint>

namespace enigma::voxel
{
    class Chunk;

    /**
     * @brief 区块扩展数据持有者（预留接口）
     *
     * 类似NeoForge的Attachment系统，用于存储模组和扩展数据。
     * 当前为空实现，为未来的序列化系统预留接口。
     */
    class ChunkAttachmentHolder
    {
    public:
        ChunkAttachmentHolder()  = default;
        ~ChunkAttachmentHolder() = default;

        // 预留：扩展数据操作接口
        // 当前返回false/空值，未来将实现实际功能

        template <typename T>
        bool SetData(const std::string& key, const T& data) { return false; }

        template <typename T>
        T GetData(const std::string& key, const T& defaultValue = T{}) const { return defaultValue; }

        template <typename T>
        bool HasData(const std::string& key) const { return false; }

        template <typename T>
        bool RemoveData(const std::string& key) { return false; }

        // 预留：序列化接口
        bool HasAttachments() const { return false; }

        void ClearAllAttachments()
        {
            /* 空实现 */
        }
    };

    /**
     * @brief 区块序列化器接口（预留）
     *
     * 未来用于区块数据的序列化和反序列化。
     * 当前为空接口定义，不影响现有功能。
     */
    class IChunkSerializer
    {
    public:
        virtual ~IChunkSerializer() = default;

        /**
         * @brief 序列化区块到二进制数据
         * @param chunk 要序列化的区块
         * @param outData 输出的二进制数据
         * @return 序列化是否成功
         */
        virtual bool SerializeChunk(const Chunk* chunk, std::vector<uint8_t>& outData)
        {
            UNUSED(chunk)
            UNUSED(outData)
            return false;
        }

        /**
         * @brief 从二进制数据反序列化区块
         * @param chunk 要填充数据的区块
         * @param data 输入的二进制数据
         * @return 反序列化是否成功
         */
        virtual bool DeserializeChunk(Chunk* chunk, const std::vector<uint8_t>& data)
        {
            UNUSED(chunk)
            UNUSED(data)
            return false;
        }

        virtual std::string GetSupportedVersion() const { return "SimpleMiner-1.0"; }
    };

    /**
     * @brief 区块存储接口（预留）
     *
     * 未来用于区块数据的持久化存储。
     * 当前为空接口定义，不影响现有功能。
     */
    class IChunkStorage
    {
    public:
        virtual ~IChunkStorage() = default;

        // 预留：存储接口
        virtual bool SaveChunk(int32_t chunkX, int32_t chunkY, const void* data)
        {
            UNUSED(chunkX)
            UNUSED(chunkY)
            UNUSED(data)
            return false;
        }

        virtual bool LoadChunk(int32_t chunkX, int32_t chunkY, void* data)
        {
            UNUSED(chunkX)
            UNUSED(chunkY)
            UNUSED(data)
            return false;
        }

        virtual bool DeleteChunk(int32_t chunkX, int32_t chunkY)
        {
            UNUSED(chunkX)
            UNUSED(chunkY)
            return false;
        }

        virtual void Flush()
        {
            // Default implementation: do nothing
        }

        virtual void Close()
        {
            // Default implementation: do nothing
        }

        virtual bool ChunkExists(int32_t chunkX, int32_t chunkY) const
        {
            UNUSED(chunkX)
            UNUSED(chunkY)
            return false;
        }
    };
}
