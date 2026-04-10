#include "ChunkBatchCollector.hpp"

#include "ChunkOcclusionCuller.hpp"
#include "../World/World.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Camera/ICamera.hpp"
#include "Engine/Visibility/OcclusionCuller.hpp"

namespace
{
    using enigma::graphic::CameraType;
    using enigma::voxel::ChunkBatchCollection;
    using enigma::voxel::ChunkBatchDrawItem;
    using enigma::voxel::ChunkBatchLayer;
    using enigma::voxel::ChunkRenderRegion;
    using enigma::voxel::ChunkOcclusionCuller;
    using enigma::voxel::World;

    enum class BatchCullingTarget : uint8_t
    {
        None = 0,
        MainCamera,
        ShadowCamera
    };

    bool CanUseDirectPreciseRegionBatch(
        const ChunkRenderRegion* region,
        ChunkBatchLayer          layer)
    {
        if (region == nullptr || !region->HasValidBatchGeometry())
        {
            return false;
        }

        return region->geometry.SupportsDirectPreciseLayer(layer);
    }

    void AppendRegionDrawItems(
        ChunkBatchCollection&       result,
        const ChunkRenderRegion&    region,
        ChunkBatchLayer             layer)
    {
        if (!CanUseDirectPreciseRegionBatch(&region, layer))
        {
            return;
        }

        const auto& subDraws = region.geometry.GetSubDrawsForLayer(layer);
        result.batchItems.reserve(result.batchItems.size() + subDraws.size());

        for (const auto& subDraw : subDraws)
        {
            ChunkBatchDrawItem drawItem;
            drawItem.regionId = region.id;
            drawItem.geometry = &region.geometry;
            drawItem.subDraw = &subDraw;
            result.batchItems.push_back(drawItem);
        }
    }

    void CollectAllRegions(
        ChunkBatchCollection& result,
        World&                world,
        ChunkBatchLayer       layer)
    {
        const auto& regions = world.GetChunkRenderRegionStorage().GetRegions();
        for (const auto& [regionId, region] : regions)
        {
            UNUSED(regionId);
            AppendRegionDrawItems(result, region, layer);
        }
    }

    BatchCullingTarget GetBatchCullingTarget(
        const enigma::voxel::ChunkBatchViewContext& view,
        ChunkBatchLayer                              layer)
    {
        if (view.camera == nullptr)
        {
            return BatchCullingTarget::None;
        }

        switch (view.camera->GetCameraType())
        {
        case CameraType::Perspective:
        case CameraType::Orthographic:
            return BatchCullingTarget::MainCamera;
        case CameraType::Shadow:
            return BatchCullingTarget::ShadowCamera;
        default:
            return BatchCullingTarget::None;
        }
    }

    void UpdateCullingStats(
        World&                                         world,
        BatchCullingTarget                             cullingTarget,
        const enigma::visibility::OcclusionCullResult& cullingResult)
    {
        auto& stats = world.MutableChunkBatchStats();
        switch (cullingTarget)
        {
        case BatchCullingTarget::MainCamera:
            stats.visibleRegions = cullingResult.visibleItemCount;
            stats.culledRegions = cullingResult.culledItemCount;
            break;
        case BatchCullingTarget::ShadowCamera:
            stats.shadowVisibleRegions = cullingResult.visibleItemCount;
            stats.shadowCulledRegions = cullingResult.culledItemCount;
            break;
        default:
            break;
        }
    }

    bool CollectCulledRegions(
        ChunkBatchCollection&                       result,
        const enigma::voxel::ChunkBatchViewContext& view,
        ChunkBatchLayer                              layer)
    {
        const BatchCullingTarget cullingTarget = GetBatchCullingTarget(view, layer);
        if (cullingTarget == BatchCullingTarget::None || view.world == nullptr)
        {
            return false;
        }

        const ChunkOcclusionCuller domainCuller(view.world->GetChunkRenderRegionStorage());
        const enigma::visibility::OcclusionCullResult cullingResult =
            enigma::visibility::OcclusionCuller::Cull(*view.camera, domainCuller);
        if (!cullingResult.volumeValid)
        {
            return false;
        }

        UpdateCullingStats(*view.world, cullingTarget, cullingResult);

        for (const void* visibleItem : cullingResult.visibleItems)
        {
            const auto* region = static_cast<const ChunkRenderRegion*>(visibleItem);
            if (region == nullptr)
            {
                continue;
            }

            AppendRegionDrawItems(result, *region, layer);
        }

        return true;
    }
}

namespace enigma::voxel
{
    ChunkBatchCollection ChunkBatchCollector::Collect(
        const ChunkBatchViewContext& view,
        ChunkBatchLayer              layer)
    {
        ChunkBatchCollection result;
        if (view.world == nullptr)
        {
            return result;
        }

        UNUSED(view.visibilityHint);

        if (!CollectCulledRegions(result, view, layer))
        {
            CollectAllRegions(result, *view.world, layer);
        }

        return result;
    }
}
