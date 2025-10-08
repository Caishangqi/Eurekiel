#pragma once
#include "Chunk.hpp"
#include "ChunkState.hpp"
#include "Engine/Core/Schedule/RunnableTask.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <atomic>
#include <memory>

namespace enigma::voxel
{
    using namespace enigma::core;

    //-----------------------------------------------------------------------------------------------
    // Base class for all chunk-related async jobs
    // Provides common functionality for chunk coordinate tracking and cancellation
    //-----------------------------------------------------------------------------------------------
    class ChunkJob : public RunnableTask
    {
    public:
        explicit ChunkJob(const std::string& taskType, IntVec2 chunkCoords)
            : RunnableTask(taskType)
              , m_chunkCoords(chunkCoords)
              , m_isCancelled(false)
        {
        }

        virtual ~ChunkJob() = default;

        // Chunk identification
        IntVec2 GetChunkCoords() const { return m_chunkCoords; }

        // Cancellation support (non-blocking, cooperative)
        void RequestCancel() { m_isCancelled.store(true, std::memory_order_release); }
        bool IsCancelled() const { return m_isCancelled.load(std::memory_order_acquire); }

    protected:
        IntVec2           m_chunkCoords; // Chunk coordinates this job operates on
        std::atomic<bool> m_isCancelled; // Cancellation flag (set by main thread, read by worker)
    };

    //-----------------------------------------------------------------------------------------------
    // Forward declarations for job types
    //-----------------------------------------------------------------------------------------------
    class GenerateChunkJob;
    class LoadChunkJob;
    class SaveChunkJob;
} // namespace enigma::voxel
