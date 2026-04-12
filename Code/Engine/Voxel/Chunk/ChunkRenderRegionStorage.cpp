#include "ChunkRenderRegionStorage.hpp"

#include <algorithm>
#include <cstring>

#include "ChunkBatchArenaRelocation.hpp"
#include "ChunkBatchRegionBuilder.hpp"
#include "Chunk.hpp"
#include "ChunkHelper.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferTransferCoordinator.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "../World/World.hpp"

namespace
{
    using enigma::graphic::BufferCopySpan;
    using enigma::graphic::BufferCreateInfo;
    using enigma::graphic::BufferTransferBatch;
    using enigma::graphic::BufferTransferCoordinator;
    using enigma::graphic::BufferTransferTransition;
    using enigma::graphic::BufferUploadSpan;
    using enigma::graphic::BufferUsage;
    using enigma::graphic::BufferUsagePolicy;
    using enigma::graphic::CommandQueueType;
    using enigma::graphic::D12IndexBuffer;
    using enigma::graphic::D12Buffer;
    using enigma::graphic::D12VertexBuffer;
    using enigma::graphic::MemoryAccess;
    using enigma::graphic::QueueSubmissionToken;
    using enigma::graphic::TerrainVertex;
    using enigma::voxel::Chunk;
    using enigma::voxel::ChunkBatchArenaAllocation;
    using enigma::voxel::ChunkBatchArenaDiagnostics;
    using enigma::voxel::ChunkBatchArenaFallbackDiagnostics;
    using enigma::voxel::ChunkBatchArenaFallbackReason;
    using enigma::voxel::ChunkBatchArenaKind;
    using enigma::voxel::ChunkBatchArenaRelocationPlan;
    using enigma::voxel::ChunkBatchArenaRelocationPlanner;
    using enigma::voxel::ChunkBatchArenaSideDiagnostics;
    using enigma::voxel::ChunkBatchChunkBuildOutput;
    using enigma::voxel::ChunkBatchChunkLayerSlice;
    using enigma::voxel::ChunkBatchChunkRuntimeSlice;
    using enigma::voxel::ChunkBatchIndexArenaState;
    using enigma::voxel::ChunkBatchLayer;
    using enigma::voxel::ChunkBatchLayerSpan;
    using enigma::voxel::ChunkBatchRegionBuildInput;
    using enigma::voxel::ChunkBatchRegionBuildOutput;
    using enigma::voxel::ChunkBatchRegionBuilder;
    using enigma::voxel::ChunkBatchRegionId;
    using enigma::voxel::ChunkBatchSubDraw;
    using enigma::voxel::ChunkBatchVertexArenaState;
    using enigma::voxel::ChunkRenderRegion;
    using enigma::voxel::ChunkRenderRegionStorage;
    using enigma::voxel::RetiredArenaAllocation;
    using enigma::voxel::World;

    constexpr uint32_t kDefaultVertexArenaCapacity = 4096;
    constexpr uint32_t kDefaultIndexArenaCapacity  = 16384;

    struct ChunkGeometryTransferPlan
    {
        BufferTransferBatch uploadBatch;

        bool HasUploadWork() const noexcept
        {
            return uploadBatch.HasUploadWork();
        }
    };

    struct PendingChunkReplacement
    {
        int64_t                       chunkKey = 0;
        ChunkBatchChunkRuntimeSlice*  runtimeSlice = nullptr;
        ChunkBatchChunkBuildOutput    buildOutput;
        bool                          clearSlice = false;
    };

    int64_t GetChunkKey(const IntVec2& chunkCoords)
    {
        return enigma::voxel::ChunkHelper::PackCoordinates(chunkCoords.x, chunkCoords.y);
    }

    int64_t GetChunkKey(const Chunk& chunk)
    {
        return GetChunkKey(chunk.GetChunkCoords());
    }

    Vec3 GetRegionOriginWorldPosition(const ChunkBatchRegionId& regionId)
    {
        const IntVec2 minChunkCoords = enigma::voxel::GetChunkBatchRegionMinChunkCoords(regionId);

        return Vec3(
            static_cast<float>(minChunkCoords.x * Chunk::CHUNK_SIZE_X),
            static_cast<float>(minChunkCoords.y * Chunk::CHUNK_SIZE_Y),
            0.0f);
    }

    AABB3 BuildFallbackRegionBounds(const ChunkBatchRegionId& regionId)
    {
        const Vec3 regionOriginWorld = GetRegionOriginWorldPosition(regionId);
        return AABB3(regionOriginWorld, regionOriginWorld);
    }

    uint32_t RoundUpToPowerOfTwo(uint32_t value)
    {
        if (value <= 1u)
        {
            return 1u;
        }

        uint32_t rounded = 1u;
        while (rounded < value)
        {
            rounded <<= 1u;
        }

        return rounded;
    }

    uint32_t ComputeExpandedCapacity(uint32_t currentCapacity, uint32_t requestedCount, uint32_t defaultCapacity)
    {
        if (currentCapacity == 0u)
        {
            return RoundUpToPowerOfTwo((std::max)(defaultCapacity, requestedCount));
        }

        uint64_t newCapacity = currentCapacity;
        while ((newCapacity - currentCapacity) < requestedCount)
        {
            newCapacity *= 2u;
        }

        return static_cast<uint32_t>(newCapacity);
    }

    bool HasContiguousFreeRange(
        const std::vector<ChunkBatchArenaAllocation>& freeRanges,
        uint32_t                                      requestedCount)
    {
        if (requestedCount == 0u)
        {
            return true;
        }

        for (const ChunkBatchArenaAllocation& freeRange : freeRanges)
        {
            if (freeRange.elementCount >= requestedCount)
            {
                return true;
            }
        }

        return false;
    }

    bool TryAllocateRange(
        std::vector<ChunkBatchArenaAllocation>& freeRanges,
        uint32_t                                requestedCount,
        ChunkBatchArenaAllocation&              outAllocation)
    {
        outAllocation.Reset();
        if (requestedCount == 0u)
        {
            return true;
        }

        for (size_t rangeIndex = 0; rangeIndex < freeRanges.size(); ++rangeIndex)
        {
            ChunkBatchArenaAllocation& freeRange = freeRanges[rangeIndex];
            if (freeRange.elementCount < requestedCount)
            {
                continue;
            }

            outAllocation.startElement = freeRange.startElement;
            outAllocation.elementCount = requestedCount;
            freeRange.startElement += requestedCount;
            freeRange.elementCount -= requestedCount;
            if (!freeRange.IsValid())
            {
                freeRanges.erase(freeRanges.begin() + static_cast<ptrdiff_t>(rangeIndex));
            }

            return true;
        }

        return false;
    }

