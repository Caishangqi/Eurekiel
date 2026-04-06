#include "ChunkBatchCollector.hpp"

#include "../World/World.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace
{
    using enigma::voxel::ChunkBatchCollection;
    using enigma::voxel::ChunkBatchDrawItem;
    using enigma::voxel::ChunkBatchLayer;
    using enigma::voxel::ChunkRenderRegion;
    using enigma::voxel::World;

    bool CanUseRegionBatch(
        const ChunkRenderRegion* region,
        ChunkBatchLayer          layer)
    {
        if (region == nullptr || !region->HasValidBatchGeometry())
        {
            return false;
        }

        return !region->geometry.GetSpanForLayer(layer).IsEmpty();
    }

    void CollectLayerImpl(
        ChunkBatchCollection&       result,
        World&                      world,
        ChunkBatchLayer             layer)
    {
        auto& regions = world.GetChunkRenderRegionStorage().GetRegions();
        for (auto& [regionId, region] : regions)
        {
            UNUSED(regionId);
            if (!CanUseRegionBatch(&region, layer))
            {
                continue;
            }

            const auto& span = region.geometry.GetSpanForLayer(layer);
            ChunkBatchDrawItem drawItem;
            drawItem.regionId   = region.id;
            drawItem.geometry   = &region.geometry;
            drawItem.startIndex = span.startIndex;
            drawItem.indexCount = span.indexCount;
            result.batchItems.push_back(drawItem);
        }
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

        UNUSED(view.camera);
        UNUSED(view.visibilityHint);

        CollectLayerImpl(result, *view.world, layer);
        return result;
    }
}
