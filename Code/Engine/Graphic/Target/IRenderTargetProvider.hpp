#pragma once

// [NEW] Unified interface for all RenderTarget providers
// Part of RenderTarget Manager Architecture Refactoring

#include <d3d12.h>
#include <cstdint>

namespace enigma::graphic
{
    // Forward declarations
    struct RTConfig;
    class UniformManager;

    /**
     * @brief Unified interface for RenderTarget providers
     * 
     * All RT providers (ColorTexture, DepthTexture, ShadowColor, ShadowTexture)
     * implement this interface to enable polymorphic access via RenderTargetBinder.
     * 
     * Design principles:
     * - Single Responsibility: Each provider manages one RT type
     * - Open-Closed: Extend via new providers, not modification
     * - Dependency Inversion: RenderTargetBinder depends on abstraction
     */
    class IRenderTargetProvider
    {
    public:
        virtual ~IRenderTargetProvider() = default;

        // ========== Core Operations ==========

        /**
         * @brief Copy RT content from source to destination
         * @param srcIndex Source RT index
         * @param dstIndex Destination RT index
         * @throws InvalidIndexException if index out of bounds
         * @throws CopyOperationFailedException if copy fails
         */
        virtual void Copy(int srcIndex, int dstIndex) = 0;

        /**
         * @brief Clear RT to specified value
         * @param index RT index
         * @param clearValue Clear value (RGBA for color, depth for depth RT)
         * @throws InvalidIndexException if index out of bounds
         */
        virtual void Clear(int index, const float* clearValue) = 0;

        // ========== RTV/DSV Access ==========

        /**
         * @brief Get main RTV handle for binding
         * @param index RT index
         * @return CPU descriptor handle for main RTV
         */
        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) = 0;

        /**
         * @brief Get alternate RTV handle (for flip-state providers)
         * @param index RT index
         * @return CPU descriptor handle for alt RTV, or null handle if not supported
         */
        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) = 0;

        // ========== Resource Access ==========

        /**
         * @brief Get underlying D3D12 resource for main buffer
         * @param index RT index
         * @return ID3D12Resource pointer for main buffer
         * 
         * Use cases:
         * - CopyResource operations (e.g., PresentRenderTarget)
         * - Resource barrier transitions
         * - Direct resource manipulation
         */
        virtual ID3D12Resource* GetMainResource(int index) = 0;

        /**
         * @brief Get underlying D3D12 resource for alternate buffer
         * @param index RT index
         * @return ID3D12Resource pointer for alt buffer, or nullptr if not supported
         * 
         * Use cases:
         * - CopyResource operations with flip-state awareness
         * - Resource barrier transitions for double-buffered RTs
         */
        virtual ID3D12Resource* GetAltResource(int index) = 0;

        // ========== Bindless Index Access ==========

        /**
         * @brief Get bindless texture index for main buffer
         * @param index RT index
         * @return Bindless SRV index for shader access
         */
        virtual uint32_t GetMainTextureIndex(int index) = 0;

        /**
         * @brief Get bindless texture index for alternate buffer
         * @param index RT index
         * @return Bindless SRV index, or UINT32_MAX if not supported
         */
        virtual uint32_t GetAltTextureIndex(int index) = 0;

        // ========== FlipState Management ==========

        /**
         * @brief Flip main/alt buffers for specified RT
         * @param index RT index
         */
        virtual void Flip(int index) = 0;

        /**
         * @brief Flip all RTs managed by this provider
         */
        virtual void FlipAll() = 0;

        /**
         * @brief Reset all flip states to initial
         */
        virtual void Reset() = 0;

        // ========== Metadata ==========

        /**
         * @brief Get number of RTs managed by this provider
         * @return RT count
         */
        virtual int GetCount() const = 0;

        /**
         * @brief Get render target format at specified index
         * @param index RT index
         * @return DXGI_FORMAT of the render target
         * @throws InvalidIndexException if index out of bounds
         * 
         * [FIX] Added to support PSO creation with correct RTVFormat.
         * Required by RenderTargetBinder to populate format cache.
         */
        virtual DXGI_FORMAT GetFormat(int index) const = 0;

        // ========== Capability Query ==========

        /**
         * @brief Check if provider supports flip-state (main/alt buffers)
         * @return true if flip-state supported
         */
        virtual bool SupportsFlipState() const = 0;

        /**
         * @brief Check if provider supports DSV (depth-stencil view)
         * @return true if DSV supported
         */
        virtual bool SupportsDSV() const = 0;

        // ========== Dynamic Configuration ==========

        /**
         * @brief Dynamically reconfigure RT at specified index
         * @param index RT index
         * @param config New RT configuration
         * @throws InvalidIndexException if index out of bounds
         * @throws ResourceNotReadyException if resource recreation fails
         * 
         * Use cases:
         * - ShadowCamera: orthographic projection with square viewport (e.g. 1024x1024)
         * - PlayerCamera: perspective projection with non-square viewport (e.g. 1920x1080)
         * - Runtime resolution adjustment
         */
        virtual void SetRtConfig(int index, const RTConfig& config) = 0;

        // ========== [NEW] Uniform Registration ==========

        /**
         * @brief Register IndexBuffer to UniformManager for GPU upload
         * @param uniformMgr Pointer to UniformManager instance
         * 
         * Called during provider initialization to register the provider's
         * IndexBuffer (e.g., ColorTargetsIndexBuffer) to UniformManager.
         * This enables automatic GPU synchronization of bindless indices.
         */
        virtual void RegisterUniform(UniformManager* uniformMgr) = 0;

        /**
         * @brief Collect and upload bindless indices to GPU
         * 
         * Called each frame or after Flip operations to synchronize
         * the provider's bindless indices with GPU constant buffer.
         * Shader can then access RT via GetColorTexture(slot) etc.
         */
        virtual void UpdateIndices() = 0;
    };
} // namespace enigma::graphic
