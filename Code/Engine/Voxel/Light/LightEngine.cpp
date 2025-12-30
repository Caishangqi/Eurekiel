#include "LightEngine.hpp"
#include "LightEngineCommon.hpp"
#include "LightException.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Chunk/Chunk.hpp"
using namespace enigma::core;

namespace enigma::voxel
{
    LightEngine::LightEngine(World* world) : m_world(world)
    {
    }

    //-------------------------------------------------------------------------------------------
    // MarkDirty
    //
    // Add a block to the dirty queue for light recalculation.
    // Skips if block is already marked dirty to avoid duplicates.
    //
    // Reference: World::MarkLightingDirty() (World.cpp:1856-1891)
    //-------------------------------------------------------------------------------------------
    void LightEngine::MarkDirty(const BlockIterator& iter)
    {
        // Validate iterator
        if (!iter.IsValid())
        {
            return;
        }

        Chunk* chunk = iter.GetChunk();
        if (!chunk)
        {
            return;
        }

        // Get local coordinates
        int32_t x, y, z;
        iter.GetLocalCoords(x, y, z);

        // Skip if already dirty (avoid duplicates)
        if (chunk->GetIsLightDirty(x, y, z))
        {
            return;
        }

        // Add to queue and mark dirty
        m_dirtyQueue.push_back(iter);
        chunk->SetIsLightDirty(x, y, z, true);
    }

    //-------------------------------------------------------------------------------------------
    // MarkDirtyIfNotOpaque
    //
    // Mark block dirty only if it's non-opaque.
    // Opaque blocks don't propagate light, so no need to recalculate.
    //
    // Reference: World::MarkLightingDirtyIfNotOpaque() (World.cpp:1893-1915)
    //-------------------------------------------------------------------------------------------
    void LightEngine::MarkDirtyIfNotOpaque(const BlockIterator& iter)
    {
        if (!iter.IsValid())
        {
            return;
        }

        BlockState* state = iter.GetBlock();
        if (!state)
        {
            return;
        }

        // Only mark non-opaque blocks
        if (!state->GetBlock()->IsOpaque(const_cast<BlockState*>(state)))
        {
            MarkDirty(iter);
        }
    }

    //-------------------------------------------------------------------------------------------
    // ProcessDirtyQueue
    //
    // Process all dirty blocks until queue is empty.
    // Ensures lighting converges completely in a single frame.
    //
    // Reference: World::ProcessDirtyLighting() (World.cpp:1994-2004)
    //-------------------------------------------------------------------------------------------
    void LightEngine::ProcessDirtyQueue()
    {
        while (!m_dirtyQueue.empty())
        {
            ProcessNextDirtyBlock();
        }

        LogDebug(LogVoxelLight, "LightEngine:: Processed all dirty lighting (queue empty)");
    }

    //-------------------------------------------------------------------------------------------
    // ProcessNextDirtyBlock
    //
    // Process a single dirty block from the queue.
    // Calculates correct light value and propagates to neighbors if changed.
    //
    // Reference: World::ProcessNextDirtyLightBlock() (World.cpp:1917-1992)
    //-------------------------------------------------------------------------------------------
    void LightEngine::ProcessNextDirtyBlock()
    {
        try
        {
            if (m_dirtyQueue.empty())
            {
                return;
            }

            // Pop front block
            BlockIterator iter = m_dirtyQueue.front();
            m_dirtyQueue.pop_front();

            // Validate iterator
            if (!iter.IsValid())
            {
                throw InvalidBlockIteratorException("LightEngine:: Invalid BlockIterator in dirty queue");
            }

            Chunk* chunk = iter.GetChunk();
            if (!chunk)
            {
                throw ChunkNotLoadedException("LightEngine:: Chunk not loaded for dirty block");
            }

            // Get local coordinates
            int32_t x, y, z;
            iter.GetLocalCoords(x, y, z);

            // Clear dirty flag
            chunk->SetIsLightDirty(x, y, z, false);

            // Calculate correct light value
            uint8_t correctLight = ComputeCorrectLight(iter);
            uint8_t currentLight = GetCurrentLightValue(chunk, x, y, z);

            // Update if changed and propagate
            if (correctLight != currentLight)
            {
                SetLightValue(chunk, x, y, z, correctLight);
                chunk->MarkDirty();
                PropagateToNeighbors(iter);
            }
        }
        catch (const InvalidBlockIteratorException& e)
        {
            LogWarn(LogVoxelLight, "%s", e.what());
        }
        catch (const ChunkNotLoadedException& e)
        {
            LogWarn(LogVoxelLight, "%s", e.what());
        }
    }

    //-------------------------------------------------------------------------------------------
    // PropagateToNeighbors
    //
    // Mark all 6 neighbors as dirty if they are non-opaque.
    // Also marks neighbor chunks for mesh rebuild if crossing chunk boundary.
    //-------------------------------------------------------------------------------------------
    void LightEngine::PropagateToNeighbors(const BlockIterator& iter)
    {
        Chunk* currentChunk = iter.GetChunk();

        for (int dir = 0; dir < 6; ++dir)
        {
            BlockIterator neighbor = iter.GetNeighbor(static_cast<Direction>(dir));
            if (neighbor.IsValid())
            {
                BlockState* neighborState = neighbor.GetBlock();
                if (neighborState && !neighborState->GetBlock()->IsOpaque(const_cast<BlockState*>(neighborState)))
                {
                    MarkDirty(neighbor);

                    // Mark neighbor chunk dirty if crossing boundary
                    Chunk* neighborChunk = neighbor.GetChunk();
                    if (neighborChunk && neighborChunk != currentChunk)
                    {
                        neighborChunk->MarkDirty();
                    }
                }
            }
        }
    }

    //-------------------------------------------------------------------------------------------
    // UndirtyAllBlocksInChunk
    //
    // Remove all blocks belonging to a chunk from the dirty queue.
    // Called when a chunk is unloaded to prevent stale references.
    //
    // Reference: World::UndirtyAllBlocksInChunk() (World.cpp:2006-2035)
    //-------------------------------------------------------------------------------------------
    void LightEngine::UndirtyAllBlocksInChunk(Chunk* chunk)
    {
        if (!chunk)
        {
            return;
        }

        auto it = m_dirtyQueue.begin();
        while (it != m_dirtyQueue.end())
        {
            if (it->GetChunk() == chunk)
            {
                // Clear dirty flag
                int32_t x, y, z;
                it->GetLocalCoords(x, y, z);
                chunk->SetIsLightDirty(x, y, z, false);

                // Remove from queue
                it = m_dirtyQueue.erase(it);
            }
            else
            {
                ++it;
            }
        }

        LogDebug(LogVoxelLight, "LightEngine:: Cleaned dirty queue for chunk at (%d, %d)",
                 chunk->GetChunkCoords().x, chunk->GetChunkCoords().y);
    }
} // namespace enigma::voxel
