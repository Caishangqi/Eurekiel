#include "ChunkRenderRegionStorage.hpp"

#include <algorithm>

#include "Chunk.hpp"
#include "ChunkHelper.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "../World/World.hpp"

namespace
{
    using enigma::graphic::D12IndexBuffer;
    using enigma::graphic::D12VertexBuffer;
    using enigma::voxel::Chunk;
    using enigma::voxel::ChunkBatchRegionBuildOutput;
    using enigma::voxel::ChunkBatchRegionBuilder;
    using enigma::voxel::ChunkBatchRegionId;
    using enigma::voxel::ChunkRenderRegion;
    using enigma::voxel::ChunkRenderRegionStorage;
    using enigma::voxel::World;

    int64_t GetChunkKey(const IntVec2& chunkCoords)
    {
        return enigma::voxel::ChunkHelper::PackCoordinates(chunkCoords.x, chunkCoords.y);
    }

    int64_t GetChunkKey(const Chunk& chunk)
    {
        return GetChunkKey(chunk.GetChunkCoords());
    }
}

namespace enigma::voxel
{
    ChunkRenderRegionStorage::ChunkRenderRegionStorage(World* world)
        : m_world(world)
    {
    }

    void ChunkRenderRegionStorage::SetWorld(World* world)
    {
        m_world = world;
    }

    ChunkRenderRegion& ChunkRenderRegionStorage::EnsureRegion(const ChunkBatchRegionId& regionId)
    {
        auto [it, inserted] = m_regions.try_emplace(regionId);
        if (inserted)
        {
            it->second.id = regionId;
        }

        return it->second;
    }

    void ChunkRenderRegionStorage::EnqueueDirtyRegion(const ChunkBatchRegionId& regionId)
    {
        if (m_dirtyRegionSet.insert(regionId).second)
        {
            m_dirtyRegionQueue.push_back(regionId);
        }

        auto regionIt = m_regions.find(regionId);
        if (regionIt != m_regions.end())
        {
            regionIt->second.dirty = true;
        }
    }

    void ChunkRenderRegionStorage::RemoveQueuedDirtyRegion(const ChunkBatchRegionId& regionId)
    {
        if (m_dirtyRegionSet.erase(regionId) == 0)
        {
            return;
        }

        const auto queuedIt = std::find(m_dirtyRegionQueue.begin(), m_dirtyRegionQueue.end(), regionId);
        if (queuedIt != m_dirtyRegionQueue.end())
        {
            m_dirtyRegionQueue.erase(queuedIt);
        }
    }

    void ChunkRenderRegionStorage::ClearRegionGeometry(ChunkRenderRegion& region, bool buildFailed)
    {
        region.geometry.vertexBuffer.reset();
        region.geometry.indexBuffer.reset();
        region.geometry.opaque = {};
        region.geometry.cutout = {};
        region.geometry.translucent = {};
        region.geometry.gpuDataValid = false;
        region.buildFailed = buildFailed;
    }

