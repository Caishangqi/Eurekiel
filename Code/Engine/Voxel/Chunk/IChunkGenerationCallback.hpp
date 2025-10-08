#pragma once

namespace enigma::voxel
{
    class Chunk;

    /**
     * @brief 区块生成回调接口
     *
     * 用于World和ChunkManager之间的通信，避免循环依赖。
     * ChunkManager通过此接口请求World执行区块生成和相关操作。
     */
    class IChunkGenerationCallback
    {
    public:
        virtual ~IChunkGenerationCallback() = default;

        /**
         * @brief 请求生成区块内容
         * @param chunk 需要生成的区块指针
         * @param chunkX 区块X坐标
         * @param chunkY 区块Y坐标
         */
        virtual void GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY) = 0;

        /**
         * @brief 询问是否应该卸载指定区块
         * @param chunkX 区块X坐标
         * @param chunkY 区块Y坐标
         * @return true 如果应该卸载该区块
         */
        virtual bool ShouldUnloadChunk(int32_t chunkX, int32_t chunkY) = 0;
    };
}
