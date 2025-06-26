#include "IRenderer.hpp"

#include "API/DX11Renderer.hpp"
#include "API/DX12Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Image.hpp"


Image* IRenderer::CreateImageFromFile(const char* imageFilePath)
{
    auto image = new Image(imageFilePath);
    return image;
}

IRenderer* IRenderer::CreateRenderer(RenderConfig& config)
{
    switch (config.m_backend)
    {
    case RendererBackend::DirectX11:
    //return new DX11Renderer();
    case RendererBackend::DirectX12:
        return new DX12Renderer(config);
    case RendererBackend::OpenGL:
        break;
        //return new GLRenderer();
    }
    ERROR_AND_DIE("None specified RenderBacked")
}
