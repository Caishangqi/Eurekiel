#include "RendererImGuiContext.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "ImGuiBackendDX11.hpp"
#include "ImGuiBackendDX12.hpp"

RendererImGuiContext::RendererImGuiContext(IRenderer* renderer)
    : m_renderer(renderer)
{
}

// ========== IImGuiRenderContext implementation ==========

void* RendererImGuiContext::GetDevice() const
{
    if (!m_renderer)
        return nullptr;

    // Return DX12 device if available, otherwise fallback to DX11
    void* device = m_renderer->GetD3D12Device();
    if (device)
        return device;

    return m_renderer->GetD3D11Device();
}

void* RendererImGuiContext::GetCommandList() const
{
    if (!m_renderer)
        return nullptr;

    // Return DX12 command list if available, otherwise DX11 device context
    void* cmdList = m_renderer->GetD3D12CommandList();
    if (cmdList)
        return cmdList;

    return m_renderer->GetD3D11DeviceContext();
}

void* RendererImGuiContext::GetSRVHeap() const
{
    if (!m_renderer)
        return nullptr;

    // Only DX12 has descriptor heaps
    return m_renderer->GetD3D12SRVHeap();
}

DXGI_FORMAT RendererImGuiContext::GetRTVFormat() const
{
    if (!m_renderer)
        return DXGI_FORMAT_UNKNOWN;

    return m_renderer->GetRTVFormat();
}

uint32_t RendererImGuiContext::GetNumFramesInFlight() const
{
    if (!m_renderer)
        return 0;

    return m_renderer->GetNumFramesInFlight();
}

RendererBackend RendererImGuiContext::GetBackendType() const
{
    if (!m_renderer)
        return RendererBackend::DirectX11; // Default fallback

    return m_renderer->GetBackendType();
}

bool RendererImGuiContext::IsReady() const
{
    if (!m_renderer)
        return false;

    return m_renderer->IsRendererReady();
}

void* RendererImGuiContext::GetCommandQueue() const
{
    if (!m_renderer)
        return nullptr;

    // Only DX12 has command queue
    return m_renderer->GetD3D12CommandQueue();
}

void* RendererImGuiContext::GetD3D11Device() const
{
    if (!m_renderer)
        return nullptr;

    return m_renderer->GetD3D11Device();
}

void* RendererImGuiContext::GetD3D11DeviceContext() const
{
    if (!m_renderer)
        return nullptr;

    return m_renderer->GetD3D11DeviceContext();
}

void* RendererImGuiContext::GetD3D11SwapChain() const
{
    if (!m_renderer)
        return nullptr;

    return m_renderer->GetD3D11SwapChain();
}

std::unique_ptr<enigma::core::IImGuiBackend> RendererImGuiContext::CreateBackend()
{
    using namespace enigma::core;

    if (!m_renderer)
    {
        return nullptr;
    }

    // 基于IRenderer的后端类型创建对应的Backend
    RendererBackend backendType = m_renderer->GetBackendType();

    switch (backendType)
    {
    case RendererBackend::DirectX11:
        return std::make_unique<ImGuiBackendDX11>(this);

    case RendererBackend::DirectX12:
        return std::make_unique<ImGuiBackendDX12>(this);

    default:
        return nullptr;
    }
}
