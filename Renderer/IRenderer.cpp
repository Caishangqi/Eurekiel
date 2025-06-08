#include "IRenderer.hpp"

#include "API/DX12Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

IRenderer* IRenderer::CreateRenderer(RenderConfig& config)
{
    switch (config.m_backend)
    {
    case RendererBackend::DirectX11:
        break;
        //return new DX11Renderer();
    case RendererBackend::DirectX12:
        return new DX12Renderer(config);
    case RendererBackend::OpenGL:
        break;
        //return new GLRenderer();
    }
    ERROR_AND_DIE("None specified RenderBacked")
}
