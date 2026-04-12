#pragma once

#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshBuildInput.hpp"

namespace enigma::voxel
{
    class Chunk;

    class ChunkMeshBuildInputFactory
    {
    public:
        static bool TryCreate(const Chunk& chunk, uint64_t buildVersion, bool important, ChunkMeshBuildInput& outInput);
    };
}
