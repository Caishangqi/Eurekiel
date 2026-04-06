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

        for (const ChunkBatchDrawItem& item : collection.batchItems)
        {
            if (!item.IsValid())
            {
                continue;
            }

            SubmitDirect(item);
            submittedDrawCount++;
        }

        return submittedDrawCount;
    }

    void ChunkBatchRenderer::SubmitDirect(const ChunkBatchDrawItem& item)
    {
        if (!item.IsValid() || g_theRendererSubsystem == nullptr)
        {
            return;
        }

        PerObjectUniforms perObjectUniforms;
        FillPerObjectUniforms(perObjectUniforms, item.geometry->regionModelMatrix);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectUniforms);

        g_theRendererSubsystem->SetVertexBuffer(item.geometry->vertexBuffer.get());
        g_theRendererSubsystem->SetIndexBuffer(item.geometry->indexBuffer.get());
        g_theRendererSubsystem->DrawIndexed(item.indexCount, item.startIndex, 0);
    }
}
