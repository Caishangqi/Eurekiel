#include "ChunkMeshingMaterializer.hpp"

#include "Engine/Voxel/Chunk/Chunk.hpp"

using namespace enigma::voxel;

namespace
{
    size_t GetNorthSouthEdgeIndex(int32_t x, int32_t z)
    {
        return static_cast<size_t>(x + (z * Chunk::CHUNK_SIZE_X));
    }

    size_t GetEastWestEdgeIndex(int32_t y, int32_t z)
    {
        return static_cast<size_t>(y + (z * Chunk::CHUNK_SIZE_Y));
    }

    size_t GetCornerIndex(int32_t z)
    {
        return static_cast<size_t>(z);
    }

    ChunkMeshingLightSample MakeLightSample(const Chunk& chunk, int32_t x, int32_t y, int32_t z)
    {
        ChunkMeshingLightSample sample;
        sample.skyLight   = chunk.GetSkyLight(x, y, z);
        sample.blockLight = chunk.GetBlockLight(x, y, z);
        return sample;
    }

    const Chunk* ResolveHorizontalNeighbor(const Chunk& chunk, Direction direction)
    {
        const Chunk* neighbor = nullptr;
        switch (direction)
        {
        case Direction::NORTH: neighbor = chunk.GetNorthNeighbor(); break;
        case Direction::SOUTH: neighbor = chunk.GetSouthNeighbor(); break;
        case Direction::EAST: neighbor = chunk.GetEastNeighbor(); break;
        case Direction::WEST: neighbor = chunk.GetWestNeighbor(); break;
        case Direction::UP:
        case Direction::DOWN:
            break;
        }

        if (neighbor == nullptr || !neighbor->CanReadForMeshing())
        {
            return nullptr;
        }

        return neighbor;
    }

    bool CopyHorizontalNeighborPlane(const Chunk& sourceChunk, Direction direction, ChunkMeshingSnapshot& snapshot)
    {
        switch (direction)
        {
        case Direction::NORTH:
            for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
            {
                for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
                {
                    snapshot.northEdgeBlocks[GetNorthSouthEdgeIndex(x, z)] = sourceChunk.GetBlock(x, 0, z);
                    snapshot.northEdgeLights[GetNorthSouthEdgeIndex(x, z)] = MakeLightSample(sourceChunk, x, 0, z);
                }
            }
            return true;

        case Direction::SOUTH:
            for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
            {
                for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
                {
                    snapshot.southEdgeBlocks[GetNorthSouthEdgeIndex(x, z)] = sourceChunk.GetBlock(x, Chunk::CHUNK_MAX_Y, z);
                    snapshot.southEdgeLights[GetNorthSouthEdgeIndex(x, z)] =
                        MakeLightSample(sourceChunk, x, Chunk::CHUNK_MAX_Y, z);
                }
            }
            return true;

        case Direction::EAST:
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
                {
                    snapshot.eastEdgeBlocks[GetEastWestEdgeIndex(y, z)] = sourceChunk.GetBlock(0, y, z);
                    snapshot.eastEdgeLights[GetEastWestEdgeIndex(y, z)] = MakeLightSample(sourceChunk, 0, y, z);
                }
            }
            return true;

        case Direction::WEST:
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
                {
                    snapshot.westEdgeBlocks[GetEastWestEdgeIndex(y, z)] = sourceChunk.GetBlock(Chunk::CHUNK_MAX_X, y, z);
                    snapshot.westEdgeLights[GetEastWestEdgeIndex(y, z)] =
                        MakeLightSample(sourceChunk, Chunk::CHUNK_MAX_X, y, z);
                }
            }
            return true;

        case Direction::UP:
        case Direction::DOWN:
            break;
        }

        return false;
    }

    void CopyDiagonalCornerColumn(const Chunk* sourceChunk, int32_t sourceX, int32_t sourceY, std::vector<BlockState*>& targetBlocks)
    {
        if (sourceChunk == nullptr || !sourceChunk->CanReadForMeshing())
        {
            return;
        }

        for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
        {
            targetBlocks[GetCornerIndex(z)] = sourceChunk->GetBlock(sourceX, sourceY, z);
        }
    }
}

