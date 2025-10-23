#include "RendererSubsystemImGuiContext.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Core/ImGui/ImGuiBackendDX12.hpp"

/**
 * @brief Adapter Pattern Implementation
 *
 * This file implements the Adapter Pattern to bridge RendererSubsystem (new architecture)
 * with the IImGuiRenderContext interface required by ImGui integration.
 *
 * All methods follow a consistent pattern:
 * 1. Check if m_rendererSubsystem is valid (nullptr check)
 * 2. Delegate to corresponding RendererSubsystem getter method
 * 3. Return nullptr/default value if RendererSubsystem is invalid
 *
 * @note Compilation will fail until Task 7 adds the required getter methods to RendererSubsystem.
 *       This is expected and intentional - the structure is correct and will compile after Task 7.
 */

RendererSubsystemImGuiContext::RendererSubsystemImGuiContext(enigma::graphic::RendererSubsystem* rendererSubsystem)
    : m_rendererSubsystem(rendererSubsystem)
{
}

void* RendererSubsystemImGuiContext::GetDevice() const
{
    if (!m_rendererSubsystem)
    {
        return nullptr;
    }
    return m_rendererSubsystem->GetD3D12Device();
}

void* RendererSubsystemImGuiContext::GetCommandList() const
{
    if (!m_rendererSubsystem)
    {
        return nullptr;
    }
    return m_rendererSubsystem->GetCurrentCommandList();
}

void* RendererSubsystemImGuiContext::GetSRVHeap() const
{
    if (!m_rendererSubsystem)
    {
        return nullptr;
    }
    return m_rendererSubsystem->GetSRVHeap();
}

DXGI_FORMAT RendererSubsystemImGuiContext::GetRTVFormat() const
{
    if (!m_rendererSubsystem)
    {
        // Return common default format if RendererSubsystem is invalid
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    return m_rendererSubsystem->GetRTVFormat();
}

uint32_t RendererSubsystemImGuiContext::GetNumFramesInFlight() const
{
    if (!m_rendererSubsystem)
    {
        // Return safe default value (typical double buffering)
        return 2;
    }
    return m_rendererSubsystem->GetFramesInFlight();
}

bool RendererSubsystemImGuiContext::IsReady() const
{
    if (!m_rendererSubsystem)
    {
        return false;
    }
    return m_rendererSubsystem->IsInitialized();
}

void* RendererSubsystemImGuiContext::GetCommandQueue() const
{
    if (!m_rendererSubsystem)
    {
        return nullptr;
    }
    return m_rendererSubsystem->GetCommandQueue();
}

std::unique_ptr<enigma::core::IImGuiBackend> RendererSubsystemImGuiContext::CreateBackend()
{
    // Factory method - create DirectX 12 ImGui backend
    // RendererSubsystem (new architecture) only supports DirectX 12
    return std::make_unique<enigma::core::ImGuiBackendDX12>(this);
}
