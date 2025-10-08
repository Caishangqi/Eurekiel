#include "SaveChunkJob.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::voxel
{
    // Constructor for ESF format
    SaveChunkJob::SaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk, ESFChunkStorage* storage)
        : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
          , m_esfStorage(storage)
          , m_esfsStorage(nullptr)
          , m_blockData()
    {
        GUARANTEE_OR_DIE(chunk != nullptr, "SaveChunkJob: Chunk is null");
        GUARANTEE_OR_DIE(storage != nullptr, "SaveChunkJob: Storage is null");

        // DEEP COPY: Take a snapshot of all block data (Decision Point 1: 方案 A)
        // This allows main thread to continue modifying chunk while save is in progress
        m_blockData.reserve(Chunk::BLOCKS_PER_CHUNK);

        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    // const_cast is safe here because we're calling const getter
                    BlockState* blockState = const_cast<Chunk*>(chunk)->GetBlock(x, y, z);
                    m_blockData.push_back(blockState);
                }
            }
        }
    }

    // Constructor for ESFS format
    SaveChunkJob::SaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk, ESFSChunkStorage* storage)
        : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
          , m_esfStorage(nullptr)
          , m_esfsStorage(storage)
          , m_blockData()
    {
        GUARANTEE_OR_DIE(chunk != nullptr, "SaveChunkJob: Chunk is null");
        GUARANTEE_OR_DIE(storage != nullptr, "SaveChunkJob: Storage is null");

        // DEEP COPY: Take a snapshot of all block data (Decision Point 1: 方案 A)
        // This allows main thread to continue modifying chunk while save is in progress
        m_blockData.reserve(Chunk::BLOCKS_PER_CHUNK);

        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                {
                    // const_cast is safe here because we're calling const getter
                    BlockState* blockState = const_cast<Chunk*>(chunk)->GetBlock(x, y, z);
                    m_blockData.push_back(blockState);
                }
            }
        }
    }

    void SaveChunkJob::Execute()
    {
        // Check cancellation before expensive IO
        if (IsCancelled())
        {
            return;
        }

        // Perform chunk saving to disk using the snapshot data (supports both ESF and ESFS)
        GUARANTEE_OR_DIE(m_blockData.size() == Chunk::BLOCKS_PER_CHUNK,
                         "SaveChunkJob: Invalid block data size");

        if (m_esfStorage != nullptr)
        {
            // ESF format
            GUARANTEE_OR_DIE(m_esfsStorage == nullptr, "SaveChunkJob: Both storages are set");
            m_esfStorage->SaveChunkFromSnapshot(m_chunkCoords.x, m_chunkCoords.y, m_blockData);
        }
        else if (m_esfsStorage != nullptr)
        {
            // ESFS format
            GUARANTEE_OR_DIE(m_esfStorage == nullptr, "SaveChunkJob: Both storages are set");
            m_esfsStorage->SaveChunkFromSnapshot(m_chunkCoords.x, m_chunkCoords.y, m_blockData);
        }
        else
        {
            GUARANTEE_OR_DIE(false, "SaveChunkJob: No storage configured");
        }

        // Check cancellation after save (cleanup already happened, but respect flag)
        if (IsCancelled())
        {
            // Save completed but chunk is being unloaded
            // Main thread will handle final state transition
            return;
        }

        // Main thread will transition chunk state based on completion queue
    }
} // namespace enigma::voxel
