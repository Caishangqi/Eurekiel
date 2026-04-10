#include "ChunkBatchRenderer.hpp"

#include "Chunk.hpp"
#include "ChunkMesh.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

namespace
{
    using enigma::graphic::PerObjectUniforms;
    using enigma::voxel::ChunkBatchDrawItem;
    using enigma::voxel::ChunkBatchRegionGeometry;
    using enigma::voxel::ChunkBatchRenderer;

    void FillPerObjectUniforms(PerObjectUniforms& uniforms, const Mat44& modelMatrix)
    {
        uniforms.modelMatrix        = modelMatrix;
        uniforms.modelMatrixInverse = modelMatrix.GetInverse();
        Rgba8::WHITE.GetAsFloats(uniforms.modelColor);
    }
}

namespace enigma::voxel
{
    uint32_t ChunkBatchRenderer::Submit(const ChunkBatchCollection& collection)
    {
        uint32_t submittedDrawCount = 0;
        const ChunkBatchRegionGeometry* boundGeometry = nullptr;

        for (const ChunkBatchDrawItem& item : collection.batchItems)
        {
            if (!item.IsValid())
            {
                continue;
            }

            if (boundGeometry != item.geometry)
            {
                BindRegionGeometry(*item.geometry);
                boundGeometry = item.geometry;
            }

            const uint32_t startIndex = item.ResolveDirectPreciseStartIndex();
            const int32_t  baseVertex = item.ResolveDirectPreciseBaseVertex();
            g_theRendererSubsystem->DrawIndexed(item.subDraw->indexCount, startIndex, baseVertex);
            submittedDrawCount++;
        }

        return submittedDrawCount;
    }

    void ChunkBatchRenderer::BindRegionGeometry(const ChunkBatchRegionGeometry& geometry)
    {
        if (!geometry.HasValidDirectPreciseBuffers() || g_theRendererSubsystem == nullptr)
        {
            return;
        }

        PerObjectUniforms perObjectUniforms;
        FillPerObjectUniforms(perObjectUniforms, geometry.regionModelMatrix);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

        g_theRendererSubsystem->SetVertexBuffer(geometry.vertexBuffer.get());
        g_theRendererSubsystem->SetIndexBuffer(geometry.indexBuffer.get());
    }

    void ChunkBatchRenderer::SubmitDirect(const ChunkBatchDrawItem& item)
    {
        if (!item.IsValid() || g_theRendererSubsystem == nullptr)
        {
            return;
        }

        BindRegionGeometry(*item.geometry);
        const uint32_t startIndex = item.ResolveDirectPreciseStartIndex();
        const int32_t  baseVertex = item.ResolveDirectPreciseBaseVertex();
        g_theRendererSubsystem->DrawIndexed(item.subDraw->indexCount, startIndex, baseVertex);
    }
}
