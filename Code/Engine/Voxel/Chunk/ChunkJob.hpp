#pragma once
#include "Chunk.hpp"
#include "ChunkState.hpp"
#include "Engine/Core/Schedule/RunnableTask.hpp"
#include "Engine/Math/IntVec2.hpp"
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
        {
        }

        virtual ~ChunkJob() = default;

        // Chunk identification
        IntVec2 GetChunkCoords() const { return m_chunkCoords; }

        // Legacy wrapper maintained until all chunk jobs switch to generic task APIs.
        void RequestCancel() { RequestCancellation(); }
        bool IsCancelled() const { return IsCancellationRequested(); }

    protected:
        IntVec2 m_chunkCoords; // Chunk coordinates this job operates on
    };

    //-----------------------------------------------------------------------------------------------
    // Forward declarations for job types
    //-----------------------------------------------------------------------------------------------
    class GenerateChunkJob;
    class LoadChunkJob;
    class SaveChunkJob;
} // namespace enigma::voxel
