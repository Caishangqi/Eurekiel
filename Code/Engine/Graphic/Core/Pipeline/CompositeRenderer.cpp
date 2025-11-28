#include "CompositeRenderer.hpp"

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    CompositeRenderer::CompositeRenderer(std::shared_ptr<IWorldRenderingPipeline>    pipeline, CompositePass                       compositePass, PackDirectives                 packDirectives,
                                         std::vector<std::shared_ptr<ShaderSource>>& shaderSources, std::shared_ptr<RenderTargets> renderTargets, std::shared_ptr<BufferFlipper> buffer,
                                         std::shared_ptr<ShadowRenderTargets>        shadowTargets, TextureStage                   textureState)
    {
        UNUSED(compositePass)
        UNUSED(packDirectives)
        UNUSED(shaderSources)
        UNUSED(renderTargets)
        UNUSED(buffer)
        UNUSED(shadowTargets)
        UNUSED(textureState)
        UNUSED(pipeline)
    }

    CompositeRenderer::~CompositeRenderer()
    {
    }
}
