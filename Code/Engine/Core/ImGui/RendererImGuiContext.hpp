#pragma once
#include "IImGuiRenderContext.hpp"
#include "Engine/Renderer/IRenderer.hpp"

class IRenderer; // Forward declaration

/**
 * @brief Adapter for legacy IRenderer to IImGuiRenderContext
 *
 * Wraps IRenderer* and implements IImGuiRenderContext interface
 * by delegating to IRenderer's GetD3D12XXX() methods.
 *
 * This adapter allows legacy applications (EurekielFeatureTest, SimpleMiner)
 * to use the new IImGuiRenderContext abstraction without major refactoring.
 *
 * Used by: EurekielFeatureTest (DX12), SimpleMiner (DX11)
 */
class RendererImGuiContext : public IImGuiRenderContext
{
public:
    explicit RendererImGuiContext(IRenderer* renderer);
    ~RendererImGuiContext() override = default;

    // IImGuiRenderContext implementation
    void*                                        GetDevice() const override;
    void*                                        GetCommandList() const override;
    void*                                        GetSRVHeap() const override;
    DXGI_FORMAT                                  GetRTVFormat() const override;
    uint32_t                                     GetNumFramesInFlight() const override;
    std::unique_ptr<enigma::core::IImGuiBackend> CreateBackend() override; // Factory method
    bool                                         IsReady() const override;
    void*                                        GetCommandQueue() const override;

    // DX11 specific resources (override default implementations)
    void* GetD3D11Device() const override;
    void* GetD3D11DeviceContext() const override;
    void* GetD3D11SwapChain() const override;

    // Internal helper method (not part of interface)
    RendererBackend GetBackendType() const;

private:
    IRenderer* m_renderer; // Non-owning pointer
};