ChunkMeshingMaterializationResult ChunkMeshingMaterializer::TryMaterialize(const Chunk& chunk,
                                                                           const ChunkMeshingDispatchContext& context,
                                                                           ChunkMeshingScratch& scratch,
                                                                           ChunkMeshingSnapshot& outSnapshot)
{
    if (!context.IsValid())
    {
        return {ChunkMeshingMaterializationStatus::InvalidDispatchContext, "InvalidDispatchContext"};
    }

    if (&outSnapshot.GetScratch() != &scratch)
    {
        return {ChunkMeshingMaterializationStatus::InvalidScratchBinding, "InvalidScratchBinding"};
    }

    if (chunk.GetChunkCoords() != context.chunkCoords)
    {
        return {ChunkMeshingMaterializationStatus::ChunkCoordsMismatch, "ChunkCoordsMismatch"};
    }

    if (chunk.GetInstanceId() != context.chunkInstanceId)
    {
        return {ChunkMeshingMaterializationStatus::ChunkInstanceMismatch, "ChunkInstanceMismatch"};
    }

    if (chunk.GetWorld() == nullptr)
    {
        return {ChunkMeshingMaterializationStatus::MissingWorld, "MissingWorld"};
    }

    if (!chunk.CanReadForMeshing())
    {
        return {ChunkMeshingMaterializationStatus::ChunkInactive, "ChunkInactive"};
    }

    outSnapshot.Reset();
    outSnapshot.chunkCoords = chunk.GetChunkCoords();

    const bool captureCenterBlocks = context.RequiresCaptureHint(ChunkMeshingCaptureHint::CenterBlockData);
    const bool captureLights       = context.RequiresCaptureHint(ChunkMeshingCaptureHint::LightingData);
    if (captureCenterBlocks || captureLights)
    {
        for (int32_t x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
        {
            for (int32_t y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
            {
                for (int32_t z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
                {
                    const size_t blockIndex = Chunk::CoordsToIndex(x, y, z);
                    if (captureCenterBlocks)
                    {
                        outSnapshot.centerBlocks[blockIndex] = chunk.GetBlock(x, y, z);
                    }

                    if (captureLights)
                    {
                        outSnapshot.centerLights[blockIndex] = MakeLightSample(chunk, x, y, z);
                    }
                }
            }
        }
    }

    const Chunk* northChunk = ResolveHorizontalNeighbor(chunk, Direction::NORTH);
    const Chunk* southChunk = ResolveHorizontalNeighbor(chunk, Direction::SOUTH);
    const Chunk* eastChunk  = ResolveHorizontalNeighbor(chunk, Direction::EAST);
    const Chunk* westChunk  = ResolveHorizontalNeighbor(chunk, Direction::WEST);
    if (northChunk == nullptr || southChunk == nullptr || eastChunk == nullptr || westChunk == nullptr)
    {
        return {ChunkMeshingMaterializationStatus::MissingHorizontalNeighbors, "MissingHorizontalNeighbors"};
    }

    outSnapshot.hasNorthNeighbor = CopyHorizontalNeighborPlane(*northChunk, Direction::NORTH, outSnapshot);
    outSnapshot.hasSouthNeighbor = CopyHorizontalNeighborPlane(*southChunk, Direction::SOUTH, outSnapshot);
    outSnapshot.hasEastNeighbor  = CopyHorizontalNeighborPlane(*eastChunk, Direction::EAST, outSnapshot);
    outSnapshot.hasWestNeighbor  = CopyHorizontalNeighborPlane(*westChunk, Direction::WEST, outSnapshot);
    if (!outSnapshot.HasAllHorizontalNeighbors())
    {
        return {ChunkMeshingMaterializationStatus::MissingHorizontalNeighbors, "MissingHorizontalNeighbors"};
    }

    if (context.RequiresCaptureHint(ChunkMeshingCaptureHint::DiagonalCorners))
    {
        const Chunk* northEastChunk = northChunk->GetEastNeighbor();
        if (northEastChunk == nullptr || !northEastChunk->CanReadForMeshing())
        {
            northEastChunk = eastChunk->GetNorthNeighbor();
            if (northEastChunk != nullptr && !northEastChunk->CanReadForMeshing())
            {
                northEastChunk = nullptr;
            }
        }

        const Chunk* northWestChunk = northChunk->GetWestNeighbor();
        if (northWestChunk == nullptr || !northWestChunk->CanReadForMeshing())
        {
            northWestChunk = westChunk->GetNorthNeighbor();
            if (northWestChunk != nullptr && !northWestChunk->CanReadForMeshing())
            {
                northWestChunk = nullptr;
            }
        }

        const Chunk* southEastChunk = southChunk->GetEastNeighbor();
        if (southEastChunk == nullptr || !southEastChunk->CanReadForMeshing())
        {
            southEastChunk = eastChunk->GetSouthNeighbor();
            if (southEastChunk != nullptr && !southEastChunk->CanReadForMeshing())
            {
                southEastChunk = nullptr;
            }
        }

        const Chunk* southWestChunk = southChunk->GetWestNeighbor();
        if (southWestChunk == nullptr || !southWestChunk->CanReadForMeshing())
        {
            southWestChunk = westChunk->GetSouthNeighbor();
            if (southWestChunk != nullptr && !southWestChunk->CanReadForMeshing())
            {
                southWestChunk = nullptr;
            }
        }

        CopyDiagonalCornerColumn(northEastChunk, 0, 0, outSnapshot.northEastCornerBlocks);
        CopyDiagonalCornerColumn(northWestChunk, Chunk::CHUNK_MAX_X, 0, outSnapshot.northWestCornerBlocks);
        CopyDiagonalCornerColumn(southEastChunk, 0, Chunk::CHUNK_MAX_Y, outSnapshot.southEastCornerBlocks);
        CopyDiagonalCornerColumn(southWestChunk, Chunk::CHUNK_MAX_X, Chunk::CHUNK_MAX_Y, outSnapshot.southWestCornerBlocks);
    }

    return {ChunkMeshingMaterializationStatus::Materialized, "Materialized"};
}
