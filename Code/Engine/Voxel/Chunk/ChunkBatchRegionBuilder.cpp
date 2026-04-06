#include "ChunkBatchRegionBuilder.hpp"

#include <algorithm>

#include "Engine/Core/EngineCommon.hpp"
#include "Chunk.hpp"
#include "ChunkMesh.hpp"

namespace
{
    using enigma::graphic::TerrainVertex;
    using enigma::voxel::Chunk;
    using enigma::voxel::ChunkBatchLayerSpan;
    using enigma::voxel::ChunkBatchRegionBuildInput;
    using enigma::voxel::ChunkBatchRegionBuildOutput;
    using enigma::voxel::ChunkBatchRegionId;

    Vec3 GetRegionOriginWorldPosition(const ChunkBatchRegionId& regionId)
    {
        const IntVec2 minChunkCoords = enigma::voxel::GetChunkBatchRegionMinChunkCoords(regionId);

        return Vec3(
            static_cast<float>(minChunkCoords.x * Chunk::CHUNK_SIZE_X),
            static_cast<float>(minChunkCoords.y * Chunk::CHUNK_SIZE_Y),
            0.0f);
    }

    Vec3 GetChunkRegionLocalTranslation(const Chunk& chunk, const ChunkBatchRegionId& regionId)
    {
        const IntVec2 minChunkCoords = enigma::voxel::GetChunkBatchRegionMinChunkCoords(regionId);
        const IntVec2 chunkCoords    = chunk.GetChunkCoords();

        return Vec3(
            static_cast<float>((chunkCoords.x - minChunkCoords.x) * Chunk::CHUNK_SIZE_X),
            static_cast<float>((chunkCoords.y - minChunkCoords.y) * Chunk::CHUNK_SIZE_Y),
            0.0f);
    }

    AABB3 BuildChunkWorldBounds(const Chunk& chunk)
    {
        const enigma::voxel::BlockPos chunkWorldPos = chunk.GetWorldPos();
        const Vec3                    mins(
            static_cast<float>(chunkWorldPos.x),
            static_cast<float>(chunkWorldPos.y),
            static_cast<float>(chunkWorldPos.z));
        const Vec3 maxs(
            mins.x + static_cast<float>(Chunk::CHUNK_SIZE_X),
            mins.y + static_cast<float>(Chunk::CHUNK_SIZE_Y),
            mins.z + static_cast<float>(Chunk::CHUNK_SIZE_Z));

        return AABB3(mins, maxs);
    }

    bool IsChunkEligibleForBuild(const Chunk* chunk, const ChunkBatchRegionId& regionId)
    {
        if (chunk == nullptr || !chunk->IsActive())
        {
            return false;
        }

        if (enigma::voxel::GetChunkBatchRegionIdForChunk(chunk->GetChunkCoords()) != regionId)
        {
            return false;
        }

        const enigma::voxel::ChunkMesh* mesh = chunk->GetChunkMesh();
        return mesh != nullptr && !mesh->IsEmpty();
    }

    const std::vector<TerrainVertex>& GetVerticesForLayer(const enigma::voxel::ChunkMesh& mesh, enigma::voxel::ChunkBatchLayer layer)
    {
        switch (layer)
        {
        case enigma::voxel::ChunkBatchLayer::Opaque: return mesh.GetOpaqueTerrainVertices();
        case enigma::voxel::ChunkBatchLayer::Cutout: return mesh.GetCutoutTerrainVertices();
        case enigma::voxel::ChunkBatchLayer::Translucent: return mesh.GetTranslucentTerrainVertices();
        default: return mesh.GetOpaqueTerrainVertices();
        }
    }

    const std::vector<uint32_t>& GetIndicesForLayer(const enigma::voxel::ChunkMesh& mesh, enigma::voxel::ChunkBatchLayer layer)
    {
        switch (layer)
        {
        case enigma::voxel::ChunkBatchLayer::Opaque: return mesh.GetOpaqueIndices();
        case enigma::voxel::ChunkBatchLayer::Cutout: return mesh.GetCutoutIndices();
        case enigma::voxel::ChunkBatchLayer::Translucent: return mesh.GetTranslucentIndices();
        default: return mesh.GetOpaqueIndices();
        }
    }

    void ReserveOutputGeometry(ChunkBatchRegionBuildOutput& output, const std::vector<const Chunk*>& chunks)
    {
        size_t totalVertexCount = 0;
        size_t totalIndexCount  = 0;

        for (const Chunk* chunk : chunks)
        {
            const enigma::voxel::ChunkMesh* mesh = chunk->GetChunkMesh();
            if (mesh == nullptr)
            {
                continue;
            }

            totalVertexCount += mesh->GetOpaqueVertexCount();
            totalVertexCount += mesh->GetCutoutVertexCount();
            totalVertexCount += mesh->GetTranslucentVertexCount();
            totalIndexCount += mesh->GetOpaqueIndexCount();
            totalIndexCount += mesh->GetCutoutIndexCount();
            totalIndexCount += mesh->GetTranslucentIndexCount();
        }

        output.vertices.reserve(totalVertexCount);
        output.indices.reserve(totalIndexCount);
    }

