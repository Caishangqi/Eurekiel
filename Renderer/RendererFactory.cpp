#include "RendererFactory.hpp"

#include "API/DX11Renderer.hpp"
#include "API/DX12Renderer.hpp"
#include "API/GLRenderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

IRenderer* CreateRenderer(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::DirectX11:
        return new DX11Renderer();
    case RendererBackend::DirectX12:
        return new DX12Renderer();
    case RendererBackend::OpenGL:
        return new GLRenderer();
    }
    ERROR_AND_DIE("None specified RenderBacked")
}
