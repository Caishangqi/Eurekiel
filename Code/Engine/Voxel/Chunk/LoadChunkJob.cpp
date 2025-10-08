#include "LoadChunkJob.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::voxel
{
    void LoadChunkJob::Execute()
    {
        // Safety check: ensure chunk is still in Loading state
        ChunkState currentState = m_chunk->GetState();
        if (currentState != ChunkState::Loading)
        {
            // Chunk was cancelled or state changed, abort loading
            m_loadSuccess = false;
            return;
        }

        // Check cancellation before expensive IO
        if (IsCancelled())
        {
            m_loadSuccess = false;
            return;
        }

        // Perform chunk loading from disk (supports both ESF and ESFS)
        GUARANTEE_OR_DIE(m_chunk != nullptr, "LoadChunkJob: Chunk is null");

        if (m_esfStorage != nullptr)
        {
            // ESF format
            GUARANTEE_OR_DIE(m_esfsStorage == nullptr, "LoadChunkJob: Both storages are set");
            m_loadSuccess = m_esfStorage->LoadChunkData(m_chunk, m_chunkCoords.x, m_chunkCoords.y);
        }
        else if (m_esfsStorage != nullptr)
        {
            // ESFS format
            GUARANTEE_OR_DIE(m_esfStorage == nullptr, "LoadChunkJob: Both storages are set");
            m_loadSuccess = m_esfsStorage->LoadChunkData(m_chunk, m_chunkCoords.x, m_chunkCoords.y);
        }
        else
        {
            GUARANTEE_OR_DIE(false, "LoadChunkJob: No storage configured");
        }

        // Check cancellation after loading
        if (IsCancelled())
        {
            // Load completed but chunk is being unloaded
            m_loadSuccess = false;
            return;
        }

        // If load failed, main thread will transition to PendingGenerate state
        // If load succeeded, main thread will transition to Active state
    }
} // namespace enigma::voxel
