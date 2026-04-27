#pragma once

#include "Engine/Graphic/Reload/RenderPipelineReloadTypes.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingDispatchContext.hpp"

#include <cstdint>
#include <memory>

namespace enigma::voxel
{
    struct ChunkMeshingSnapshot;

    struct ChunkMeshBuildInput
    {
        ChunkMeshingDispatchContext                 dispatchContext;
        std::shared_ptr<const ChunkMeshingSnapshot> snapshot;
        enigma::graphic::RenderPipelineReloadGeneration reloadGeneration;

        const IntVec2& GetChunkCoords() const noexcept
        {
            return dispatchContext.chunkCoords;
        }

        uint64_t GetChunkInstanceId() const noexcept
        {
            return dispatchContext.chunkInstanceId;
        }

        uint64_t GetBuildVersion() const noexcept
        {
            return dispatchContext.buildVersion;
        }

        enigma::graphic::RenderPipelineReloadGeneration GetReloadGeneration() const noexcept
        {
            return reloadGeneration;
        }

        bool IsImportant() const noexcept
        {
            return dispatchContext.important;
        }

        bool HasDispatchContext() const noexcept
        {
            return dispatchContext.IsValid();
        }

        bool HasSnapshot() const noexcept
        {
            return snapshot != nullptr;
        }

        bool RequiresWorkerMaterialization() const noexcept
        {
            return !HasSnapshot();
        }
    };
}