    void FreeRange(
        std::vector<ChunkBatchArenaAllocation>& freeRanges,
        const ChunkBatchArenaAllocation&        allocation)
    {
        if (!allocation.IsValid())
        {
            return;
        }

        freeRanges.push_back(allocation);
        std::sort(
            freeRanges.begin(),
            freeRanges.end(),
            [](const ChunkBatchArenaAllocation& lhs, const ChunkBatchArenaAllocation& rhs)
            {
                return lhs.startElement < rhs.startElement;
            });

        std::vector<ChunkBatchArenaAllocation> mergedRanges;
        mergedRanges.reserve(freeRanges.size());

        for (const ChunkBatchArenaAllocation& freeRange : freeRanges)
        {
            if (!freeRange.IsValid())
            {
                continue;
            }

            if (mergedRanges.empty())
            {
                mergedRanges.push_back(freeRange);
                continue;
            }

            ChunkBatchArenaAllocation& mergedRange = mergedRanges.back();
            const uint32_t mergedRangeEnd = mergedRange.startElement + mergedRange.elementCount;
            if (mergedRangeEnd >= freeRange.startElement)
            {
                const uint32_t freeRangeEnd = freeRange.startElement + freeRange.elementCount;
                mergedRange.elementCount = (std::max)(mergedRangeEnd, freeRangeEnd) - mergedRange.startElement;
                continue;
            }

            mergedRanges.push_back(freeRange);
        }

        freeRanges = std::move(mergedRanges);
    }

    void DrainRetiredRanges(
        std::vector<ChunkBatchArenaAllocation>& freeRanges,
        std::vector<RetiredArenaAllocation>&    retiredRanges,
        const enigma::graphic::QueueFenceSnapshot& completedSnapshot)
    {
        auto retiredIt = retiredRanges.begin();
        while (retiredIt != retiredRanges.end())
        {
            if (!retiredIt->IsReadyForReuse(completedSnapshot))
            {
                ++retiredIt;
                continue;
            }

            FreeRange(freeRanges, retiredIt->allocation);
            retiredIt = retiredRanges.erase(retiredIt);
        }
    }

    uint32_t SumFreeRangeCapacity(const std::vector<ChunkBatchArenaAllocation>& freeRanges)
    {
        uint32_t totalFreeCapacity = 0u;
        for (const ChunkBatchArenaAllocation& allocation : freeRanges)
        {
            totalFreeCapacity += allocation.elementCount;
        }

        return totalFreeCapacity;
    }

    uint64_t ComputeByteOffset(const ChunkBatchArenaAllocation& allocation, size_t bytesPerElement)
    {
        return static_cast<uint64_t>(allocation.startElement) * static_cast<uint64_t>(bytesPerElement);
    }

    uint64_t ComputeByteSize(uint32_t elementCount, size_t bytesPerElement)
    {
        return static_cast<uint64_t>(elementCount) * static_cast<uint64_t>(bytesPerElement);
    }

