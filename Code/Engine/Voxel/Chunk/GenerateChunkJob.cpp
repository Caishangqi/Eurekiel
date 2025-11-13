#include "GenerateChunkJob.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/World/World.hpp"

namespace enigma::voxel
{
    void GenerateChunkJob::Execute()
    {
        // [REFACTORED] Phase 1: Get Chunk via coordinates (eliminates null pointer checks)
        // If Chunk was deleted, GetChunk() returns nullptr and we abort gracefully
        Chunk* chunk = m_world->GetChunk(m_chunkCoords.x, m_chunkCoords.y);
        if (!chunk || !m_generator)
        {
            // Chunk was deleted or generator is null, abort silently
            return;
        }

        // Phase 2: Verify chunk state before accessing
        ChunkState currentState = chunk->GetState();
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

        // Phase 3: Exception protection for terrain generation
        // Wrap the generation call in try-catch to prevent crashes from unexpected errors
        try
        {
            if (chunk->GetState() != ChunkState::Generating) return;
            m_generator->GenerateChunk(chunk, m_chunkCoords.x, m_chunkCoords.y, m_worldSeed);
        }
        catch (const std::exception& e)
        {
            // Log error but don't crash - allow graceful degradation
            LogError("GenerateChunkJob", "Exception during chunk generation (%d, %d): %s",
                     m_chunkCoords.x, m_chunkCoords.y, e.what());
            return;
        }
        catch (...)
        {
            // Catch any other exceptions (non-std::exception types)
            LogError("GenerateChunkJob", "Unknown exception during chunk generation (%d, %d)",
                     m_chunkCoords.x, m_chunkCoords.y);
            return;
        }

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
