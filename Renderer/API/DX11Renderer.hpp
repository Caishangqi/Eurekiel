#pragma once
#include "Engine/Renderer/IRenderer.hpp"

class DX11Renderer : public IRenderer
{
public:
    DX11Renderer();
    void Startup() override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void ClearScreen(const Rgba8& color) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& vertices) override;
};
