#include "ChunkBatchRegionBuilder.hpp"

#include <algorithm>

#include "Engine/Core/EngineCommon.hpp"
#include "Chunk.hpp"
#include "ChunkMesh.hpp"

namespace
{
    using enigma::graphic::TerrainVertex;
    using enigma::voxel::Chunk;
    using enigma::voxel::ChunkBatchChunkBuildOutput;
    using enigma::voxel::ChunkBatchChunkLayerSlice;
    using enigma::voxel::ChunkBatchRegionBuildInput;
    using enigma::voxel::ChunkBatchRegionBuildOutput;
    using enigma::voxel::ChunkBatchRegionId;
    using enigma::voxel::ChunkBatchSubDraw;

    constexpr uint32_t kVertexReserveAlignment = 8u;
    constexpr uint32_t kIndexReserveAlignment  = 3u;

    uint32_t AlignUp(uint32_t value, uint32_t alignment)
    {
        if (value == 0u || alignment == 0u)
        {
            return value;
        }

        const uint32_t remainder = value % alignment;
        return (remainder == 0u) ? value : (value + alignment - remainder);
    }

    uint32_t ComputeReservedVertexCount(uint32_t actualCount)
    {
        if (actualCount == 0u)
        {
            return 0u;
        }

        const uint32_t slack = (std::max)(actualCount / 8u, 8u);
        return AlignUp(actualCount + slack, kVertexReserveAlignment);
    }

    uint32_t ComputeReservedIndexCount(uint32_t actualCount)
    {
        if (actualCount == 0u)
        {
            return 0u;
        }

        const uint32_t slack = (std::max)(actualCount / 8u, 3u);
        return AlignUp(actualCount + slack, kIndexReserveAlignment);
    }

    TerrainVertex MakeZeroTerrainVertex()
    {
        return TerrainVertex{
            Vec3(),
            Rgba8(),
            Vec2(),
            Vec3(),
            Vec2(),
            0u,
            0u,
            Vec2()
        };
    }

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

    bool AppendLayerGeometry(
        ChunkBatchChunkBuildOutput& output,
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

        std::vector<uint32_t>& targetIndices = output.GetIndicesForLayer(layer);
        targetIndices.reserve(targetIndices.size() + sourceIndices.size());
        for (uint32_t sourceIndex : sourceIndices)
        {
            if (sourceIndex >= sourceVertices.size())
            {
                ERROR_AND_DIE("ChunkBatchRegionBuilder encountered invalid layer geometry indices");
            }

            targetIndices.push_back(baseVertex + sourceIndex);
        }

        return true;
    }

