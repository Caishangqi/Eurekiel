#include "ChunkMeshBuildInputFactory.hpp"

#include "Engine/Voxel/Chunk/Chunk.hpp"
#include "Engine/Voxel/World/World.hpp"

using namespace enigma::voxel;

namespace
{
    ChunkMeshingDispatchContext MakeDispatchContext(const Chunk& chunk, uint64_t buildVersion, bool important)
    {
        ChunkMeshingDispatchContext context;
        context.chunkCoords       = chunk.GetChunkCoords();
        context.chunkInstanceId   = chunk.GetInstanceId();
        context.buildVersion      = buildVersion;
        context.important         = important;
        context.worldLifetimeToken = chunk.GetWorld() != nullptr ? chunk.GetWorld()->GetWorldLifetimeToken() : 0;
        return context;
    }
}

bool ChunkMeshBuildInputFactory::TryCreate(const Chunk& chunk, uint64_t buildVersion, bool important, ChunkMeshBuildInput& outInput)
{
    return TryCreate(chunk, buildVersion, important, {}, outInput);
}

bool ChunkMeshBuildInputFactory::TryCreate(const Chunk& chunk,
                                           uint64_t buildVersion,
                                           bool important,
                                           enigma::graphic::RenderPipelineReloadGeneration reloadGeneration,
                                           ChunkMeshBuildInput& outInput)
{
    outInput = {};

    if (!chunk.IsActive() || chunk.GetWorld() == nullptr)
    {
        return false;
    }

    outInput.dispatchContext = MakeDispatchContext(chunk, buildVersion, important);
    outInput.reloadGeneration = reloadGeneration;
    return true;
}
