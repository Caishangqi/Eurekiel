#pragma once
#include "ChunkJob.hpp"
#include "Engine/Voxel/World/ESFWorldStorage.hpp"
#include "Engine/Voxel/World/ESFSWorldStorage.hpp"

namespace enigma::voxel
{
    //-----------------------------------------------------------------------------------------------
    // Job for loading chunk data from disk on FileIO threads
    // IO-intensive work: file reading, decompression, deserialization
    // Supports both ESF and ESFS storage formats
    //-----------------------------------------------------------------------------------------------
    class LoadChunkJob : public ChunkJob
    {
    public:
        // Constructor for ESF format
        LoadChunkJob(IntVec2 chunkCoords, Chunk* chunk, ESFChunkStorage* storage)
            : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
              , m_chunk(chunk)
              , m_esfStorage(storage)
              , m_esfsStorage(nullptr)
              , m_loadSuccess(false)
        {
        }

        // Constructor for ESFS format
        LoadChunkJob(IntVec2 chunkCoords, Chunk* chunk, ESFSChunkStorage* storage)
            : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
              , m_chunk(chunk)
              , m_esfStorage(nullptr)
              , m_esfsStorage(storage)
              , m_loadSuccess(false)
        {
        }

        void Execute() override;

        // Result query (called by main thread after completion)
        bool WasSuccessful() const { return m_loadSuccess; }

    private:
        Chunk*            m_chunk; // Target chunk (worker populates block data)
        ESFChunkStorage*  m_esfStorage; // ESF storage (or nullptr if using ESFS)
        ESFSChunkStorage* m_esfsStorage; // ESFS storage (or nullptr if using ESF)
        bool              m_loadSuccess; // Did the load operation succeed?
    };
} // namespace enigma::voxel
