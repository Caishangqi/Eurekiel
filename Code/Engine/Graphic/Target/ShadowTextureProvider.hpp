#pragma once

// ============================================================================
// ShadowTextureProvider.hpp - [NEW] Shadow depth texture provider
// Implements IRenderTargetProvider for shadowtex0-1 management
// Part of RenderTarget Manager Unified Architecture Refactoring
// ============================================================================

#include "IRenderTargetProvider.hpp"
#include "RenderTargetProviderCommon.hpp"
#include "RenderTargetProviderException.hpp"
#include "D12DepthTexture.hpp"
#include "Engine/Graphic/Shader/Uniform/ShadowTexturesIndexUniforms.hpp"

#include <vector>
#include <memory>
#include <string>

namespace enigma::graphic
{
    // Forward declarations
    class D3D12RenderSystem;
    class UniformManager;

    /**
     * @class ShadowTextureProvider
     * @brief Manages shadowtex0-1 with D12DepthTexture, no FlipState support
     * 
     * [NEW] Implements IRenderTargetProvider for shadow depth textures.
     * 
     * Features:
     * - Manages 1-2 shadow depth textures (shadowtex0-1)
     * - NO flip-state support (shadow depth textures are single-buffered)
     * - DSV access via GetDSV() method
     * - Fixed square resolution (not screen-dependent)
     * - Bindless texture index access for shader sampling
     * 
     * Iris compatibility:
     * - shadowtex0: Main shadow depth (all shadow casters)
     * - shadowtex1: Pre-translucent shadow depth (before translucent shadow casters)
     * 
     * Key difference from DepthTextureProvider:
     * - Fixed square resolution (e.g. 1024x1024, 2048x2048)
     * - No screen-relative scaling
     */
    class ShadowTextureProvider : public IRenderTargetProvider
    {
    public:
        // ========================================================================
        // Constructor / Destructor
        // ========================================================================

        /**
         * @brief RAII constructor - creates shadow depth textures from config
         * @param baseWidth Base width (for interface consistency, shadow uses fixed resolution from config)
         * @param baseHeight Base height (for interface consistency, shadow uses fixed resolution from config)
         * @param configs Vector of RTConfig for each shadowtex (use ShadowDepthTarget factory)
         * @param uniformMgr UniformManager pointer for Bindless index upload (required)
         * @throws std::invalid_argument if configs invalid or uniformMgr null
         * @throws std::out_of_range if config count out of range [1-2]
         */
        ShadowTextureProvider(
            int                                    baseWidth,
            int                                    baseHeight,
            const std::vector<RenderTargetConfig>& configs,
            UniformManager*                        uniformMgr
        );

        ~ShadowTextureProvider() override = default;

        // Non-copyable, movable
        ShadowTextureProvider(const ShadowTextureProvider&)            = delete;
        ShadowTextureProvider& operator=(const ShadowTextureProvider&) = delete;
        ShadowTextureProvider(ShadowTextureProvider&&)                 = default;
        ShadowTextureProvider& operator=(ShadowTextureProvider&&)      = default;

        // ========================================================================
        // IRenderTargetProvider Implementation
        // ========================================================================

        // Core Operations
        void Copy(int srcIndex, int dstIndex) override;
        void Clear(int index, const float* clearValue) override;

        // RTV Access - shadow depth textures have NO RTV, throws logic_error
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) override;
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) override;

        // Resource Access
        ID3D12Resource* GetMainResource(int index) override;
        ID3D12Resource* GetAltResource(int index) override;

        // Bindless Index Access
        uint32_t GetMainTextureIndex(int index) override;
        uint32_t GetAltTextureIndex(int index) override;

        // FlipState Management - no-op for shadow depth textures
        void Flip(int index) override;
        void FlipAll() override;
        void Reset() override;

        // Metadata
        int GetCount() const override { return m_activeCount; }

        // Capability Query
        bool SupportsFlipState() const override { return false; }
        bool SupportsDSV() const override { return true; }

        // Dynamic Configuration
        void SetRtConfig(int index, const RenderTargetConfig& config) override;

        // [NEW] Reset and Config Query
        void                      ResetToDefault(const std::vector<RenderTargetConfig>& defaultConfigs) override;
        const RenderTargetConfig& GetConfig(int index) const override;

        // ========================================================================
        // [NEW] Uniform Update API - Shader RT Fetching Feature
        // ========================================================================

        /**
         * @brief Update and upload bindless indices to GPU
         * @note Call after resource recreation (no flip for shadow textures)
         */
        void UpdateIndices();

        // ========================================================================
        // Extended API (ShadowTexture-specific)
        // ========================================================================

        /**
         * @brief Get DSV handle for shadow depth binding
         * @param index Shadow texture index [0-1]
         * @return CPU descriptor handle for DSV
         * @throws InvalidIndexException if index out of bounds
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(int index) const;

        /**
         * @brief Get underlying D12DepthTexture
         * @param index Shadow texture index
         * @return Shared pointer to D12DepthTexture
         */
        std::shared_ptr<D12DepthTexture> GetDepthTexture(int index) const;

        /**
         * @brief Copy shadowtex0 -> shadowtex1 (Iris CopyPreTranslucentDepth equivalent)
         * @param cmdList Command list for copy operation
         * 
         * Use case: Capture shadow depth before translucent shadow casters
         */
        void CopyPreTranslucentDepth(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief Get current shadow map width
         * @return Width in pixels
         */
        int GetBaseWidth() const { return m_baseWidth; }

        /**
         * @brief Get current shadow map height
         * @return Height in pixels
         */
        int GetBaseHeight() const { return m_baseHeight; }

        /**
         * @brief Get format of shadow texture at index
         * @param index Shadow texture index
         * @return DXGI format
         */
        DXGI_FORMAT GetFormat(int index) const;

        /**
         * @brief Set new resolution for all shadow textures
         * @param newWidth New width
         * @param newHeight New height
         * 
         * Recreates all shadow depth textures with new resolution
         */
        void SetResolution(int newWidth, int newHeight);

        /**
         * @brief Get debug info for specific shadow texture
         * @param index Shadow texture index
         * @return Debug string
         */
        std::string GetDebugInfo(int index) const;

        /**
         * @brief Get debug info for all shadow textures
         * @return Debug string
         */
        std::string GetAllInfo() const;

    private:
        // ========================================================================
        // Private Methods
        // ========================================================================

        void ValidateIndex(int index) const;
        bool IsValidIndex(int index) const;

        void CopyDepth(ID3D12GraphicsCommandList* cmdList, int srcIndex, int dstIndex);

        /**
         * @brief [RAII] Register index buffer to UniformManager for GPU upload
         * @param uniformMgr UniformManager pointer (required, validated in constructor)
         * @note Called internally by constructor - private to enforce RAII pattern
         */
        void RegisterUniform(UniformManager* uniformMgr) override;

        // ========================================================================
        // Private Members
        // ========================================================================

        std::vector<std::shared_ptr<D12DepthTexture>> m_depthTextures;
        std::vector<RenderTargetConfig>               m_configs;

        int m_baseWidth   = 0; // Base width (can be non-square)
        int m_baseHeight  = 0; // Base height (can be non-square)
        int m_activeCount = 0;

        // [NEW] Uniform registration for Shader RT Fetching
        UniformManager*             m_uniformManager = nullptr;
        ShadowTexturesIndexUniforms m_indexBuffer;
    };
} // namespace enigma::graphic