    BufferCreateInfo MakeChunkVertexArenaCreateInfo(uint32_t capacityVertices)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = sizeof(TerrainVertex) * static_cast<size_t>(capacityVertices);
        createInfo.usage        = BufferUsage::VertexBuffer;
        createInfo.memoryAccess = MemoryAccess::GPUOnly;
        createInfo.initialData  = nullptr;
        createInfo.debugName    = "ChunkBatchVertexArena";
        createInfo.byteStride   = sizeof(TerrainVertex);
        createInfo.usagePolicy  = BufferUsagePolicy::GpuOnlyCopyReady;
        return createInfo;
    }

    BufferCreateInfo MakeChunkIndexArenaCreateInfo(uint32_t capacityIndices)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = sizeof(uint32_t) * static_cast<size_t>(capacityIndices);
        createInfo.usage        = BufferUsage::IndexBuffer;
        createInfo.memoryAccess = MemoryAccess::GPUOnly;
        createInfo.initialData  = nullptr;
        createInfo.debugName    = "ChunkBatchIndexArena";
        createInfo.byteStride   = sizeof(uint32_t);
        createInfo.usagePolicy  = BufferUsagePolicy::GpuOnlyCopyReady;
        return createInfo;
    }

    bool HasTransitionForBuffer(
        const std::vector<BufferTransferTransition>& transitions,
        const D12Buffer*                             buffer)
    {
        return std::any_of(
            transitions.begin(),
            transitions.end(),
            [buffer](const BufferTransferTransition& transition)
            {
                return transition.buffer == buffer;
            });
    }

    void AppendTransitionIfMissing(
        std::vector<BufferTransferTransition>& transitions,
        D12Buffer*                             buffer,
        D3D12_RESOURCE_STATES                  transferState,
        D3D12_RESOURCE_STATES                  finalState,
        const char*                            debugName)
    {
        if (buffer == nullptr || HasTransitionForBuffer(transitions, buffer))
        {
            return;
        }

        transitions.push_back(BufferTransferTransition{
            buffer,
            transferState,
            finalState,
            debugName
        });
    }

    bool ExecuteUploadBatchAndWait(const BufferTransferBatch& batch)
    {
        if (!batch.HasUploadWork())
        {
            return true;
        }

        BufferTransferCoordinator coordinator;
        auto execution = coordinator.ExecuteUploadBatch(batch);
        if (!execution.IsValid())
        {
            return false;
        }

        return coordinator.WaitForTransferCompletion(execution);
    }

    bool ExecuteCopyBatchAndWait(const BufferTransferBatch& batch)
    {
        if (!batch.HasCopyWork())
        {
            return true;
        }

        BufferTransferCoordinator coordinator;
        auto execution = coordinator.ExecuteCopyBatch(batch);
        if (!execution.IsValid())
        {
            return false;
        }

        return coordinator.WaitForTransferCompletion(execution);
    }

    bool IsGraphicsSubmissionComplete(
        const QueueSubmissionToken& token,
        const enigma::graphic::QueueFenceSnapshot& completedSnapshot)
    {
        if (!token.IsValid())
        {
            return true;
        }

        return completedSnapshot.GetCompletedFenceValue(token.queueType) >= token.fenceValue;
    }

    bool WaitForLatestGraphicsWorkCompletion()
    {
        auto* commandListManager = enigma::graphic::D3D12RenderSystem::GetCommandListManager();
        if (commandListManager == nullptr)
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Command list manager is unavailable while waiting for graphics work");
            return false;
        }

        commandListManager->UpdateCompletedCommandLists();
        const enigma::graphic::QueueSubmittedFenceSnapshot submittedSnapshot =
            commandListManager->GetLastSubmittedFenceSnapshot();
        const QueueSubmissionToken latestGraphicsToken = {
            CommandQueueType::Graphics,
            submittedSnapshot.graphicsSubmitted
        };
        if (!latestGraphicsToken.IsValid())
        {
            return true;
        }

        const enigma::graphic::QueueFenceSnapshot completedSnapshot =
            commandListManager->GetCompletedFenceSnapshot();
        if (IsGraphicsSubmissionComplete(latestGraphicsToken, completedSnapshot))
        {
            return true;
        }

        if (!commandListManager->WaitForSubmission(latestGraphicsToken))
        {
            ERROR_RECOVERABLE(Stringf(
                "ChunkRenderRegionStorage: Failed to retire graphics queue fence %llu before arena mutation",
                latestGraphicsToken.fenceValue));
            return false;
        }

        commandListManager->UpdateCompletedCommandLists();
        return true;
    }

    template <typename TBuffer>
    bool ValidateUploadRange(
        const std::shared_ptr<TBuffer>& arenaBuffer,
        const ChunkBatchArenaAllocation& allocation,
        size_t                           bytesPerElement,
        size_t                           sourceElementCount)
    {
        if (arenaBuffer == nullptr || !allocation.IsValid())
        {
            return false;
        }

        const size_t writeOffset = static_cast<size_t>(allocation.startElement) * bytesPerElement;
        const size_t writeSize   = sourceElementCount * bytesPerElement;
        return writeOffset + writeSize <= arenaBuffer->GetSize();
    }

    bool IsChunkEligibleForReplacement(const Chunk* chunk, const ChunkBatchRegionId& regionId)
    {
        if (chunk == nullptr || !chunk->IsActive())
        {
            return false;
        }

        if (enigma::voxel::GetChunkBatchRegionIdForChunk(chunk->GetChunkCoords()) != regionId)
        {
            return false;
        }

        const enigma::voxel::ChunkMesh* chunkMesh = chunk->GetChunkMesh();
        return chunkMesh != nullptr && !chunkMesh->IsEmpty();
    }

    bool IsReplacementUploadFallbackReason(ChunkBatchArenaFallbackReason reason)
    {
        switch (reason)
        {
        case ChunkBatchArenaFallbackReason::ReplacementStateInvalid:
        case ChunkBatchArenaFallbackReason::ReplacementVertexOverflow:
        case ChunkBatchArenaFallbackReason::ReplacementIndexOverflow:
        case ChunkBatchArenaFallbackReason::ReplacementVertexUploadFailed:
        case ChunkBatchArenaFallbackReason::ReplacementIndexUploadFailed:
            return true;
        default:
            return false;
        }
    }

    ChunkBatchArenaAllocation MakeAbsoluteAllocation(
        const ChunkBatchArenaAllocation& regionAllocation,
        uint32_t                         relativeStart,
        uint32_t                         elementCount)
    {
        return ChunkBatchArenaAllocation{
            regionAllocation.startElement + relativeStart,
            elementCount
        };
    }

    std::vector<ChunkBatchSubDraw> BuildRegionSubDrawsFromChunkSlices(
        const std::unordered_map<int64_t, ChunkBatchChunkRuntimeSlice>& chunkSlices,
        ChunkBatchLayer                                                 layer)
    {
        std::vector<ChunkBatchSubDraw> subDraws;
        subDraws.reserve(chunkSlices.size());

        for (const auto& [chunkKey, chunkSlice] : chunkSlices)
        {
            UNUSED(chunkKey);

            const ChunkBatchChunkLayerSlice& layerSlice = chunkSlice.GetLayerSlice(layer);
            if (!layerSlice.HasGeometry())
            {
                continue;
            }

            subDraws.push_back(ChunkBatchSubDraw{
                layerSlice.startIndex,
                layerSlice.indexCount
            });
        }

        std::sort(
            subDraws.begin(),
            subDraws.end(),
            [](const ChunkBatchSubDraw& lhs, const ChunkBatchSubDraw& rhs)
            {
                return lhs.startIndex < rhs.startIndex;
            });

        return subDraws;
    }

    ChunkBatchLayerSpan BuildRegionSpanFromSubDraws(
        const std::vector<ChunkBatchSubDraw>& subDraws)
    {
        ChunkBatchLayerSpan span;
        if (subDraws.empty())
        {
            return span;
        }

        span.startIndex = subDraws.front().startIndex;
        uint32_t lastDrawEnd = subDraws.front().GetEndIndex();
        for (const ChunkBatchSubDraw& subDraw : subDraws)
        {
            lastDrawEnd = (std::max)(lastDrawEnd, subDraw.GetEndIndex());
        }

        span.indexCount = lastDrawEnd - span.startIndex;
        return span;
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

    graphic::QueueSubmissionToken ChunkRenderRegionStorage::CaptureLatestGraphicsRetirementToken() const
    {
        auto* commandListManager = graphic::D3D12RenderSystem::GetCommandListManager();
        if (commandListManager == nullptr)
        {
            return {};
        }

        const graphic::QueueSubmittedFenceSnapshot submittedSnapshot =
            commandListManager->GetLastSubmittedFenceSnapshot();
        if (submittedSnapshot.graphicsSubmitted == 0)
        {
            return {};
        }

        return { graphic::CommandQueueType::Graphics, submittedSnapshot.graphicsSubmitted };
    }

    void ChunkRenderRegionStorage::DrainRetiredVertexArenaAllocations(const graphic::QueueFenceSnapshot& completedSnapshot)
    {
        DrainRetiredRanges(m_vertexArena.freeRanges, m_vertexArena.retiredRanges, completedSnapshot);
    }

    void ChunkRenderRegionStorage::DrainRetiredIndexArenaAllocations(const graphic::QueueFenceSnapshot& completedSnapshot)
    {
        DrainRetiredRanges(m_indexArena.freeRanges, m_indexArena.retiredRanges, completedSnapshot);
    }

    void ChunkRenderRegionStorage::DrainRetiredArenaAllocations()
    {
        auto* commandListManager = graphic::D3D12RenderSystem::GetCommandListManager();
        if (commandListManager == nullptr)
        {
            return;
        }

        commandListManager->UpdateCompletedCommandLists();
        const graphic::QueueFenceSnapshot completedSnapshot = commandListManager->GetCompletedFenceSnapshot();
        DrainRetiredVertexArenaAllocations(completedSnapshot);
        DrainRetiredIndexArenaAllocations(completedSnapshot);
    }

    uint32_t ChunkRenderRegionStorage::GetVertexArenaRemainingCapacity() const
    {
        return SumFreeRangeCapacity(m_vertexArena.freeRanges);
    }

    uint32_t ChunkRenderRegionStorage::GetIndexArenaRemainingCapacity() const
    {
        return SumFreeRangeCapacity(m_indexArena.freeRanges);
    }

    ChunkRenderRegion& ChunkRenderRegionStorage::EnsureRegion(const ChunkBatchRegionId& regionId)
    {
        auto [it, inserted] = m_regions.try_emplace(regionId);
        if (inserted)
        {
            it->second.id = regionId;
            it->second.geometry.worldBounds = BuildFallbackRegionBounds(regionId);
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

    void ChunkRenderRegionStorage::ReleaseRegionArenaAllocations(ChunkRenderRegion& region)
    {
        RetireVertexArenaSlice(region.geometry.vertexAllocation);
        RetireIndexArenaSlice(region.geometry.indexAllocation);
        region.geometry.vertexAllocation.Reset();
        region.geometry.indexAllocation.Reset();
    }

    void ChunkRenderRegionStorage::ClearRegionGeometry(ChunkRenderRegion& region, bool buildFailed)
    {
        ReleaseRegionArenaAllocations(region);
        region.geometry.vertexBuffer.reset();
        region.geometry.indexBuffer.reset();
        region.geometry.opaque = {};
        region.geometry.cutout = {};
        region.geometry.translucent = {};
        region.geometry.opaqueSubDraws.clear();
        region.geometry.cutoutSubDraws.clear();
        region.geometry.translucentSubDraws.clear();
        region.geometry.gpuDataValid = false;
        region.chunkSlices.clear();
        region.dirtyChunkKeys.clear();
        region.buildFailed = buildFailed;
    }

    bool ChunkRenderRegionStorage::EnsureVertexArenaCapacity(uint32_t minimumContiguousVertices)
    {
        if (minimumContiguousVertices == 0u)
        {
            return true;
        }

        DrainRetiredArenaAllocations();
        if (!m_vertexArena.retiredRanges.empty())
        {
            if (!WaitForLatestGraphicsWorkCompletion())
            {
                ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to drain pending graphics work before vertex arena growth");
                return false;
            }

            DrainRetiredArenaAllocations();
            if (HasContiguousFreeRange(m_vertexArena.freeRanges, minimumContiguousVertices))
            {
                return true;
            }
        }

        // Future async reuse is limited to CPU-only relocation planning. The grow itself, persistent-memory
        // copy scheduling, and active region publication stay on the owning thread in the first version.
        const ChunkBatchArenaRelocationPlan relocationPlan = ChunkBatchArenaRelocationPlanner::BuildPlanFromFreeRanges(
            ChunkBatchArenaKind::Vertex,
            m_vertexArena.capacityVertices,
            minimumContiguousVertices,
            kDefaultVertexArenaCapacity,
            m_vertexArena.freeRanges);
        if (!ApplyVertexArenaRelocationPlan(relocationPlan))
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to grow shared vertex arena");
            return false;
        }

        return true;
    }

    bool ChunkRenderRegionStorage::EnsureIndexArenaCapacity(uint32_t minimumContiguousIndices)
    {
        if (minimumContiguousIndices == 0u)
        {
            return true;
        }

        DrainRetiredArenaAllocations();
        if (!m_indexArena.retiredRanges.empty())
        {
            if (!WaitForLatestGraphicsWorkCompletion())
            {
                ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to drain pending graphics work before index arena growth");
                return false;
            }

            DrainRetiredArenaAllocations();
            if (HasContiguousFreeRange(m_indexArena.freeRanges, minimumContiguousIndices))
            {
                return true;
            }
        }

        // Future async reuse is limited to CPU-only relocation planning. The grow itself, persistent-memory
        // copy scheduling, and active region publication stay on the owning thread in the first version.
        const ChunkBatchArenaRelocationPlan relocationPlan = ChunkBatchArenaRelocationPlanner::BuildPlanFromFreeRanges(
            ChunkBatchArenaKind::Index,
            m_indexArena.capacityIndices,
            minimumContiguousIndices,
            kDefaultIndexArenaCapacity,
            m_indexArena.freeRanges);
        if (!ApplyIndexArenaRelocationPlan(relocationPlan))
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to grow shared index arena");
            return false;
        }

        return true;
    }

    bool ChunkRenderRegionStorage::AllocateVertexArenaSlice(uint32_t vertexCount, ChunkBatchArenaAllocation& outAllocation)
    {
        DrainRetiredArenaAllocations();

        if (TryAllocateRange(m_vertexArena.freeRanges, vertexCount, outAllocation))
        {
            return true;
        }

        if (!EnsureVertexArenaCapacity(vertexCount))
        {
            return false;
        }

        if (!TryAllocateRange(m_vertexArena.freeRanges, vertexCount, outAllocation))
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Vertex arena allocation failed after grow");
            return false;
        }

        return true;
    }

    bool ChunkRenderRegionStorage::AllocateIndexArenaSlice(uint32_t indexCount, ChunkBatchArenaAllocation& outAllocation)
    {
        DrainRetiredArenaAllocations();

        if (TryAllocateRange(m_indexArena.freeRanges, indexCount, outAllocation))
        {
            return true;
        }

        if (!EnsureIndexArenaCapacity(indexCount))
        {
            return false;
        }

        if (!TryAllocateRange(m_indexArena.freeRanges, indexCount, outAllocation))
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Index arena allocation failed after grow");
            return false;
        }

        return true;
    }

    void ChunkRenderRegionStorage::FreeVertexArenaSlice(const ChunkBatchArenaAllocation& allocation)
    {
        FreeRange(m_vertexArena.freeRanges, allocation);
    }

    void ChunkRenderRegionStorage::FreeIndexArenaSlice(const ChunkBatchArenaAllocation& allocation)
    {
        FreeRange(m_indexArena.freeRanges, allocation);
    }

    void ChunkRenderRegionStorage::RetireVertexArenaSlice(const ChunkBatchArenaAllocation& allocation)
    {
        if (!allocation.IsValid())
        {
            return;
        }

        const graphic::QueueSubmissionToken retireToken = CaptureLatestGraphicsRetirementToken();
        if (!retireToken.IsValid())
        {
            FreeVertexArenaSlice(allocation);
            return;
        }

        m_vertexArena.retiredRanges.push_back({ allocation, retireToken });
    }

    void ChunkRenderRegionStorage::RetireIndexArenaSlice(const ChunkBatchArenaAllocation& allocation)
    {
        if (!allocation.IsValid())
        {
            return;
        }

        const graphic::QueueSubmissionToken retireToken = CaptureLatestGraphicsRetirementToken();
        if (!retireToken.IsValid())
        {
            FreeIndexArenaSlice(allocation);
            return;
        }

        m_indexArena.retiredRanges.push_back({ allocation, retireToken });
    }

    bool ChunkRenderRegionStorage::ApplyVertexArenaRelocationPlan(const ChunkBatchArenaRelocationPlan& relocationPlan)
    {
        std::unique_ptr<D12VertexBuffer> newBuffer = graphic::D3D12RenderSystem::CreateVertexBuffer(
            MakeChunkVertexArenaCreateInfo(relocationPlan.newCapacity));
        if (!newBuffer)
        {
            return false;
        }

        std::shared_ptr<D12VertexBuffer> oldBuffer = m_vertexArena.buffer;
        std::shared_ptr<D12VertexBuffer> newSharedBuffer(std::move(newBuffer));
        if (oldBuffer != nullptr && !WaitForLatestGraphicsWorkCompletion())
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to retire graphics work before vertex arena relocation");
            return false;
        }

        if (oldBuffer != nullptr && relocationPlan.HasCopySpans())
        {
            BufferTransferBatch transferBatch = {};
            transferBatch.workload       = enigma::graphic::QueueWorkloadClass::ChunkArenaRelocation;
            transferBatch.preferredQueue = CommandQueueType::Copy;
            AppendTransitionIfMissing(
                transferBatch.preTransferTransitions,
                oldBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                oldBuffer->GetCurrentState(),
                "ChunkBatchVertexArenaRelocationSource");
            AppendTransitionIfMissing(
                transferBatch.preTransferTransitions,
                newSharedBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                "ChunkBatchVertexArenaRelocationDestination");
            AppendTransitionIfMissing(
                transferBatch.postTransferTransitions,
                newSharedBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                "ChunkBatchVertexArenaRelocationDestination");

            for (const auto& copySpan : relocationPlan.copySpans)
            {
                if (!copySpan.IsValid())
                {
                    ERROR_AND_DIE("ChunkRenderRegionStorage: Vertex arena relocation plan contains an invalid copy span");
                }

                transferBatch.copySpans.push_back(BufferCopySpan{
                    oldBuffer.get(),
                    newSharedBuffer.get(),
                    ComputeByteOffset(copySpan.sourceAllocation, sizeof(TerrainVertex)),
                    ComputeByteOffset(copySpan.destinationAllocation, sizeof(TerrainVertex)),
                    ComputeByteSize(copySpan.sourceAllocation.elementCount, sizeof(TerrainVertex))
                });
            }

            if (!ExecuteCopyBatchAndWait(transferBatch))
            {
                ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to migrate used vertex arena ranges");
                return false;
            }
        }

        m_vertexArena.buffer = newSharedBuffer;
        m_vertexArena.capacityVertices = relocationPlan.newCapacity;
        m_vertexArena.freeRanges.clear();
        m_vertexArena.retiredRanges.clear();
        if (relocationPlan.newCapacity > relocationPlan.relocatedElementCount)
        {
            m_vertexArena.freeRanges.push_back(ChunkBatchArenaAllocation{
                relocationPlan.relocatedElementCount,
                relocationPlan.newCapacity - relocationPlan.relocatedElementCount
            });
        }

        RefreshRegionVertexAllocationsAfterRelocation(relocationPlan);
        RefreshVertexArenaBindings();
        RecordArenaRelocation(relocationPlan);
        oldBuffer.reset();
        return true;
    }

    bool ChunkRenderRegionStorage::ApplyIndexArenaRelocationPlan(const ChunkBatchArenaRelocationPlan& relocationPlan)
    {
        std::unique_ptr<D12IndexBuffer> newBuffer = graphic::D3D12RenderSystem::CreateIndexBuffer(
            MakeChunkIndexArenaCreateInfo(relocationPlan.newCapacity));
        if (!newBuffer)
        {
            return false;
        }

        std::shared_ptr<D12IndexBuffer> oldBuffer = m_indexArena.buffer;
        std::shared_ptr<D12IndexBuffer> newSharedBuffer(std::move(newBuffer));
        if (oldBuffer != nullptr && !WaitForLatestGraphicsWorkCompletion())
        {
            ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to retire graphics work before index arena relocation");
            return false;
        }

        if (oldBuffer != nullptr && relocationPlan.HasCopySpans())
        {
            BufferTransferBatch transferBatch = {};
            transferBatch.workload       = enigma::graphic::QueueWorkloadClass::ChunkArenaRelocation;
            transferBatch.preferredQueue = CommandQueueType::Copy;
            AppendTransitionIfMissing(
                transferBatch.preTransferTransitions,
                oldBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                oldBuffer->GetCurrentState(),
                "ChunkBatchIndexArenaRelocationSource");
            AppendTransitionIfMissing(
                transferBatch.preTransferTransitions,
                newSharedBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                "ChunkBatchIndexArenaRelocationDestination");
            AppendTransitionIfMissing(
                transferBatch.postTransferTransitions,
                newSharedBuffer.get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                "ChunkBatchIndexArenaRelocationDestination");

            for (const auto& copySpan : relocationPlan.copySpans)
            {
                if (!copySpan.IsValid())
                {
                    ERROR_AND_DIE("ChunkRenderRegionStorage: Index arena relocation plan contains an invalid copy span");
                }

                transferBatch.copySpans.push_back(BufferCopySpan{
                    oldBuffer.get(),
                    newSharedBuffer.get(),
                    ComputeByteOffset(copySpan.sourceAllocation, sizeof(uint32_t)),
                    ComputeByteOffset(copySpan.destinationAllocation, sizeof(uint32_t)),
                    ComputeByteSize(copySpan.sourceAllocation.elementCount, sizeof(uint32_t))
                });
            }

            if (!ExecuteCopyBatchAndWait(transferBatch))
            {
                ERROR_RECOVERABLE("ChunkRenderRegionStorage: Failed to migrate used index arena ranges");
                return false;
            }
        }

        m_indexArena.buffer = newSharedBuffer;
        m_indexArena.capacityIndices = relocationPlan.newCapacity;
        m_indexArena.freeRanges.clear();
        m_indexArena.retiredRanges.clear();
        if (relocationPlan.newCapacity > relocationPlan.relocatedElementCount)
        {
            m_indexArena.freeRanges.push_back(ChunkBatchArenaAllocation{
                relocationPlan.relocatedElementCount,
                relocationPlan.newCapacity - relocationPlan.relocatedElementCount
            });
        }

        RefreshRegionIndexAllocationsAfterRelocation(relocationPlan);
        RefreshIndexArenaBindings();
        RecordArenaRelocation(relocationPlan);
        oldBuffer.reset();
        return true;
    }

    void ChunkRenderRegionStorage::RefreshRegionVertexAllocationsAfterRelocation(const ChunkBatchArenaRelocationPlan& relocationPlan)
    {
        for (auto& [regionId, region] : m_regions)
        {
            UNUSED(regionId);
            if (!region.geometry.vertexAllocation.IsValid())
            {
                continue;
            }

            region.geometry.vertexAllocation = ChunkBatchArenaRelocationPlanner::ComputeRelocatedAllocation(
                relocationPlan,
                region.geometry.vertexAllocation);
        }
    }

    void ChunkRenderRegionStorage::RefreshRegionIndexAllocationsAfterRelocation(const ChunkBatchArenaRelocationPlan& relocationPlan)
    {
        for (auto& [regionId, region] : m_regions)
        {
            UNUSED(regionId);
            if (!region.geometry.indexAllocation.IsValid())
            {
                continue;
            }

            region.geometry.indexAllocation = ChunkBatchArenaRelocationPlanner::ComputeRelocatedAllocation(
                relocationPlan,
                region.geometry.indexAllocation);
        }
    }

    void ChunkRenderRegionStorage::RefreshRegionGeometryBindings(ChunkRenderRegion& region)
    {
        if (region.geometry.vertexAllocation.IsValid())
        {
            region.geometry.vertexBuffer = m_vertexArena.buffer;
        }

        if (region.geometry.indexAllocation.IsValid())
        {
            region.geometry.indexBuffer = m_indexArena.buffer;
        }
    }

    void ChunkRenderRegionStorage::RefreshVertexArenaBindings()
    {
        for (auto& [regionId, region] : m_regions)
        {
            UNUSED(regionId);
            RefreshRegionGeometryBindings(region);
        }
    }

    void ChunkRenderRegionStorage::RefreshIndexArenaBindings()
    {
        for (auto& [regionId, region] : m_regions)
        {
            UNUSED(regionId);
            RefreshRegionGeometryBindings(region);
        }
    }

    void ChunkRenderRegionStorage::RecordArenaRelocation(const ChunkBatchArenaRelocationPlan& relocationPlan)
    {
        ChunkBatchArenaSideDiagnostics* diagnostics = nullptr;
        switch (relocationPlan.arenaKind)
        {
        case ChunkBatchArenaKind::Vertex:
            diagnostics = &m_arenaDiagnostics.vertex;
            break;
        case ChunkBatchArenaKind::Index:
            diagnostics = &m_arenaDiagnostics.index;
            break;
        default:
            ERROR_AND_DIE("ChunkRenderRegionStorage: Unsupported arena diagnostics target");
        }

        diagnostics->lastRequestedCapacity = relocationPlan.minimumRequestedCapacity;
        diagnostics->lastCapacityBeforeGrow = relocationPlan.oldCapacity;
        diagnostics->lastCapacityAfterGrow = relocationPlan.newCapacity;
        diagnostics->lastRelocationElementCount = relocationPlan.relocatedElementCount;
        diagnostics->lastRelocationSpanCount = static_cast<uint32_t>(relocationPlan.copySpans.size());

        if (relocationPlan.IsGrowRequired())
        {
            diagnostics->growCountLifetime++;
        }

        if (relocationPlan.relocatedElementCount > 0u)
        {
            diagnostics->relocationCountLifetime++;
            diagnostics->relocatedElementCountLifetime += relocationPlan.relocatedElementCount;
        }
    }

    void ChunkRenderRegionStorage::RecordReplacementFallback(ChunkBatchArenaFallbackReason reason)
    {
        if (reason == ChunkBatchArenaFallbackReason::None)
        {
            return;
        }

        ChunkBatchArenaFallbackDiagnostics& diagnostics = m_arenaDiagnostics.fallback;
        diagnostics.totalCountLifetime++;
        diagnostics.lastReason = reason;

        switch (reason)
        {
        case ChunkBatchArenaFallbackReason::ReplacementStateInvalid:
            diagnostics.replacementStateInvalidCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::ReplacementVertexOverflow:
            diagnostics.replacementVertexOverflowCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::ReplacementIndexOverflow:
            diagnostics.replacementIndexOverflowCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::ReplacementVertexUploadFailed:
            diagnostics.replacementVertexUploadFailureCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::ReplacementIndexUploadFailed:
            diagnostics.replacementIndexUploadFailureCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::VertexArenaGrowFailed:
            diagnostics.vertexArenaGrowFailureCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::IndexArenaGrowFailed:
            diagnostics.indexArenaGrowFailureCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::VertexArenaUploadFailed:
            diagnostics.vertexArenaUploadFailureCountLifetime++;
            break;
        case ChunkBatchArenaFallbackReason::IndexArenaUploadFailed:
            diagnostics.indexArenaUploadFailureCountLifetime++;
            break;
        default:
            ERROR_AND_DIE("ChunkRenderRegionStorage: Unsupported chunk batch fallback reason");
        }

        if (IsReplacementUploadFallbackReason(reason))
        {
            m_replacementFallbackCount++;
        }
    }

    bool ChunkRenderRegionStorage::CommitBuildOutput(ChunkRenderRegion& region, const ChunkBatchRegionBuildOutput& buildOutput)
    {
        if (!buildOutput.HasCpuGeometry())
        {
            ClearRegionGeometry(region, false);
            region.geometry.regionModelMatrix = buildOutput.geometry.regionModelMatrix;
            region.geometry.worldBounds = buildOutput.geometry.worldBounds;
            region.buildFailed = false;
            return true;
        }

        const uint32_t vertexCount = buildOutput.totalVertexCapacity;
        const uint32_t indexCount  = buildOutput.totalIndexCapacity;

        ChunkBatchArenaAllocation newVertexAllocation;
        ChunkBatchArenaAllocation newIndexAllocation;
        if (!AllocateVertexArenaSlice(vertexCount, newVertexAllocation))
        {
            RecordReplacementFallback(ChunkBatchArenaFallbackReason::VertexArenaGrowFailed);
            FreeVertexArenaSlice(newVertexAllocation);
            FreeIndexArenaSlice(newIndexAllocation);
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Failed to allocate shared arena ranges for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        if (!AllocateIndexArenaSlice(indexCount, newIndexAllocation))
        {
            RecordReplacementFallback(ChunkBatchArenaFallbackReason::IndexArenaGrowFailed);
            FreeVertexArenaSlice(newVertexAllocation);
            FreeIndexArenaSlice(newIndexAllocation);
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Failed to allocate shared arena ranges for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        if (!ValidateUploadRange(
                m_vertexArena.buffer,
                newVertexAllocation,
                sizeof(TerrainVertex),
                buildOutput.vertices.size()))
        {
            RecordReplacementFallback(ChunkBatchArenaFallbackReason::VertexArenaUploadFailed);
            FreeVertexArenaSlice(newVertexAllocation);
            FreeIndexArenaSlice(newIndexAllocation);
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Invalid vertex upload range for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        if (!ValidateUploadRange(
                m_indexArena.buffer,
                newIndexAllocation,
                sizeof(uint32_t),
                buildOutput.indices.size()))
        {
            RecordReplacementFallback(ChunkBatchArenaFallbackReason::IndexArenaUploadFailed);
            FreeVertexArenaSlice(newVertexAllocation);
            FreeIndexArenaSlice(newIndexAllocation);
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Invalid index upload range for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        ChunkGeometryTransferPlan transferPlan = {};
        transferPlan.uploadBatch.workload       = enigma::graphic::QueueWorkloadClass::ChunkGeometryUpload;
        transferPlan.uploadBatch.preferredQueue = CommandQueueType::Copy;

        AppendTransitionIfMissing(
            transferPlan.uploadBatch.preTransferTransitions,
            m_vertexArena.buffer.get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            "ChunkBatchVertexArena");
        AppendTransitionIfMissing(
            transferPlan.uploadBatch.postTransferTransitions,
            m_vertexArena.buffer.get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            "ChunkBatchVertexArena");
        transferPlan.uploadBatch.uploadSpans.push_back(BufferUploadSpan{
            m_vertexArena.buffer.get(),
            ComputeByteOffset(newVertexAllocation, sizeof(TerrainVertex)),
            buildOutput.vertices.data(),
            ComputeByteSize(static_cast<uint32_t>(buildOutput.vertices.size()), sizeof(TerrainVertex))
        });

        AppendTransitionIfMissing(
            transferPlan.uploadBatch.preTransferTransitions,
            m_indexArena.buffer.get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            "ChunkBatchIndexArena");
        AppendTransitionIfMissing(
            transferPlan.uploadBatch.postTransferTransitions,
            m_indexArena.buffer.get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            "ChunkBatchIndexArena");
        transferPlan.uploadBatch.uploadSpans.push_back(BufferUploadSpan{
            m_indexArena.buffer.get(),
            ComputeByteOffset(newIndexAllocation, sizeof(uint32_t)),
            buildOutput.indices.data(),
            ComputeByteSize(static_cast<uint32_t>(buildOutput.indices.size()), sizeof(uint32_t))
        });

        if (!transferPlan.HasUploadWork() || !ExecuteUploadBatchAndWait(transferPlan.uploadBatch))
        {
            RecordReplacementFallback(ChunkBatchArenaFallbackReason::VertexArenaUploadFailed);
            FreeVertexArenaSlice(newVertexAllocation);
            FreeIndexArenaSlice(newIndexAllocation);
            ERROR_RECOVERABLE(Stringf("ChunkRenderRegionStorage: Failed to execute shared arena transfer batch for region (%d, %d)",
                region.id.regionCoords.x,
                region.id.regionCoords.y));
            region.buildFailed = true;
            return false;
        }

        const ChunkBatchArenaAllocation oldVertexAllocation = region.geometry.vertexAllocation;
        const ChunkBatchArenaAllocation oldIndexAllocation  = region.geometry.indexAllocation;

        region.geometry.regionModelMatrix = buildOutput.geometry.regionModelMatrix;
        region.geometry.worldBounds = buildOutput.geometry.worldBounds;
        region.geometry.opaque = buildOutput.geometry.opaque;
        region.geometry.cutout = buildOutput.geometry.cutout;
        region.geometry.translucent = buildOutput.geometry.translucent;
        region.geometry.opaqueSubDraws = buildOutput.geometry.opaqueSubDraws;
        region.geometry.cutoutSubDraws = buildOutput.geometry.cutoutSubDraws;
        region.geometry.translucentSubDraws = buildOutput.geometry.translucentSubDraws;
        region.geometry.vertexBuffer = m_vertexArena.buffer;
        region.geometry.indexBuffer = m_indexArena.buffer;
        region.geometry.vertexAllocation = newVertexAllocation;
        region.geometry.indexAllocation = newIndexAllocation;
        region.geometry.gpuDataValid = true;
        region.buildFailed = false;
        region.chunkSlices.clear();
        region.chunkSlices.reserve(buildOutput.chunkOutputs.size());
        region.dirtyChunkKeys.clear();

        for (const ChunkBatchChunkBuildOutput& chunkOutput : buildOutput.chunkOutputs)
        {
            ChunkBatchChunkRuntimeSlice runtimeSlice;
            runtimeSlice.vertexAllocation = chunkOutput.vertexAllocation;
            runtimeSlice.opaque = chunkOutput.opaque;
            runtimeSlice.cutout = chunkOutput.cutout;
            runtimeSlice.translucent = chunkOutput.translucent;
            runtimeSlice.worldBounds = chunkOutput.worldBounds;
            runtimeSlice.hasWorldBounds = chunkOutput.hasWorldBounds;
            region.chunkSlices.emplace(GetChunkKey(chunkOutput.chunkCoords), runtimeSlice);
        }

        RetireVertexArenaSlice(oldVertexAllocation);
        RetireIndexArenaSlice(oldIndexAllocation);
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

    bool ChunkRenderRegionStorage::TryApplyDirtyChunkReplacements(ChunkRenderRegion& region, ChunkBatchArenaFallbackReason& outFallbackReason)
    {
        outFallbackReason = ChunkBatchArenaFallbackReason::None;
        if (m_world == nullptr ||
            !region.geometry.gpuDataValid ||
            region.geometry.vertexBuffer == nullptr ||
            region.geometry.indexBuffer == nullptr ||
            !region.geometry.HasValidArenaAllocations() ||
            region.chunkSlices.empty() ||
            region.dirtyChunkKeys.empty())
        {
            return false;
        }

        auto failWithFallback = [&](ChunkBatchArenaFallbackReason reason)
        {
            outFallbackReason = reason;
            return false;
        };

        auto* commandListManager = graphic::D3D12RenderSystem::GetCommandListManager();
        if (commandListManager == nullptr)
        {
            return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementStateInvalid);
        }

        commandListManager->UpdateCompletedCommandLists();
        const graphic::QueueFenceSnapshot completedSnapshot = commandListManager->GetCompletedFenceSnapshot();
        const QueueSubmissionToken latestGraphicsToken = CaptureLatestGraphicsRetirementToken();
        if (!IsGraphicsSubmissionComplete(latestGraphicsToken, completedSnapshot))
        {
            return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementStateInvalid);
        }

        std::vector<int64_t> dirtyChunkKeys(region.dirtyChunkKeys.begin(), region.dirtyChunkKeys.end());
        std::sort(dirtyChunkKeys.begin(), dirtyChunkKeys.end());

        std::vector<PendingChunkReplacement> pendingReplacements;
        pendingReplacements.reserve(dirtyChunkKeys.size());

        for (int64_t chunkKey : dirtyChunkKeys)
        {
            auto runtimeSliceIt = region.chunkSlices.find(chunkKey);
            if (runtimeSliceIt == region.chunkSlices.end())
            {
                return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementStateInvalid);
            }

            PendingChunkReplacement replacement;
            replacement.chunkKey = chunkKey;
            replacement.runtimeSlice = &runtimeSliceIt->second;

            int32_t chunkX = 0;
            int32_t chunkY = 0;
            ChunkHelper::UnpackCoordinates(chunkKey, chunkX, chunkY);

            Chunk* chunk = m_world->GetChunk(chunkX, chunkY);
            if (!IsChunkEligibleForReplacement(chunk, region.id))
            {
                replacement.clearSlice = true;
                pendingReplacements.push_back(std::move(replacement));
                continue;
            }

            replacement.buildOutput = ChunkBatchRegionBuilder::BuildChunkGeometry(*chunk, region.id);
            if (!replacement.buildOutput.HasGeometry())
            {
                replacement.clearSlice = true;
                pendingReplacements.push_back(std::move(replacement));
                continue;
            }

            const ChunkBatchChunkRuntimeSlice& runtimeSlice = *replacement.runtimeSlice;
            if (replacement.buildOutput.vertices.size() > runtimeSlice.vertexAllocation.elementCount)
            {
                return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementVertexOverflow);
            }

            const ChunkBatchLayer layers[] = {
                ChunkBatchLayer::Opaque,
                ChunkBatchLayer::Cutout,
                ChunkBatchLayer::Translucent
            };

            for (ChunkBatchLayer layer : layers)
            {
                const ChunkBatchChunkLayerSlice& runtimeLayerSlice = runtimeSlice.GetLayerSlice(layer);
                const size_t newIndexCount = replacement.buildOutput.GetIndicesForLayer(layer).size();
                if (newIndexCount > runtimeLayerSlice.reservedIndexCount)
                {
                    return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementIndexOverflow);
                }
            }

            pendingReplacements.push_back(std::move(replacement));
        }

        uint32_t appliedReplacementCount = 0u;
        const ChunkBatchLayer layers[] = {
            ChunkBatchLayer::Opaque,
            ChunkBatchLayer::Cutout,
            ChunkBatchLayer::Translucent
        };

        for (PendingChunkReplacement& replacement : pendingReplacements)
        {
            ChunkBatchChunkRuntimeSlice& runtimeSlice = *replacement.runtimeSlice;
            BufferTransferBatch          replacementBatch = {};
            replacementBatch.workload       = enigma::graphic::QueueWorkloadClass::ChunkGeometryUpload;
            replacementBatch.preferredQueue = CommandQueueType::Copy;

            std::vector<std::vector<uint32_t>> replacementIndexUploads;
            replacementIndexUploads.reserve(3u);

            const bool  hasVertexUpload    = !replacement.clearSlice && !replacement.buildOutput.vertices.empty();
            bool        nextHasWorldBounds = false;
            AABB3       nextWorldBounds    = runtimeSlice.worldBounds;
            uint32_t    nextIndexCounts[3] = {};

            if (hasVertexUpload)
            {
                const ChunkBatchArenaAllocation absoluteVertexAllocation = MakeAbsoluteAllocation(
                    region.geometry.vertexAllocation,
                    runtimeSlice.vertexAllocation.startElement,
                    static_cast<uint32_t>(replacement.buildOutput.vertices.size()));
                if (!ValidateUploadRange(
                        m_vertexArena.buffer,
                        absoluteVertexAllocation,
                        sizeof(TerrainVertex),
                        replacement.buildOutput.vertices.size()))
                {
                    return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementVertexUploadFailed);
                }

                AppendTransitionIfMissing(
                    replacementBatch.preTransferTransitions,
                    m_vertexArena.buffer.get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    "ChunkBatchVertexArena");
                AppendTransitionIfMissing(
                    replacementBatch.postTransferTransitions,
                    m_vertexArena.buffer.get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    "ChunkBatchVertexArena");
                replacementBatch.uploadSpans.push_back(BufferUploadSpan{
                    m_vertexArena.buffer.get(),
                    ComputeByteOffset(absoluteVertexAllocation, sizeof(TerrainVertex)),
                    replacement.buildOutput.vertices.data(),
                    ComputeByteSize(static_cast<uint32_t>(replacement.buildOutput.vertices.size()), sizeof(TerrainVertex))
                });

                nextWorldBounds = replacement.buildOutput.worldBounds;
                nextHasWorldBounds = replacement.buildOutput.hasWorldBounds;
            }

            for (ChunkBatchLayer layer : layers)
            {
                ChunkBatchChunkLayerSlice& runtimeLayerSlice = runtimeSlice.GetLayerSlice(layer);
                const size_t               layerIndex = static_cast<size_t>(layer);
                if (!runtimeLayerSlice.HasReservedCapacity())
                {
                    nextIndexCounts[layerIndex] = 0u;
                    continue;
                }

                replacementIndexUploads.emplace_back(
                    runtimeLayerSlice.reservedIndexCount,
                    runtimeSlice.vertexAllocation.startElement);
                std::vector<uint32_t>& layerUploadIndices = replacementIndexUploads.back();

                if (!replacement.clearSlice)
                {
                    const std::vector<uint32_t>& sourceIndices = replacement.buildOutput.GetIndicesForLayer(layer);
                    for (uint32_t indexOffset = 0u; indexOffset < static_cast<uint32_t>(sourceIndices.size()); ++indexOffset)
                    {
                        const uint32_t sourceIndex = sourceIndices[indexOffset];
                        if (sourceIndex >= replacement.buildOutput.vertices.size())
                        {
                            ERROR_AND_DIE("ChunkRenderRegionStorage: Replacement upload encountered invalid chunk-local indices");
                        }

                        layerUploadIndices[indexOffset] = runtimeSlice.vertexAllocation.startElement + sourceIndex;
                    }

                    nextIndexCounts[layerIndex] = static_cast<uint32_t>(sourceIndices.size());
                }
                else
                {
                    nextIndexCounts[layerIndex] = 0u;
                }

                const ChunkBatchArenaAllocation absoluteIndexAllocation = MakeAbsoluteAllocation(
                    region.geometry.indexAllocation,
                    runtimeLayerSlice.startIndex,
                    runtimeLayerSlice.reservedIndexCount);
                if (!ValidateUploadRange(
                        m_indexArena.buffer,
                        absoluteIndexAllocation,
                        sizeof(uint32_t),
                        layerUploadIndices.size()))
                {
                    return failWithFallback(ChunkBatchArenaFallbackReason::ReplacementIndexUploadFailed);
                }

                AppendTransitionIfMissing(
                    replacementBatch.preTransferTransitions,
                    m_indexArena.buffer.get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    "ChunkBatchIndexArena");
                AppendTransitionIfMissing(
                    replacementBatch.postTransferTransitions,
                    m_indexArena.buffer.get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    "ChunkBatchIndexArena");
                replacementBatch.uploadSpans.push_back(BufferUploadSpan{
                    m_indexArena.buffer.get(),
                    ComputeByteOffset(absoluteIndexAllocation, sizeof(uint32_t)),
                    layerUploadIndices.data(),
                    ComputeByteSize(runtimeLayerSlice.reservedIndexCount, sizeof(uint32_t))
                });
            }

            if (!ExecuteUploadBatchAndWait(replacementBatch))
            {
                return failWithFallback(
                    hasVertexUpload
                        ? ChunkBatchArenaFallbackReason::ReplacementVertexUploadFailed
                        : ChunkBatchArenaFallbackReason::ReplacementIndexUploadFailed);
            }

            runtimeSlice.worldBounds = nextWorldBounds;
            runtimeSlice.hasWorldBounds = nextHasWorldBounds;
            for (ChunkBatchLayer layer : layers)
            {
                runtimeSlice.GetLayerSlice(layer).indexCount = nextIndexCounts[static_cast<size_t>(layer)];
            }

            appliedReplacementCount++;
        }

        region.geometry.opaqueSubDraws = BuildRegionSubDrawsFromChunkSlices(region.chunkSlices, ChunkBatchLayer::Opaque);
        region.geometry.cutoutSubDraws = BuildRegionSubDrawsFromChunkSlices(region.chunkSlices, ChunkBatchLayer::Cutout);
        region.geometry.translucentSubDraws = BuildRegionSubDrawsFromChunkSlices(region.chunkSlices, ChunkBatchLayer::Translucent);
        region.geometry.opaque = BuildRegionSpanFromSubDraws(region.geometry.opaqueSubDraws);
        region.geometry.cutout = BuildRegionSpanFromSubDraws(region.geometry.cutoutSubDraws);
        region.geometry.translucent = BuildRegionSpanFromSubDraws(region.geometry.translucentSubDraws);

        bool hasWorldBounds = false;
        AABB3 worldBounds = BuildFallbackRegionBounds(region.id);
        for (const auto& [chunkKey, chunkSlice] : region.chunkSlices)
        {
            UNUSED(chunkKey);
            if (!chunkSlice.hasWorldBounds)
            {
                continue;
            }

            if (!hasWorldBounds)
            {
                worldBounds = chunkSlice.worldBounds;
                hasWorldBounds = true;
            }
            else
            {
                worldBounds.StretchToIncludePoint(chunkSlice.worldBounds.m_mins);
                worldBounds.StretchToIncludePoint(chunkSlice.worldBounds.m_maxs);
            }
        }

        region.geometry.worldBounds = hasWorldBounds ? worldBounds : BuildFallbackRegionBounds(region.id);
        region.geometry.regionModelMatrix = Mat44::MakeTranslation3D(GetRegionOriginWorldPosition(region.id));
        region.geometry.vertexBuffer = m_vertexArena.buffer;
        region.geometry.indexBuffer = m_indexArena.buffer;
        region.geometry.gpuDataValid =
            region.geometry.vertexBuffer != nullptr &&
            region.geometry.indexBuffer != nullptr &&
            region.geometry.HasValidArenaAllocations();
        region.buildFailed = false;
        region.dirtyChunkKeys.clear();
        m_replacementUploadCount += appliedReplacementCount;
        return true;
    }

    void ChunkRenderRegionStorage::MarkChunkDirty(const IntVec2& chunkCoords)
    {
        const ChunkBatchRegionId regionId = GetChunkBatchRegionIdForChunk(chunkCoords);
        ChunkRenderRegion&       region   = EnsureRegion(regionId);
        const int64_t            chunkKey = GetChunkKey(chunkCoords);

        region.chunkKeys.insert(chunkKey);
        region.dirtyChunkKeys.insert(chunkKey);
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
        region.dirtyChunkKeys.insert(chunkKey);
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
        region.dirtyChunkKeys.insert(chunkKey);

        if (!region.HasResidentChunks())
        {
            RemoveQueuedDirtyRegion(regionId);
            ClearRegionGeometry(region, false);
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
            ClearRegionGeometry(region, false);
            m_regions.erase(regionIt);
            return true;
        }

        if (region.geometry.gpuDataValid &&
            region.geometry.vertexBuffer != nullptr &&
            region.geometry.indexBuffer != nullptr &&
            region.geometry.HasValidArenaAllocations() &&
            !region.chunkSlices.empty() &&
            !region.dirtyChunkKeys.empty())
        {
            ChunkBatchArenaFallbackReason fallbackReason = ChunkBatchArenaFallbackReason::None;
            if (TryApplyDirtyChunkReplacements(region, fallbackReason))
            {
                return true;
            }

            if (fallbackReason != ChunkBatchArenaFallbackReason::None)
            {
                RecordReplacementFallback(fallbackReason);
            }
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