    ChunkBatchChunkBuildOutput BuildChunkGeometryInternal(const Chunk& chunk, const ChunkBatchRegionId& regionId)
    {
        ChunkBatchChunkBuildOutput output;
        output.chunkCoords = chunk.GetChunkCoords();

        const enigma::voxel::ChunkMesh* mesh = chunk.GetChunkMesh();
        if (mesh == nullptr)
        {
            return output;
        }

        const Vec3 translation = GetChunkRegionLocalTranslation(chunk, regionId);
        AppendLayerGeometry(output, *mesh, enigma::voxel::ChunkBatchLayer::Opaque, translation);
        AppendLayerGeometry(output, *mesh, enigma::voxel::ChunkBatchLayer::Cutout, translation);
        AppendLayerGeometry(output, *mesh, enigma::voxel::ChunkBatchLayer::Translucent, translation);

        if (output.HasGeometry())
        {
            output.worldBounds = BuildChunkWorldBounds(chunk);
            output.hasWorldBounds = true;
        }

        return output;
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

    uint32_t AssignVertexAllocations(std::vector<ChunkBatchChunkBuildOutput>& chunkOutputs)
    {
        uint32_t vertexCursor = 0u;
        for (ChunkBatchChunkBuildOutput& chunkOutput : chunkOutputs)
        {
            const uint32_t reservedVertexCount = ComputeReservedVertexCount(static_cast<uint32_t>(chunkOutput.vertices.size()));
            chunkOutput.vertexAllocation.startElement = vertexCursor;
            chunkOutput.vertexAllocation.elementCount = reservedVertexCount;
            vertexCursor += reservedVertexCount;
        }

        return vertexCursor;
    }

    uint32_t AssignLayerSlices(
        std::vector<ChunkBatchChunkBuildOutput>& chunkOutputs,
        enigma::voxel::ChunkBatchLayer           layer,
        uint32_t                                 startIndex)
    {
        uint32_t indexCursor = startIndex;
        for (ChunkBatchChunkBuildOutput& chunkOutput : chunkOutputs)
        {
            ChunkBatchChunkLayerSlice& chunkLayerSlice = chunkOutput.GetLayerSlice(layer);
            const uint32_t actualIndexCount = static_cast<uint32_t>(chunkOutput.GetIndicesForLayer(layer).size());
            const uint32_t reservedIndexCount = ComputeReservedIndexCount(actualIndexCount);

            chunkLayerSlice.startIndex = indexCursor;
            chunkLayerSlice.indexCount = actualIndexCount;
            chunkLayerSlice.reservedIndexCount = reservedIndexCount;
            indexCursor += reservedIndexCount;
        }

        return indexCursor;
    }

    enigma::voxel::ChunkBatchLayerSpan BuildRegionLayerSpan(
        const std::vector<ChunkBatchChunkBuildOutput>& chunkOutputs,
        enigma::voxel::ChunkBatchLayer                 layer)
    {
        enigma::voxel::ChunkBatchLayerSpan span;
        bool hasActiveSlice = false;
        uint32_t lastDrawEnd = 0u;

        for (const ChunkBatchChunkBuildOutput& chunkOutput : chunkOutputs)
        {
            const ChunkBatchChunkLayerSlice& chunkLayerSlice = chunkOutput.GetLayerSlice(layer);
            if (!chunkLayerSlice.HasGeometry())
            {
                continue;
            }

            if (!hasActiveSlice)
            {
                span.startIndex = chunkLayerSlice.startIndex;
                hasActiveSlice = true;
            }

            lastDrawEnd = chunkLayerSlice.GetDrawEndIndex();
        }

        if (hasActiveSlice)
        {
            span.indexCount = lastDrawEnd - span.startIndex;
        }

        return span;
    }

    std::vector<ChunkBatchSubDraw> BuildRegionSubDraws(
        const std::vector<ChunkBatchChunkBuildOutput>& chunkOutputs,
        enigma::voxel::ChunkBatchLayer                 layer)
    {
        std::vector<ChunkBatchSubDraw> subDraws;
        subDraws.reserve(chunkOutputs.size());

        for (const ChunkBatchChunkBuildOutput& chunkOutput : chunkOutputs)
        {
            const ChunkBatchChunkLayerSlice& chunkLayerSlice = chunkOutput.GetLayerSlice(layer);
            if (!chunkLayerSlice.HasGeometry())
            {
                continue;
            }

            subDraws.push_back(ChunkBatchSubDraw{
                chunkLayerSlice.startIndex,
                chunkLayerSlice.indexCount
            });
        }

        return subDraws;
    }

    void BuildRegionCpuBuffers(ChunkBatchRegionBuildOutput& output)
    {
        if (!output.HasCpuGeometry())
        {
            return;
        }

        output.vertices.assign(output.totalVertexCapacity, MakeZeroTerrainVertex());
        output.indices.assign(output.totalIndexCapacity, 0u);

        for (const ChunkBatchChunkBuildOutput& chunkOutput : output.chunkOutputs)
        {
            if (!chunkOutput.HasGeometry())
            {
                continue;
            }

            for (size_t vertexIndex = 0; vertexIndex < chunkOutput.vertices.size(); ++vertexIndex)
            {
                output.vertices[chunkOutput.vertexAllocation.startElement + static_cast<uint32_t>(vertexIndex)] = chunkOutput.vertices[vertexIndex];
            }

            const uint32_t degenerateIndex = chunkOutput.vertexAllocation.startElement;
            const enigma::voxel::ChunkBatchLayer layers[] = {
                enigma::voxel::ChunkBatchLayer::Opaque,
                enigma::voxel::ChunkBatchLayer::Cutout,
                enigma::voxel::ChunkBatchLayer::Translucent
            };

            for (enigma::voxel::ChunkBatchLayer layer : layers)
            {
                const ChunkBatchChunkLayerSlice& chunkLayerSlice = chunkOutput.GetLayerSlice(layer);
                if (!chunkLayerSlice.HasReservedCapacity())
                {
                    continue;
                }

                const uint32_t reservedEnd = chunkLayerSlice.GetReservedEndIndex();
                for (uint32_t index = chunkLayerSlice.startIndex; index < reservedEnd; ++index)
                {
                    output.indices[index] = degenerateIndex;
                }

                const std::vector<uint32_t>& sourceIndices = chunkOutput.GetIndicesForLayer(layer);
                for (uint32_t indexOffset = 0; indexOffset < static_cast<uint32_t>(sourceIndices.size()); ++indexOffset)
                {
                    const uint32_t sourceIndex = sourceIndices[indexOffset];
                    if (sourceIndex >= chunkOutput.vertices.size())
                    {
                        ERROR_AND_DIE("ChunkBatchRegionBuilder encountered invalid chunk-local indices");
                    }

                    output.indices[chunkLayerSlice.startIndex + indexOffset] = chunkOutput.vertexAllocation.startElement + sourceIndex;
                }
            }
        }
    }
}

namespace enigma::voxel
{
    ChunkBatchChunkBuildOutput ChunkBatchRegionBuilder::BuildChunkGeometry(const Chunk& chunk, const ChunkBatchRegionId& regionId)
    {
        if (!IsChunkEligibleForBuild(&chunk, regionId))
        {
            return {};
        }

        return BuildChunkGeometryInternal(chunk, regionId);
    }

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

