#pragma once
//-----------------------------------------------------------------------------------------------
// LightEngine.hpp
//
// [NEW] Abstract base class for light engines (BlockLightEngine, SkyLightEngine)
// Provides BFS dirty queue management for light propagation
//
// Reference: Minecraft LightEngine.java
//
//-----------------------------------------------------------------------------------------------

#include <deque>
#include <cstdint>

#include "Engine/Voxel/Block/BlockIterator.hpp"

namespace enigma::voxel
{
    struct BlockPos;
    class Chunk;
    class World;

    //-------------------------------------------------------------------------------------------
    // LightEngine
    //
    // Abstract base class providing BFS queue management for light propagation.
    // Subclasses (BlockLightEngine, SkyLightEngine) implement GetLightValue and ComputeCorrectLight.
    //-------------------------------------------------------------------------------------------
    class LightEngine
    {
    public:
        virtual  ~LightEngine() = default;
        explicit LightEngine(World* world);

        //-----------------------------------------------------------------------------------
        // Pure Virtual Methods - Subclass implements
        //-----------------------------------------------------------------------------------
        virtual uint8_t GetLightValue(const BlockPos& pos) const = 0;
        virtual uint8_t ComputeCorrectLight(const BlockIterator& iter) const = 0;

        //-----------------------------------------------------------------------------------
        // BFS Queue Management - Shared implementation
        //-----------------------------------------------------------------------------------
        void   MarkDirty(const BlockIterator& iter);
        void   MarkDirtyIfNotOpaque(const BlockIterator& iter);
        void   ProcessDirtyQueue();
        void   ProcessNextDirtyBlock();
        bool   HasWork() const { return !m_dirtyQueue.empty(); }
        size_t GetQueueSize() const { return m_dirtyQueue.size(); }

        //-----------------------------------------------------------------------------------
        // Chunk Cleanup
        //-----------------------------------------------------------------------------------
        void UndirtyAllBlocksInChunk(Chunk* chunk);

    protected:
        //-----------------------------------------------------------------------------------
        // BFS Propagation Helper
        //-----------------------------------------------------------------------------------
        void PropagateToNeighbors(const BlockIterator& iter);

        //-----------------------------------------------------------------------------------
        // Light Value Update - Subclass implements
        //-----------------------------------------------------------------------------------
        virtual void    SetLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z, uint8_t value) = 0;
        virtual uint8_t GetCurrentLightValue(Chunk* chunk, int32_t x, int32_t y, int32_t z) const = 0;

        std::deque<BlockIterator> m_dirtyQueue;
        World*                    m_world = nullptr; // [NEW] World reference for BlockPos lookup
    };
} // namespace enigma::voxel
