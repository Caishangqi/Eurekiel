#include "GenerateChunkJob.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::voxel
{
    void GenerateChunkJob::Execute()
    {
        // Safety check: ensure chunk is still in Generating state
        ChunkState currentState = m_chunk->GetState();
        if (currentState != ChunkState::Generating)
        {
            // Chunk was cancelled or state changed, abort generation
            return;
        }

        // Check cancellation before expensive work
        if (IsCancelled())
        {
            return;
        }

        // Perform terrain generation
        GUARANTEE_OR_DIE(m_generator != nullptr, "GenerateChunkJob: Generator is null")
        GUARANTEE_OR_DIE(m_chunk != nullptr, "GenerateChunkJob: Chunk is null")

        m_generator->GenerateChunk(m_chunk, m_chunkCoords.x, m_chunkCoords.y, m_worldSeed);

        // Check cancellation after generation (before final state transition)
        if (IsCancelled())
        {
            // Generation completed but chunk is being unloaded
            // Main thread will handle cleanup based on state
            return;
        }

        // Mark generation as complete (only if not cancelled)
        // Main thread will transition to Active state when it processes completed jobs
    }
} // namespace enigma::voxel