        output.chunkOutputs.reserve(chunks.size());
        bool hasWorldBounds = false;

        for (const Chunk* chunk : chunks)
        {
            if (chunk == nullptr)
            {
                continue;
            }

            ChunkBatchChunkBuildOutput chunkOutput = BuildChunkGeometryInternal(*chunk, input.regionId);
            if (!chunkOutput.HasGeometry())
            {
                continue;
            }

            if (!hasWorldBounds)
            {
                output.geometry.worldBounds = chunkOutput.worldBounds;
                hasWorldBounds = true;
            }
            else
            {
                output.geometry.worldBounds.StretchToIncludePoint(chunkOutput.worldBounds.m_mins);
                output.geometry.worldBounds.StretchToIncludePoint(chunkOutput.worldBounds.m_maxs);
            }

            output.chunkOutputs.push_back(std::move(chunkOutput));
        }

        if (output.chunkOutputs.empty())
        {
            return output;
        }

        output.totalVertexCapacity = AssignVertexAllocations(output.chunkOutputs);
        uint32_t indexCursor = 0u;
        indexCursor = AssignLayerSlices(output.chunkOutputs, ChunkBatchLayer::Opaque, indexCursor);
        indexCursor = AssignLayerSlices(output.chunkOutputs, ChunkBatchLayer::Cutout, indexCursor);
        indexCursor = AssignLayerSlices(output.chunkOutputs, ChunkBatchLayer::Translucent, indexCursor);
        output.totalIndexCapacity = indexCursor;

        output.geometry.opaque = BuildRegionLayerSpan(output.chunkOutputs, ChunkBatchLayer::Opaque);
        output.geometry.cutout = BuildRegionLayerSpan(output.chunkOutputs, ChunkBatchLayer::Cutout);
        output.geometry.translucent = BuildRegionLayerSpan(output.chunkOutputs, ChunkBatchLayer::Translucent);
        output.geometry.opaqueSubDraws = BuildRegionSubDraws(output.chunkOutputs, ChunkBatchLayer::Opaque);
        output.geometry.cutoutSubDraws = BuildRegionSubDraws(output.chunkOutputs, ChunkBatchLayer::Cutout);
        output.geometry.translucentSubDraws = BuildRegionSubDraws(output.chunkOutputs, ChunkBatchLayer::Translucent);

        if (hasWorldBounds)
        {
            BuildRegionCpuBuffers(output);
        }

        return output;
    }
}
