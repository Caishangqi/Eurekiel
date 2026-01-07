#pragma once

// ============================================================================
// ColorTextureProvider.hpp - [REFACTOR] Refactored from RenderTargetManager
// Implements IRenderTargetProvider for colortex0-15 management
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
     * @class ColorTextureProvider
     * @brief Manages colortex0-15 with D12RenderTarget and BufferFlipState
     * 
     * [REFACTOR] Refactored from RenderTargetManager to implement IRenderTargetProvider.
     * 
     * Features:
     * - Manages 1-16 color render targets (colortex0-15)
     * - Supports Main/Alt flip-state for ping-pong rendering
     * - RAII constructor with vector of RTConfig
     * - Bindless texture index access
     * 
     * Iris compatibility:
     * - Maps to Iris RenderTargets.java (16 colortex management)
     */
    class ColorTextureProvider : public IRenderTargetProvider
    {
    public:
        // ========================================================================
        // Constants
        // ========================================================================

        static constexpr int MAX_COLOR_TEXTURES = 16;
        static constexpr int MIN_COLOR_TEXTURES = 1;

        // ========================================================================
        // Constructor / Destructor
        // ========================================================================

        /**
         * @brief RAII constructor - creates color render targets from config
         * @param baseWidth Base screen width
         * @param baseHeight Base screen height
         * @param configs Vector of RTConfig for each colortex
         * @throws std::invalid_argument if dimensions invalid or configs empty
         */
        ColorTextureProvider(
            int                          baseWidth,
            int                          baseHeight,
            const std::vector<RTConfig>& configs
        );

        ~ColorTextureProvider() override = default;

        // Non-copyable, movable
        ColorTextureProvider(const ColorTextureProvider&)            = delete;
        ColorTextureProvider& operator=(const ColorTextureProvider&) = delete;
        ColorTextureProvider(ColorTextureProvider&&)                 = default;
        ColorTextureProvider& operator=(ColorTextureProvider&&)      = default;

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
        // Extended API (ColorTexture-specific)
        // ========================================================================

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
         * @brief Handle window resize
         * @param newWidth New base width
         * @param newHeight New base height
         */
        void OnResize(int newWidth, int newHeight);

        /**
         * @brief Generate mipmaps for all enabled RTs
         * @param cmdList Command list
         */
        void GenerateMipmaps(ID3D12GraphicsCommandList* cmdList);

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

        int m_baseWidth   = 0;
        int m_baseHeight  = 0;
        int m_activeCount = 0;
    };
} // namespace enigma::graphic
