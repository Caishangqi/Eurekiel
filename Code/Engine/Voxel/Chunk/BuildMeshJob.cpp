#include "BuildMeshJob.hpp"
#include "Chunk.hpp"
#include "ChunkMeshBuilder.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::voxel;

//-----------------------------------------------------------------------------------------------
// Execute: Build chunk mesh on worker thread (CPU-intensive, thread-safe)
//-----------------------------------------------------------------------------------------------
void BuildMeshJob::Execute()
{
    if (!m_chunk)
    {
        core::LogError("BuildMeshJob", "Execute() called with null chunk!");
        return;
    }

    IntVec2 coords = GetChunkCoords();
    core::LogInfo("BuildMeshJob",
                  "Execute() started for chunk (%d, %d) priority=%s",
                  coords.x, coords.y,
                  m_priority == core::TaskPriority::High ? "High" : "Normal");

    // CPU-intensive mesh building (1-5ms per chunk)
    // This is thread-safe: only reads chunk block data
    ChunkMeshBuilder builder;
    if (m_chunk->GetState() == ChunkState::Inactive) return;
    m_resultMesh = builder.BuildMesh(m_chunk);

    core::LogInfo("BuildMeshJob",
                  "Execute() completed for chunk (%d, %d), mesh vertices=%d",
                  coords.x, coords.y,
                  m_resultMesh ? (int)m_resultMesh->GetOpaqueVertexCount() + m_resultMesh->GetTransparentVertexCount() : 0);
}
