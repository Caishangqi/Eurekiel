#pragma once

// ============================================================================
// ShadowColorProvider.hpp - [NEW] Shadow color render target provider
// Implements IRenderTargetProvider for shadowcolor0-7 management
// Part of RenderTarget Manager Unified Architecture Refactoring
// ============================================================================

#include "IRenderTargetProvider.hpp"
#include "RenderTargetProviderCommon.hpp"
#include "RenderTargetProviderException.hpp"
#include "BufferFlipState.hpp"
#include "RTTypes.hpp"

#include <vector>
#include <memory>
#include <string>

namespace enigma::graphic
{
    // Forward declarations
    class D12RenderTarget;
    class D3D12RenderSystem;

    /**
     * @class ShadowColorProvider
     * @brief Manages shadowcolor0-7 with D12RenderTarget and BufferFlipState
     * 
     * [NEW] Implements IRenderTargetProvider for shadow color render targets.
     * 
     * Features:
     * - Manages 1-8 shadow color render targets (shadowcolor0-7)
     * - Fixed resolution independent of screen size
     * - Supports Main/Alt flip-state for ping-pong rendering
     * - RAII constructor with vector of RTConfig
     * - Bindless texture index access
     * 
     * Iris compatibility:
     * - Maps to Iris shadow color buffers for shadow mapping
     */
    class ShadowColorProvider : public IRenderTargetProvider
    {
    public:
        // ========================================================================
        // Constants
        // ========================================================================

        static constexpr int MAX_SHADOW_COLORS = 8;
        static constexpr int MIN_SHADOW_COLORS = 1;

        // ========================================================================
        // Constructor / Destructor
        // ========================================================================

        /**
         * @brief RAII constructor - creates shadow color render targets from config
         * @param baseWidth Base width (for interface consistency, shadow uses fixed resolution from config)
         * @param baseHeight Base height (for interface consistency, shadow uses fixed resolution from config)
         * @param configs Vector of RTConfig for each shadowcolor (resolution from config)
         * @throws std::invalid_argument if configs empty or invalid
         * 
         * Note: Shadow textures use fixed resolution from config, not screen-dependent.
         * Resolution is taken from configs[0].width/height (typically square).
         */
        ShadowColorProvider(
            int                          baseWidth,
            int                          baseHeight,
            const std::vector<RTConfig>& configs
        );

        ~ShadowColorProvider() override = default;

        // Non-copyable, movable
        ShadowColorProvider(const ShadowColorProvider&)            = delete;
        ShadowColorProvider& operator=(const ShadowColorProvider&) = delete;
        ShadowColorProvider(ShadowColorProvider&&)                 = default;
        ShadowColorProvider& operator=(ShadowColorProvider&&)      = default;

        // ========================================================================
        // IRenderTargetProvider Implementation
        // ========================================================================

        // Core Operations
        void Copy(int srcIndex, int dstIndex) override;
        void Clear(int index, const float* clearValue) override;

        // RTV/DSV Access
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) override;
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) override;

        // Resource Access
        ID3D12Resource* GetMainResource(int index) override;
        ID3D12Resource* GetAltResource(int index) override;

        // Bindless Index Access
        uint32_t GetMainTextureIndex(int index) override;
        uint32_t GetAltTextureIndex(int index) override;

        // FlipState Management
        void Flip(int index) override;
        void FlipAll() override;
        void Reset() override;

        // Metadata
        int GetCount() const override { return m_activeCount; }

        // Capability Query
        bool SupportsFlipState() const override { return true; }
        bool SupportsDSV() const override { return false; }

        // Dynamic Configuration
        void SetRtConfig(int index, const RTConfig& config) override;

        // ========================================================================
        // Extended API (ShadowColor-specific)
        // ========================================================================

        /**
         * @brief Get shadow base width
         * @return Width in pixels
         */
        int GetBaseWidth() const { return m_baseWidth; }

        /**
         * @brief Get shadow base height
         * @return Height in pixels
         */
        int GetBaseHeight() const { return m_baseHeight; }

        /**
         * @brief Get render target format at index
         * @param index RT index
         * @return DXGI_FORMAT
         */
        DXGI_FORMAT GetFormat(int index) const;

        /**
         * @brief Get underlying D12RenderTarget
         * @param index RT index
         * @return Shared pointer to D12RenderTarget
         */
        std::shared_ptr<D12RenderTarget> GetRenderTarget(int index) const;

        /**
         * @brief Check if index is flipped
         * @param index RT index
         * @return true if flipped (read Alt, write Main)
         */
        bool IsFlipped(int index) const;

        /**
         * @brief Change shadow resolution for all targets
         * @param newWidth New width
         * @param newHeight New height
         */
        void SetResolution(int newWidth, int newHeight);

        /**
         * @brief Get debug info for specific RT
         * @param index RT index
         * @return Debug string
         */
        std::string GetDebugInfo(int index) const;

        /**
         * @brief Get overview of all RTs
         * @return Debug string
         */
        std::string GetAllInfo() const;

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

        // ========================================================================
        // Private Members
        // ========================================================================

        std::vector<std::shared_ptr<D12RenderTarget>> m_renderTargets;
        std::vector<RTConfig>                         m_configs;
        RenderTargetFlipState                         m_flipState;

        int m_baseWidth   = 0; // Base width (can be non-square)
        int m_baseHeight  = 0; // Base height (can be non-square)
        int m_activeCount = 0;
    };
} // namespace enigma::graphic
