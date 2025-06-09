#pragma once

class IRenderer;

enum class RendererBackend
{
    DirectX11,
    DirectX12,
    OpenGL
};

IRenderer* CreateRenderer(RendererBackend backend);
