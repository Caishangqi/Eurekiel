#pragma once
#include "ChunkJob.hpp"
#include "Engine/Voxel/World/ESFWorldStorage.hpp"
#include "Engine/Voxel/World/ESFSWorldStorage.hpp"

namespace enigma::voxel
{
    // Forward declaration
    class World;

    //-----------------------------------------------------------------------------------------------
    // Job for loading chunk data from disk on FileIO threads
    // IO-intensive work: file reading, decompression, deserialization
    // Supports both ESF and ESFS storage formats
    // [REFACTORED] No longer holds Chunk*, retrieves it via World::GetChunk() in Execute()
    //-----------------------------------------------------------------------------------------------
    class LoadChunkJob : public ChunkJob
    {
    public:
        // Constructor for ESF format
        LoadChunkJob(IntVec2 chunkCoords, World* world, ESFChunkStorage* storage)
            : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
              , m_world(world)
              , m_esfStorage(storage)
              , m_esfsStorage(nullptr)
              , m_loadSuccess(false)
        {
        }

        // Constructor for ESFS format
        LoadChunkJob(IntVec2 chunkCoords, World* world, ESFSChunkStorage* storage)
            : ChunkJob(TaskTypeConstants::FILE_IO, chunkCoords)
              , m_world(world)
              , m_esfStorage(nullptr)
              , m_esfsStorage(storage)
              , m_loadSuccess(false)
        {
        }

        void Execute() override;

        // Result query (called by main thread after completion)
        bool WasSuccessful() const { return m_loadSuccess; }

    private:
        World*            m_world; // [NEW] World instance to get Chunk via coordinates
        ESFChunkStorage*  m_esfStorage; // ESF storage (or nullptr if using ESFS)
        ESFSChunkStorage* m_esfsStorage; // ESFS storage (or nullptr if using ESF)
        bool              m_loadSuccess; // Did the load operation succeed?
    };
} // namespace enigma::voxel
