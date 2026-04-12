#pragma once

#include "Engine/Voxel/Chunk/MeshBuild/ChunkMeshingScratch.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"

#include <cstddef>
#include <memory>

namespace enigma::voxel
{
    struct ChunkMeshingSnapshot
    {
        static constexpr size_t kHorizontalEdgeBlockCount = ChunkMeshingScratch::kHorizontalEdgeBlockCount;
        static constexpr size_t kVerticalEdgeBlockCount   = ChunkMeshingScratch::kVerticalEdgeBlockCount;
        static constexpr size_t kCornerColumnBlockCount   = ChunkMeshingScratch::kCornerColumnBlockCount;

        IntVec2                                chunkCoords = IntVec2(0, 0);
        std::shared_ptr<ChunkMeshingScratch>   scratch;
        std::vector<BlockState*>&              centerBlocks;
        std::vector<ChunkMeshingLightSample>&  centerLights;
        std::vector<BlockState*>&              northEdgeBlocks;
        std::vector<BlockState*>&              southEdgeBlocks;
        std::vector<BlockState*>&              eastEdgeBlocks;
        std::vector<BlockState*>&              westEdgeBlocks;
        std::vector<ChunkMeshingLightSample>&  northEdgeLights;
        std::vector<ChunkMeshingLightSample>&  southEdgeLights;
        std::vector<ChunkMeshingLightSample>&  eastEdgeLights;
        std::vector<ChunkMeshingLightSample>&  westEdgeLights;
        std::vector<BlockState*>&              northEastCornerBlocks;
        std::vector<BlockState*>&              northWestCornerBlocks;
        std::vector<BlockState*>&              southEastCornerBlocks;
        std::vector<BlockState*>&              southWestCornerBlocks;
        bool&                                  hasNorthNeighbor;
        bool&                                  hasSouthNeighbor;
        bool&                                  hasEastNeighbor;
        bool&                                  hasWestNeighbor;

        ChunkMeshingSnapshot();
        explicit ChunkMeshingSnapshot(std::shared_ptr<ChunkMeshingScratch> scratchStorage);

        void Reset();
        bool HasAllHorizontalNeighbors() const noexcept;
        BlockState* GetCenterBlock(int32_t x, int32_t y, int32_t z) const;
        ChunkMeshingLightSample GetCenterLight(int32_t x, int32_t y, int32_t z) const;
        BlockState* GetBlock(int32_t x, int32_t y, int32_t z) const;
        ChunkMeshingLightSample GetLight(int32_t x, int32_t y, int32_t z) const;
        ChunkMeshingScratch& GetScratch() noexcept { return *scratch; }
        const ChunkMeshingScratch& GetScratch() const noexcept { return *scratch; }

    private:
        static bool IsInsideHorizontalRange(int32_t value) noexcept;
        static bool IsInsideVerticalRange(int32_t value) noexcept;
        static size_t GetCenterIndex(int32_t x, int32_t y, int32_t z);
        static size_t GetNorthSouthEdgeIndex(int32_t x, int32_t z);
        static size_t GetEastWestEdgeIndex(int32_t y, int32_t z);
        static size_t GetCornerIndex(int32_t z);
    };
}
