//==============================================================================
// IImGuiRenderContext.hpp
// Abstract interface for ImGui renderer context
//
// Provides platform-agnostic access to DirectX 12 rendering resources required
// by ImGui backend implementation. Uses void* returns to avoid exposing D3D12
// types in the interface.
//
// Created: 2025-10-22
//==============================================================================

#pragma once

#include <cstdint>
#include <memory>
#include <dxgiformat.h>

#include "Engine/Renderer/IRenderer.hpp"

// Forward declarations
namespace enigma::core
{
    class IImGuiBackend;
}

//==============================================================================
// IImGuiRenderContext
//==============================================================================
/// @brief Abstract interface defining DirectX 12 resource access for ImGui rendering
/// @details Decouples ImGui backend from concrete renderer implementations
class IImGuiRenderContext
{
public:
    virtual ~IImGuiRenderContext() = default;

    //--------------------------------------------------------------------------
    // Core Resource Access (Required)
    //--------------------------------------------------------------------------

    /// @brief Get DirectX 12 device pointer
    /// @return ID3D12Device* as void*, nullptr if not ready
    virtual void* GetDevice() const = 0;

    /// @brief Get current frame's graphics command list
    /// @return ID3D12GraphicsCommandList* as void*, nullptr if not ready
    virtual void* GetCommandList() const = 0;

    /// @brief Get shader resource view descriptor heap
    /// @return ID3D12DescriptorHeap* as void*, must be shader-visible
    virtual void* GetSRVHeap() const = 0;

    /// @brief Get render target view format for current swap chain
    /// @return DXGI_FORMAT (e.g., DXGI_FORMAT_R8G8B8A8_UNORM)
    virtual DXGI_FORMAT GetRTVFormat() const = 0;

    /// @brief Get number of frames in flight (swap chain buffer count)
    /// @return Frame buffer count (typically 2 or 3)
    virtual uint32_t GetNumFramesInFlight() const = 0;

    //--------------------------------------------------------------------------
    // Factory Method
    //--------------------------------------------------------------------------

    /// @brief Create appropriate ImGui backend for this rendering context
    /// @return Unique pointer to IImGuiBackend instance
    /// @note Factory method pattern - concrete context determines backend type
    virtual std::unique_ptr<enigma::core::IImGuiBackend> CreateBackend() = 0;

    //--------------------------------------------------------------------------
    // State Query
    //--------------------------------------------------------------------------

    /// @brief Check if rendering context is initialized and ready
    /// @return true if all resources are available, false otherwise
    virtual bool IsReady() const = 0;

    //--------------------------------------------------------------------------
    // Optional Resources (Default Implementation)
    //--------------------------------------------------------------------------

    /// @brief Get command queue for resource uploads (optional)
    /// @return ID3D12CommandQueue* as void*, nullptr if not supported
    virtual void* GetCommandQueue() const { return nullptr; }

    //--------------------------------------------------------------------------
    // DirectX 11 Specific Resources (for DX11 backend)
    //--------------------------------------------------------------------------

    /// @brief Get DirectX 11 device pointer
    /// @return ID3D11Device* as void*, nullptr if not DX11 or not ready
    virtual void* GetD3D11Device() const { return nullptr; }

    /// @brief Get DirectX 11 device context
    /// @return ID3D11DeviceContext* as void*, nullptr if not DX11 or not ready
    virtual void* GetD3D11DeviceContext() const { return nullptr; }

    /// @brief Get DirectX 11 swap chain
    /// @return IDXGISwapChain* as void*, nullptr if not DX11 or not ready
    virtual void* GetD3D11SwapChain() const { return nullptr; }
};
