#pragma once
#include "ChunkJob.hpp"
#include "Engine/Voxel/World/ESFWorldStorage.hpp"
#include "Engine/Voxel/World/ESFSWorldStorage.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include <vector>

namespace enigma::voxel
{
    //-----------------------------------------------------------------------------------------------
    // Job for saving chunk data to disk on FileIO threads
    // IO-intensive work: serialization, compression, file writing
    // Supports both ESF and ESFS storage formats
    //
    // THREAD SAFETY STRATEGY: Deep copy approach (Decision Point 1: 方案 A)
    // - Constructor takes a snapshot of all block data
    // - Worker thread operates on the snapshot only
    // - Main thread can safely modify original chunk after job creation
    //-----------------------------------------------------------------------------------------------
    class SaveChunkJob : public ChunkJob
    {
    public:
        // Constructor for ESF format
        SaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk, ESFChunkStorage* storage);

        // Constructor for ESFS format
        SaveChunkJob(IntVec2 chunkCoords, const Chunk* chunk, ESFSChunkStorage* storage);

        void Execute() override;

    private:
        ESFChunkStorage*         m_esfStorage; // ESF storage (or nullptr if using ESFS)
        ESFSChunkStorage*        m_esfsStorage; // ESFS storage (or nullptr if using ESF)
        std::vector<BlockState*> m_blockData; // Deep copy of block data (snapshot)

        // Note: We store BlockState pointers which point to registered block states
        // These are safe to copy because BlockState instances are immutable singletons
        // managed by BlockRegistry (never deleted during gameplay)
    };
} // namespace enigma::voxel
