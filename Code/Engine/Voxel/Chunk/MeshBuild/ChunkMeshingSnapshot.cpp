#include "ChunkMeshingSnapshot.hpp"

#include <utility>

using namespace enigma::voxel;

ChunkMeshingSnapshot::ChunkMeshingSnapshot()
    : ChunkMeshingSnapshot(std::make_shared<ChunkMeshingScratch>())
{
}

ChunkMeshingSnapshot::ChunkMeshingSnapshot(std::shared_ptr<ChunkMeshingScratch> scratchStorage)
    : scratch(scratchStorage != nullptr ? std::move(scratchStorage) : std::make_shared<ChunkMeshingScratch>())
    , centerBlocks(scratch->centerBlocks)
    , centerLights(scratch->centerLights)
    , northEdgeBlocks(scratch->northEdgeBlocks)
    , southEdgeBlocks(scratch->southEdgeBlocks)
    , eastEdgeBlocks(scratch->eastEdgeBlocks)
    , westEdgeBlocks(scratch->westEdgeBlocks)
    , northEdgeLights(scratch->northEdgeLights)
    , southEdgeLights(scratch->southEdgeLights)
    , eastEdgeLights(scratch->eastEdgeLights)
    , westEdgeLights(scratch->westEdgeLights)
    , northEastCornerBlocks(scratch->northEastCornerBlocks)
    , northWestCornerBlocks(scratch->northWestCornerBlocks)
    , southEastCornerBlocks(scratch->southEastCornerBlocks)
    , southWestCornerBlocks(scratch->southWestCornerBlocks)
    , hasNorthNeighbor(scratch->hasNorthNeighbor)
    , hasSouthNeighbor(scratch->hasSouthNeighbor)
    , hasEastNeighbor(scratch->hasEastNeighbor)
    , hasWestNeighbor(scratch->hasWestNeighbor)
{
    scratch->EnsureCapacity();
    Reset();
}

void ChunkMeshingSnapshot::Reset()
{
    chunkCoords = IntVec2(0, 0);
    scratch->BeginBuild();
}

bool ChunkMeshingSnapshot::HasAllHorizontalNeighbors() const noexcept
{
    return hasNorthNeighbor && hasSouthNeighbor && hasEastNeighbor && hasWestNeighbor;
}

BlockState* ChunkMeshingSnapshot::GetCenterBlock(int32_t x, int32_t y, int32_t z) const
{
    return centerBlocks[GetCenterIndex(x, y, z)];
}

ChunkMeshingLightSample ChunkMeshingSnapshot::GetCenterLight(int32_t x, int32_t y, int32_t z) const
{
    return centerLights[GetCenterIndex(x, y, z)];
}

BlockState* ChunkMeshingSnapshot::GetBlock(int32_t x, int32_t y, int32_t z) const
{
    if (!IsInsideVerticalRange(z))
    {
        return nullptr;
    }

    if (IsInsideHorizontalRange(x) && IsInsideHorizontalRange(y))
    {
        return GetCenterBlock(x, y, z);
    }

    if (x < 0)
    {
        if (y < 0)
        {
            return southWestCornerBlocks[GetCornerIndex(z)];
        }

        if (y >= Chunk::CHUNK_SIZE_Y)
        {
            return northWestCornerBlocks[GetCornerIndex(z)];
        }

        return westEdgeBlocks[GetEastWestEdgeIndex(y, z)];
    }

    if (x >= Chunk::CHUNK_SIZE_X)
    {
        if (y < 0)
        {
            return southEastCornerBlocks[GetCornerIndex(z)];
        }

        if (y >= Chunk::CHUNK_SIZE_Y)
        {
            return northEastCornerBlocks[GetCornerIndex(z)];
        }

        return eastEdgeBlocks[GetEastWestEdgeIndex(y, z)];
    }

    if (y < 0)
    {
        return southEdgeBlocks[GetNorthSouthEdgeIndex(x, z)];
    }

    if (y >= Chunk::CHUNK_SIZE_Y)
    {
        return northEdgeBlocks[GetNorthSouthEdgeIndex(x, z)];
    }

    return nullptr;
}

ChunkMeshingLightSample ChunkMeshingSnapshot::GetLight(int32_t x, int32_t y, int32_t z) const
{
    if (!IsInsideVerticalRange(z))
    {
        return ChunkMeshingLightSample{};
    }

    if (IsInsideHorizontalRange(x) && IsInsideHorizontalRange(y))
    {
        return GetCenterLight(x, y, z);
    }

    if (x < 0 && IsInsideHorizontalRange(y))
    {
        return westEdgeLights[GetEastWestEdgeIndex(y, z)];
    }

    if (x >= Chunk::CHUNK_SIZE_X && IsInsideHorizontalRange(y))
    {
        return eastEdgeLights[GetEastWestEdgeIndex(y, z)];
    }

    if (y < 0 && IsInsideHorizontalRange(x))
    {
        return southEdgeLights[GetNorthSouthEdgeIndex(x, z)];
    }

    if (y >= Chunk::CHUNK_SIZE_Y && IsInsideHorizontalRange(x))
    {
        return northEdgeLights[GetNorthSouthEdgeIndex(x, z)];
    }

    return ChunkMeshingLightSample{};
}

bool ChunkMeshingSnapshot::IsInsideHorizontalRange(int32_t value) noexcept
{
    return value >= 0 && value < Chunk::CHUNK_SIZE_X;
}

bool ChunkMeshingSnapshot::IsInsideVerticalRange(int32_t value) noexcept
{
    return value >= 0 && value < Chunk::CHUNK_SIZE_Z;
}

size_t ChunkMeshingSnapshot::GetCenterIndex(int32_t x, int32_t y, int32_t z)
{
    return Chunk::CoordsToIndex(x, y, z);
}

size_t ChunkMeshingSnapshot::GetNorthSouthEdgeIndex(int32_t x, int32_t z)
{
    return static_cast<size_t>(x + (z * Chunk::CHUNK_SIZE_X));
}

size_t ChunkMeshingSnapshot::GetEastWestEdgeIndex(int32_t y, int32_t z)
{
    return static_cast<size_t>(y + (z * Chunk::CHUNK_SIZE_Y));
}

size_t ChunkMeshingSnapshot::GetCornerIndex(int32_t z)
{
    return static_cast<size_t>(z);
}