    bool AppendLayerGeometry(
        ChunkBatchRegionBuildOutput& output,
        const enigma::voxel::ChunkMesh& mesh,
        enigma::voxel::ChunkBatchLayer layer,
        const Vec3& translation)
    {
        const std::vector<TerrainVertex>& sourceVertices = GetVerticesForLayer(mesh, layer);
        const std::vector<uint32_t>&      sourceIndices  = GetIndicesForLayer(mesh, layer);
        if (sourceVertices.empty() || sourceIndices.empty())
        {
            return false;
        }

        const uint32_t baseVertex = static_cast<uint32_t>(output.vertices.size());
        output.vertices.reserve(output.vertices.size() + sourceVertices.size());

        for (const TerrainVertex& sourceVertex : sourceVertices)
        {
            TerrainVertex translatedVertex = sourceVertex;
            translatedVertex.m_position += translation;
            output.vertices.push_back(translatedVertex);
        }

        output.indices.reserve(output.indices.size() + sourceIndices.size());
        for (uint32_t sourceIndex : sourceIndices)
        {
            if (sourceIndex >= sourceVertices.size())
            {
                ERROR_AND_DIE("ChunkBatchRegionBuilder encountered invalid layer geometry indices");
            }

            output.indices.push_back(baseVertex + sourceIndex);
        }

        return true;
    }

    ChunkBatchLayerSpan BuildLayerSpan(
        ChunkBatchRegionBuildOutput& output,
        const std::vector<const Chunk*>& chunks,
        const ChunkBatchRegionId& regionId,
        enigma::voxel::ChunkBatchLayer layer,
        bool& hasWorldBounds,
        AABB3& worldBounds)
    {
        ChunkBatchLayerSpan span;
        span.startIndex = static_cast<uint32_t>(output.indices.size());

        for (const Chunk* chunk : chunks)
        {
            const enigma::voxel::ChunkMesh* mesh = chunk->GetChunkMesh();
            if (mesh == nullptr)
            {
                continue;
            }

            const bool appended = AppendLayerGeometry(
                output,
                *mesh,
                layer,
                GetChunkRegionLocalTranslation(*chunk, regionId));
            if (!appended)
            {
                continue;
            }

            const AABB3 chunkBounds = BuildChunkWorldBounds(*chunk);
            if (!hasWorldBounds)
            {
                worldBounds    = chunkBounds;
                hasWorldBounds = true;
            }
            else
            {
                worldBounds.StretchToIncludePoint(chunkBounds.m_mins);
                worldBounds.StretchToIncludePoint(chunkBounds.m_maxs);
            }
        }

        span.indexCount = static_cast<uint32_t>(output.indices.size()) - span.startIndex;
        return span;
    }

    std::vector<const Chunk*> GetSortedEligibleChunks(const ChunkBatchRegionBuildInput& input)
    {
        std::vector<const Chunk*> sortedChunks;
        sortedChunks.reserve(input.chunks.size());

        for (const Chunk* chunk : input.chunks)
        {
            if (IsChunkEligibleForBuild(chunk, input.regionId))
            {
                sortedChunks.push_back(chunk);
            }
        }

        std::sort(
            sortedChunks.begin(),
            sortedChunks.end(),
            [](const Chunk* lhs, const Chunk* rhs)
            {
                if (lhs->GetChunkX() != rhs->GetChunkX())
                {
                    return lhs->GetChunkX() < rhs->GetChunkX();
                }

                return lhs->GetChunkY() < rhs->GetChunkY();
            });

        sortedChunks.erase(
            std::unique(
                sortedChunks.begin(),
                sortedChunks.end(),
                [](const Chunk* lhs, const Chunk* rhs)
                {
                    return lhs->GetChunkCoords() == rhs->GetChunkCoords();
                }),
            sortedChunks.end());

        return sortedChunks;
    }
}

namespace enigma::voxel
{
    ChunkBatchRegionBuildOutput ChunkBatchRegionBuilder::BuildRegionGeometry(const ChunkBatchRegionBuildInput& input)
    {
        ChunkBatchRegionBuildOutput output;

        const Vec3 regionOriginWorld = GetRegionOriginWorldPosition(input.regionId);
        output.geometry.regionModelMatrix = Mat44::MakeTranslation3D(regionOriginWorld);
        output.geometry.worldBounds       = AABB3(regionOriginWorld, regionOriginWorld);
        output.geometry.gpuDataValid      = false;

        const std::vector<const Chunk*> chunks = GetSortedEligibleChunks(input);
        if (chunks.empty())
        {
            return output;
        }

        ReserveOutputGeometry(output, chunks);

        bool hasWorldBounds = false;
        AABB3 worldBounds(regionOriginWorld, regionOriginWorld);

        output.geometry.opaque = BuildLayerSpan(output, chunks, input.regionId, ChunkBatchLayer::Opaque, hasWorldBounds, worldBounds);
        output.geometry.cutout = BuildLayerSpan(output, chunks, input.regionId, ChunkBatchLayer::Cutout, hasWorldBounds, worldBounds);
        output.geometry.translucent = BuildLayerSpan(output, chunks, input.regionId, ChunkBatchLayer::Translucent, hasWorldBounds, worldBounds);

        if (hasWorldBounds)
        {
            output.geometry.worldBounds = worldBounds;
        }

        return output;
    }
}
