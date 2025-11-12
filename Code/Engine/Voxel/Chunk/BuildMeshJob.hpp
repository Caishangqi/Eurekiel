#pragma once
#include "ChunkJob.hpp"
#include "ChunkMesh.hpp"
#include <memory>

#include "Engine/Core/Schedule/ScheduleSubsystem.hpp"

namespace enigma::voxel
{
    // Forward declaration
    class Chunk;

    // Import TaskPriority from enigma::core namespace
    using enigma::core::TaskPriority;

    //-----------------------------------------------------------------------------------------------
    // Job for asynchronous chunk mesh building on worker threads
    // CPU-intensive work: Iterate 16x16x128 blocks, check neighbors, build mesh vertices
    //
    // THREADING DESIGN:
    // - Execute() runs on worker thread (ChunkGen or MeshBuilding pool)
    // - BuildMesh() is CPU-bound and thread-safe (reads chunk blocks, doesn't modify)
    // - CompileToGPU() MUST run on main thread (DirectX 11 limitation)
    //
    // WORKFLOW:
    // 1. Worker thread: BuildMeshJob::Execute() -> ChunkMeshHelper::BuildMesh()
    // 2. Main thread: ProcessCompletedMeshJobs() -> CompileToGPU() -> SetMesh()
    //-----------------------------------------------------------------------------------------------
    class BuildMeshJob : public ChunkJob
    {
    public:
        BuildMeshJob(IntVec2 chunkCoords, Chunk* chunk, TaskPriority priority = TaskPriority::Normal)
            : ChunkJob(TaskTypeConstants::MESH_BUILDING, chunkCoords)
              , m_chunk(chunk)
              , m_priority(priority)
        {
        }

        void Execute() override;

        // Result access (called by main thread after job completes)
        std::unique_ptr<ChunkMesh> TakeMesh() { return std::move(m_resultMesh); }
        Chunk*                     GetChunk() const { return m_chunk; }
        TaskPriority               GetPriority() const { return m_priority; }

    private:
        Chunk*                     m_chunk; // Target chunk (main thread owns, worker only reads)
        TaskPriority               m_priority; // Job priority (High for player interaction)
        std::unique_ptr<ChunkMesh> m_resultMesh; // Built mesh (worker writes, main thread reads)
    };
} // namespace enigma::voxel
