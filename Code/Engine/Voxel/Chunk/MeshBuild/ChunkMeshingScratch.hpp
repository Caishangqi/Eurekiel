#pragma once

#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Chunk/Chunk.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace enigma::voxel
{
    struct ChunkMeshingLightSample
    {
        uint8_t skyLight   = 0;
        uint8_t blockLight = 0;
    };

    struct ChunkMeshingScratch
    {
        static constexpr size_t kCenterBlockCount         = static_cast<size_t>(Chunk::BLOCKS_PER_CHUNK);
        static constexpr size_t kHorizontalEdgeBlockCount = static_cast<size_t>(Chunk::CHUNK_SIZE_X * Chunk::CHUNK_SIZE_Z);
        static constexpr size_t kVerticalEdgeBlockCount   = static_cast<size_t>(Chunk::CHUNK_SIZE_Y * Chunk::CHUNK_SIZE_Z);
        static constexpr size_t kCornerColumnBlockCount   = static_cast<size_t>(Chunk::CHUNK_SIZE_Z);

        std::vector<BlockState*> centerBlocks;
        std::vector<ChunkMeshingLightSample> centerLights;
        std::vector<BlockState*> northEdgeBlocks;
        std::vector<BlockState*> southEdgeBlocks;
        std::vector<BlockState*> eastEdgeBlocks;
        std::vector<BlockState*> westEdgeBlocks;
        std::vector<ChunkMeshingLightSample> northEdgeLights;
        std::vector<ChunkMeshingLightSample> southEdgeLights;
        std::vector<ChunkMeshingLightSample> eastEdgeLights;
        std::vector<ChunkMeshingLightSample> westEdgeLights;
        std::vector<BlockState*> northEastCornerBlocks;
        std::vector<BlockState*> northWestCornerBlocks;
        std::vector<BlockState*> southEastCornerBlocks;
        std::vector<BlockState*> southWestCornerBlocks;
        bool hasNorthNeighbor = false;
        bool hasSouthNeighbor = false;
        bool hasEastNeighbor  = false;
        bool hasWestNeighbor  = false;

        ChunkMeshingScratch()
        {
            EnsureCapacity();
            ResetFlags();
        }

        void EnsureCapacity()
        {
            if (centerBlocks.size() != kCenterBlockCount)
            {
                centerBlocks.resize(kCenterBlockCount, nullptr);
            }

            if (centerLights.size() != kCenterBlockCount)
            {
                centerLights.resize(kCenterBlockCount);
            }

            if (northEdgeBlocks.size() != kHorizontalEdgeBlockCount)
            {
                northEdgeBlocks.resize(kHorizontalEdgeBlockCount, nullptr);
            }

            if (southEdgeBlocks.size() != kHorizontalEdgeBlockCount)
            {
                southEdgeBlocks.resize(kHorizontalEdgeBlockCount, nullptr);
            }

            if (eastEdgeBlocks.size() != kVerticalEdgeBlockCount)
            {
                eastEdgeBlocks.resize(kVerticalEdgeBlockCount, nullptr);
            }

            if (westEdgeBlocks.size() != kVerticalEdgeBlockCount)
            {
                westEdgeBlocks.resize(kVerticalEdgeBlockCount, nullptr);
            }

            if (northEdgeLights.size() != kHorizontalEdgeBlockCount)
            {
                northEdgeLights.resize(kHorizontalEdgeBlockCount);
            }

            if (southEdgeLights.size() != kHorizontalEdgeBlockCount)
            {
                southEdgeLights.resize(kHorizontalEdgeBlockCount);
            }

            if (eastEdgeLights.size() != kVerticalEdgeBlockCount)
            {
                eastEdgeLights.resize(kVerticalEdgeBlockCount);
            }

            if (westEdgeLights.size() != kVerticalEdgeBlockCount)
            {
                westEdgeLights.resize(kVerticalEdgeBlockCount);
            }

            if (northEastCornerBlocks.size() != kCornerColumnBlockCount)
            {
                northEastCornerBlocks.resize(kCornerColumnBlockCount, nullptr);
            }

            if (northWestCornerBlocks.size() != kCornerColumnBlockCount)
            {
                northWestCornerBlocks.resize(kCornerColumnBlockCount, nullptr);
            }

            if (southEastCornerBlocks.size() != kCornerColumnBlockCount)
            {
                southEastCornerBlocks.resize(kCornerColumnBlockCount, nullptr);
            }

            if (southWestCornerBlocks.size() != kCornerColumnBlockCount)
            {
                southWestCornerBlocks.resize(kCornerColumnBlockCount, nullptr);
            }
        }

        void BeginBuild()
        {
            EnsureCapacity();
            ResetFlags();
        }

        void ResetFlags()
        {
            hasNorthNeighbor = false;
            hasSouthNeighbor = false;
            hasEastNeighbor  = false;
            hasWestNeighbor  = false;

            std::fill(northEastCornerBlocks.begin(), northEastCornerBlocks.end(), nullptr);
            std::fill(northWestCornerBlocks.begin(), northWestCornerBlocks.end(), nullptr);
            std::fill(southEastCornerBlocks.begin(), southEastCornerBlocks.end(), nullptr);
            std::fill(southWestCornerBlocks.begin(), southWestCornerBlocks.end(), nullptr);
        }
    };
}