    bool ChunkRenderRegionStorage::CommitBuildOutput(ChunkRenderRegion& region, const ChunkBatchRegionBuildOutput& buildOutput)
    {
        region.geometry.regionModelMatrix = buildOutput.geometry.regionModelMatrix;
        region.geometry.worldBounds = buildOutput.geometry.worldBounds;
        region.geometry.opaque = buildOutput.geometry.opaque;
        region.geometry.cutout = buildOutput.geometry.cutout;
        region.geometry.translucent = buildOutput.geometry.translucent;

        if (!buildOutput.HasCpuGeometry())
        {
            ClearRegionGeometry(region, false);
            return true;
        }

        const size_t vertexBufferSize = sizeof(graphic::TerrainVertex) * buildOutput.vertices.size();
        const size_t indexBufferSize  = sizeof(uint32_t) * buildOutput.indices.size();

        std::unique_ptr<D12VertexBuffer> vertexBuffer = graphic::D3D12RenderSystem::CreateVertexBuffer(
            vertexBufferSize,
            sizeof(graphic::TerrainVertex),
            buildOutput.vertices.data(),
            "ChunkRegionVertexBuffer");
        std::unique_ptr<D12IndexBuffer> indexBuffer = graphic::D3D12RenderSystem::CreateIndexBuffer(
            indexBufferSize,
            buildOutput.indices.data(),
            "ChunkRegionIndexBuffer");

        if (!vertexBuffer || !indexBuffer)
        {
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Failed to create GPU buffers for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        region.geometry.vertexBuffer = std::shared_ptr<D12VertexBuffer>(std::move(vertexBuffer));
        region.geometry.indexBuffer = std::shared_ptr<D12IndexBuffer>(std::move(indexBuffer));
        region.geometry.gpuDataValid = true;
        region.buildFailed = false;
        return true;
    }

    std::vector<const Chunk*> ChunkRenderRegionStorage::GatherRegionChunks(ChunkRenderRegion& region)
    {
        std::vector<const Chunk*> chunks;
        if (m_world == nullptr)
        {
            return chunks;
        }

        std::vector<int64_t> staleChunkKeys;
        chunks.reserve(region.chunkKeys.size());

        for (int64_t chunkKey : region.chunkKeys)
        {
            int32_t chunkX = 0;
            int32_t chunkY = 0;
            ChunkHelper::UnpackCoordinates(chunkKey, chunkX, chunkY);

            Chunk* chunk = m_world->GetChunk(chunkX, chunkY);
            if (chunk == nullptr)
            {
                staleChunkKeys.push_back(chunkKey);
                continue;
            }

            const ChunkBatchRegionId expectedRegionId = GetChunkBatchRegionIdForChunk(chunk->GetChunkCoords());
            if (expectedRegionId != region.id)
            {
                staleChunkKeys.push_back(chunkKey);
                continue;
            }

            chunks.push_back(chunk);
        }

        for (int64_t staleChunkKey : staleChunkKeys)
        {
            region.chunkKeys.erase(staleChunkKey);
            m_chunkToRegion.erase(staleChunkKey);
        }

        return chunks;
    }

    void ChunkRenderRegionStorage::MarkChunkDirty(const IntVec2& chunkCoords)
    {
        const ChunkBatchRegionId regionId = GetChunkBatchRegionIdForChunk(chunkCoords);
        ChunkRenderRegion&       region   = EnsureRegion(regionId);
        const int64_t            chunkKey = GetChunkKey(chunkCoords);

        region.chunkKeys.insert(chunkKey);
        m_chunkToRegion[chunkKey] = regionId;
        EnqueueDirtyRegion(regionId);
    }

    void ChunkRenderRegionStorage::NotifyChunkMeshReady(Chunk* chunk)
    {
        if (chunk == nullptr)
        {
            return;
        }

        const ChunkBatchRegionId regionId = GetChunkBatchRegionIdForChunk(chunk->GetChunkCoords());
        ChunkRenderRegion&       region   = EnsureRegion(regionId);
        const int64_t            chunkKey = GetChunkKey(*chunk);

        region.chunkKeys.insert(chunkKey);
        m_chunkToRegion[chunkKey] = regionId;
        EnqueueDirtyRegion(regionId);
    }

    void ChunkRenderRegionStorage::NotifyChunkUnloaded(const IntVec2& chunkCoords)
    {
        const int64_t chunkKey = GetChunkKey(chunkCoords);
        auto          mappingIt = m_chunkToRegion.find(chunkKey);
        if (mappingIt == m_chunkToRegion.end())
        {
            return;
        }

        const ChunkBatchRegionId regionId = mappingIt->second;
        m_chunkToRegion.erase(mappingIt);

        auto regionIt = m_regions.find(regionId);
        if (regionIt == m_regions.end())
        {
            return;
        }

        ChunkRenderRegion& region = regionIt->second;
        region.chunkKeys.erase(chunkKey);

        if (!region.HasResidentChunks())
        {
            RemoveQueuedDirtyRegion(regionId);
            m_regions.erase(regionIt);
            return;
        }

        EnqueueDirtyRegion(regionId);
    }

    uint32_t ChunkRenderRegionStorage::RebuildDirtyRegions(uint32_t maxRegionsPerFrame)
    {
        if (maxRegionsPerFrame == 0)
        {
            return 0;
        }

        uint32_t rebuiltRegionCount = 0;
        while (rebuiltRegionCount < maxRegionsPerFrame && !m_dirtyRegionQueue.empty())
        {
            const ChunkBatchRegionId regionId = m_dirtyRegionQueue.front();
            m_dirtyRegionQueue.pop_front();
            m_dirtyRegionSet.erase(regionId);
            if (RebuildRegionInternal(regionId))
            {
                rebuiltRegionCount++;
            }
        }

        return rebuiltRegionCount;
    }

    uint32_t ChunkRenderRegionStorage::RebuildRegionsNow(const std::vector<ChunkBatchRegionId>& regionIds)
    {
        uint32_t rebuiltRegionCount = 0;

        for (const ChunkBatchRegionId& regionId : regionIds)
        {
            RemoveQueuedDirtyRegion(regionId);
            if (RebuildRegionInternal(regionId))
            {
                rebuiltRegionCount++;
            }
        }

        return rebuiltRegionCount;
    }

    bool ChunkRenderRegionStorage::RebuildRegionInternal(const ChunkBatchRegionId& regionId)
    {
        auto regionIt = m_regions.find(regionId);
        if (regionIt == m_regions.end())
        {
            return false;
        }

        ChunkRenderRegion& region = regionIt->second;
        region.dirty = false;

        std::vector<const Chunk*> sourceChunks = GatherRegionChunks(region);
        if (!region.HasResidentChunks())
        {
            m_regions.erase(regionIt);
            return true;
        }

        const ChunkBatchRegionBuildOutput buildOutput = ChunkBatchRegionBuilder::BuildRegionGeometry(
            ChunkBatchRegionBuildInput{regionId, sourceChunks});
        const bool commitSucceeded = CommitBuildOutput(region, buildOutput);

        if (commitSucceeded && m_world != nullptr)
        {
            for (const Chunk* sourceChunk : sourceChunks)
            {
                Chunk* mutableChunk = const_cast<Chunk*>(sourceChunk);
                if (mutableChunk == nullptr || !mutableChunk->IsActive())
                {
                    continue;
                }

                ChunkMesh* chunkMesh = mutableChunk->GetChunkMesh();
                if (chunkMesh == nullptr)
                {
                    continue;
                }

                chunkMesh->ReleaseGpuBuffers(true, true, true);
            }
        }

        return true;
    }

    ChunkRenderRegion* ChunkRenderRegionStorage::GetRegion(const ChunkBatchRegionId& id)
    {
        const auto it = m_regions.find(id);
        if (it == m_regions.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    const ChunkRenderRegion* ChunkRenderRegionStorage::GetRegion(const ChunkBatchRegionId& id) const
    {
        const auto it = m_regions.find(id);
        if (it == m_regions.end())
        {
            return nullptr;
        }

        return &it->second;
    }
}
