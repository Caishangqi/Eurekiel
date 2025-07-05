#include "IRenderer.hpp"

#include "API/DX12Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Image.hpp"

#undef max
#undef min
Vertex_PCUTBN* ConversionBuffer::Allocate(size_t count)
{
    if (cursor + count > buffer.size())
    {
        // Expansion strategy: 1.5 times growth

        size_t newSize = std::max(buffer.size() * 3 / 2, cursor + count);
        buffer.resize(newSize);
    }

    Vertex_PCUTBN* result = buffer.data() + cursor;
    cursor += count;
    return result;
}
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

