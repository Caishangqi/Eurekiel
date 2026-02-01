#pragma once

// ============================================================================
// DepthTextureProvider.hpp - [REFACTOR] Refactored from DepthTextureManager
// Implements IRenderTargetProvider for depthtex0-2 management
// Part of RenderTarget Manager Unified Architecture Refactoring
// ============================================================================

#include "IRenderTargetProvider.hpp"
#include "RenderTargetProviderCommon.hpp"
#include "RenderTargetProviderException.hpp"
#include "RTTypes.hpp"
#include "D12DepthTexture.hpp"
#include "Engine/Graphic/Shader/Uniform/DepthTextureIndexUniforms.hpp"

#include <vector>
#include <memory>
#include <string>

namespace enigma::graphic
{
    // Forward declarations
    class D3D12RenderSystem;
    class UniformManager;

    /**
     * @class DepthTextureProvider
     * @brief Manages depthtex0-2 with D12DepthTexture, no FlipState support
     * 
     * [REFACTOR] Refactored from DepthTextureManager to implement IRenderTargetProvider.
     * 
     * Features:
     * - Manages 1-3 depth textures (depthtex0-2)
     * - NO flip-state support (depth textures are single-buffered)
     * - DSV access via GetDSV() method
     * - Bindless texture index access for shader sampling
     * 
     * Iris compatibility:
     * - depthtex0: Main depth (all geometry)
     * - depthtex1: Pre-translucent depth (before TERRAIN_TRANSLUCENT)
     * - depthtex2: Pre-hand depth (before HAND_SOLID)
     */
    class DepthTextureProvider : public IRenderTargetProvider
    {
    public:
        // ========================================================================
        // Constructor / Destructor
        // ========================================================================

        /**
         * @brief RAII constructor - creates depth textures from RTConfig
         * @param baseWidth Base screen width
         * @param baseHeight Base screen height
         * @param configs Vector of RTConfig for each depthtex (size determines count)
         * @param uniformMgr UniformManager pointer for Bindless index upload (required)
         * @throws std::invalid_argument if dimensions invalid, configs empty, or uniformMgr null
         * @throws std::out_of_range if configs.size() out of range [1-3]
         * 
         * @note Use RTConfig::DepthTarget() to create depth texture configs
         */
        DepthTextureProvider(
            int                          baseWidth,
            int                          baseHeight,
            const std::vector<RTConfig>& configs,
            UniformManager*              uniformMgr
        );

        ~DepthTextureProvider() override = default;

        // Non-copyable, movable
        DepthTextureProvider(const DepthTextureProvider&)            = delete;
        DepthTextureProvider& operator=(const DepthTextureProvider&) = delete;
        DepthTextureProvider(DepthTextureProvider&&)                 = default;
        DepthTextureProvider& operator=(DepthTextureProvider&&)      = default;

        // ========================================================================
        // IRenderTargetProvider Implementation
        // ========================================================================

        // Core Operations
        void Copy(int srcIndex, int dstIndex) override;
        void Clear(int index, const float* clearValue) override;

        // RTV/DSV Access - depth textures use DSV, not RTV
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) override;
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) override;

        // Resource Access
        ID3D12Resource* GetMainResource(int index) override;
        ID3D12Resource* GetAltResource(int index) override;

        // Bindless Index Access
        uint32_t GetMainTextureIndex(int index) override;
        uint32_t GetAltTextureIndex(int index) override;

        // FlipState Management - no-op for depth textures
        void Flip(int index) override;
        void FlipAll() override;
        void Reset() override;

        // Metadata
        int GetCount() const override { return m_activeCount; }

        /**
         * @brief Get depth texture format at index
         * @param index Depth texture index [0-2]
         * @return DXGI_FORMAT (typically DXGI_FORMAT_D32_FLOAT or DXGI_FORMAT_D24_UNORM_S8_UINT)
         * @throws InvalidIndexException if index out of bounds
         * 
         * [FIX] Added to implement IRenderTargetProvider::GetFormat for PSO creation.
         */
        DXGI_FORMAT GetFormat(int index) const override;

        // Capability Query
        bool SupportsFlipState() const override { return false; }
        bool SupportsDSV() const override { return true; }

        // Dynamic Configuration
        void SetRtConfig(int index, const RTConfig& config) override;

        // [NEW] Reset and Config Query
        void            ResetToDefault(const std::vector<RTConfig>& defaultConfigs) override;
        const RTConfig& GetConfig(int index) const override;

        // ========================================================================
        // [NEW] Uniform Update API - Shader RT Fetching Feature
        // ========================================================================

        /**
         * @brief Update and upload bindless indices to GPU
         * @note Call after resource recreation (SetRtConfig)
         */
        void UpdateIndices() override;

        // ========================================================================
        // Extended API (DepthTexture-specific)
        // ========================================================================

        /**
         * @brief Get DSV handle for depth binding
         * @param index Depth texture index [0-2]
         * @return CPU descriptor handle for DSV
         * @throws InvalidIndexException if index out of bounds
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(int index) const;

        /**
         * @brief Get underlying D12DepthTexture
         * @param index Depth texture index
         * @return Shared pointer to D12DepthTexture
         */
        std::shared_ptr<D12DepthTexture> GetDepthTexture(int index) const;

        /**
         * @brief Handle window resize
         * @param newWidth New base width
         * @param newHeight New base height
         */
        void OnResize(int newWidth, int newHeight);

        /**
         * @brief Get debug info for all depth textures
         * @return Debug string
         */
        std::string GetDebugInfo() const;

    private:
        // ========================================================================
        // Private Methods
        // ========================================================================

        /**
         * @brief Validate index and throw InvalidIndexException if out of bounds
         * @param index Index to validate
         * @throws InvalidIndexException if index invalid
         */
        void ValidateIndex(int index) const;

        /**
         * @brief Check if index is valid (no throw)
         * @param index Index to check
         * @return true if valid
         */
        bool IsValidIndex(int index) const;

        /**
         * @brief Internal copy with command list
         * @param cmdList Command list
         * @param srcIndex Source index
         * @param dstIndex Destination index
         */
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
        std::vector<RTConfig>                         m_configs;

        int m_baseWidth   = 0;
        int m_baseHeight  = 0;
        int m_activeCount = 0;

        // [NEW] Uniform registration for Shader RT Fetching
        UniformManager*           m_uniformManager = nullptr;
        DepthTextureIndexUniforms m_indexBuffer;
    };
} // namespace enigma::graphic
