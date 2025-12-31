#pragma once
//-----------------------------------------------------------------------------------------------
// VoxelLightEngine.hpp
//
// [NEW] Composite light engine manager
// Composes BlockLightEngine and SkyLightEngine, provides unified interface
//
// Reference: Minecraft LevelLightEngine.java
//
//-----------------------------------------------------------------------------------------------

#include "BlockLightEngine.hpp"
#include "SkyLightEngine.hpp"
#include <memory>

namespace enigma::voxel
{
    class Chunk;

    //-------------------------------------------------------------------------------------------
    // VoxelLightEngine
    //
    // Composite manager for block and sky light engines.
    // Provides unified interface for light queries and updates.
    //-------------------------------------------------------------------------------------------
    class VoxelLightEngine
    {
    public:
        explicit VoxelLightEngine(World* world);
        ~VoxelLightEngine() = default;

        //-----------------------------------------------------------------------------------
        // Light Query - Delegate to sub-engines
        //-----------------------------------------------------------------------------------
        uint8_t GetSkyLight(const BlockPos& pos) const;
        uint8_t GetBlockLight(const BlockPos& pos) const;

        //-----------------------------------------------------------------------------------
        // Combined Brightness
        // Reference: Minecraft LevelLightEngine.getRawBrightness(blockPos, skyDarken)
        //-----------------------------------------------------------------------------------
        int GetRawBrightness(const BlockPos& pos, int skyDarken) const;

        //-----------------------------------------------------------------------------------
        // Unified Update Interface
        //-----------------------------------------------------------------------------------
        void CheckBlock(const BlockPos& pos);
        int  RunLightUpdates(); // Returns number of blocks processed
        bool HasLightWork() const;

        //-----------------------------------------------------------------------------------
        // Sub-engine Access (for advanced use)
        //-----------------------------------------------------------------------------------
        BlockLightEngine&       GetBlockEngine() { return *m_blockEngine; }
        SkyLightEngine&         GetSkyEngine() { return *m_skyEngine; }
        const BlockLightEngine& GetBlockEngine() const { return *m_blockEngine; }
        const SkyLightEngine&   GetSkyEngine() const { return *m_skyEngine; }

        //-----------------------------------------------------------------------------------
        // Chunk Cleanup
        //-----------------------------------------------------------------------------------
        void UndirtyAllBlocksInChunk(Chunk* chunk);

    private:
        World*                            m_world = nullptr; // [NEW] World reference for BlockIterator creation
        std::unique_ptr<BlockLightEngine> m_blockEngine;
        std::unique_ptr<SkyLightEngine>   m_skyEngine;
    };
} // namespace enigma::voxel
