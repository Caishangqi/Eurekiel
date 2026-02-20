/**
 * @file RenderTargetBinder.hpp
 * @brief [REFACTOR] Unified render target binder with Provider-based API
 *
 * Design:
 * 1. Aggregates 4 Providers (ColorTexture, DepthTexture, ShadowColor, ShadowTexture)
 * 2. Unified BindRenderTargets() API with pair-based targets
 * 3. State caching to avoid redundant OMSetRenderTargets calls (target: 70%+ reduction)
 * 4. Deferred binding - FlushBindings() actually calls D3D12 API
 * 5. Depth binding validation (DX12 allows only one depth buffer per pass)
 *
 * Breaking Changes:
 * - Old BindRenderTargets(vector<RTType>, vector<int>, ...) removed
 * - Convenience APIs (BindGBuffer, BindShadowPass, BindCompositePass) removed
 * - Constructor now takes IRenderTargetProvider* instead of Manager*
 */

#pragma once

#include <vector>
#include <utility>
#include <cstdint>
#include <d3d12.h>
#include "RTTypes.hpp"
#include "RenderTargetProviderException.hpp"

namespace enigma::graphic
{
    // Forward declarations
    class IRenderTargetProvider;
    class ShadowTextureProvider;
    class DepthTextureProvider;

    /**
     * @class RenderTargetBinder
     * @brief Unified render target binder with Provider-based architecture
     *
     * Core responsibilities:
     * 1. Provider aggregation - unified management of 4 providers
     * 2. Unified binding interface - BindRenderTargets() with pair-based targets
     * 3. State caching - avoid redundant OMSetRenderTargets calls
     * 4. Deferred binding - batch submit to reduce API calls
     * 5. Depth binding validation - DX12 only allows one depth buffer per pass
     *
     * Usage:
     * @code
     * // Bind GBuffer (4 ColorTex + 1 DepthTex)
     * binder->BindRenderTargets({
     *     {RTType::ColorTex, 0}, {RTType::ColorTex, 1},
     *     {RTType::ColorTex, 2}, {RTType::ColorTex, 3},
     *     {RTType::DepthTex, 0}
     * });
     * binder->FlushBindings(cmdList);
     *
     * // Shadow pass: shadowcolor + shadowtex
     * binder->BindRenderTargets({
     *     {RTType::ShadowColor, 0},
     *     {RTType::ShadowTex, 0}
     * });
     * @endcode
     */
    class RenderTargetBinder
    {
    public:
        /**
         * @brief Construct RenderTargetBinder with 4 providers
         * @param colorProvider ColorTextureProvider (manages ColorTex 0-15)
         * @param depthProvider DepthTextureProvider (manages DepthTex 0-2)
         * @param shadowColorProvider ShadowColorProvider (manages ShadowColor 0-7)
         * @param shadowTexProvider ShadowTextureProvider (manages ShadowTex 0-1)
         *
         * Note: Raw pointers used - providers owned by RendererSubsystem
         */
        RenderTargetBinder(
            IRenderTargetProvider* colorProvider,
            IRenderTargetProvider* depthProvider,
            IRenderTargetProvider* shadowColorProvider,
            IRenderTargetProvider* shadowTexProvider
        );

        ~RenderTargetBinder() = default;

        // Non-copyable, non-movable (typically used as singleton)
        RenderTargetBinder(const RenderTargetBinder&)            = delete;
        RenderTargetBinder& operator=(const RenderTargetBinder&) = delete;
        RenderTargetBinder(RenderTargetBinder&&)                 = delete;
        RenderTargetBinder& operator=(RenderTargetBinder&&)      = delete;

        // ========================================================================
        // Unified Binding Interface
        // ========================================================================

        /**
         * @brief Bind render targets (new pair-based API)
         * @param targets Target list, each element is {RTType, index} pair
         *
         * DX12 constraint: only one depth buffer per pass
         * - ShadowTex and DepthTex are both depth textures, cannot bind together
         * - System auto-identifies depth type from targets
         *
         * @throws InvalidBindingException when:
         *   - Both ShadowTex and DepthTex bound (dual depth binding)
         *   - Multiple ShadowTex bound
         *   - Multiple DepthTex bound
         *
         * @example
         * // Shadow camera: only shadowtex0 as depth
         * binder->BindRenderTargets({{RenderTargetType::ShadowTex, 0}});
         *
         * // Player camera: 4 colortex + 1 depthtex
         * binder->BindRenderTargets({
         *     {RenderTargetType::ColorTex, 0}, {RenderTargetType::ColorTex, 1},
         *     {RenderTargetType::ColorTex, 2}, {RenderTargetType::ColorTex, 3},
         *     {RenderTargetType::DepthTex, 0}
         * });
         */
        void BindRenderTargets(const std::vector<std::pair<RenderTargetType, int>>& targets);

        /**
         * @brief Get provider by RTType
         * @param rtType RT type
         * @return Provider pointer
         * @throws std::invalid_argument for unknown RTType
         */
        IRenderTargetProvider* GetProvider(RenderTargetType rtType);

        /**
         * @brief Clear all bindings (unbind all RTs)
         */
        void ClearBindings();

        // ========================================================================
        // State Management Interface
        // ========================================================================

        /**
         * @brief Flush binding state, actually call OMSetRenderTargets
         * @param cmdList Command list pointer
         *
         * Performance optimization:
         * - Hash-based fast comparison: O(n), n = RTV count
         * - Early exit: zero overhead when state unchanged
         */
        void FlushBindings(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief Force flush bindings (skip hash check)
         * @param cmdList Command list pointer
         *
         * Use cases:
         * - External state change (e.g., ResizeBuffers)
         * - Debug to ensure correct state
         */
        void ForceFlushBindings(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief Check if there are pending state changes
         * @return Whether FlushBindings needs to be called
         */
        bool HasPendingChanges() const;

        /**
         * @brief Get current bound RT formats
         * @param outFormats Output format array (8 elements)
         */
        void GetCurrentRTFormats(DXGI_FORMAT outFormats[8]) const;

        /**
         * @brief Get current depth format
         * @return Depth format
         */
        DXGI_FORMAT GetCurrentDepthFormat() const;

        // ========================================================================
        // Backbuffer Format Override (for PresentRenderTarget draw fallback)
        // ========================================================================

        /**
         * @brief Set temporary backbuffer format override for PSO creation
         * @param format Backbuffer format (e.g., DXGI_FORMAT_R8G8B8A8_UNORM)
         *
         * When active, GetCurrentRTFormats() returns override format in slot 0
         * and DXGI_FORMAT_UNKNOWN in slots 1-7. GetCurrentDepthFormat() returns UNKNOWN.
         * Used by PresentRenderTargetWithDraw() to create correct PSO for backbuffer output.
         */
        void SetBackbufferOverride(DXGI_FORMAT format);

        /**
         * @brief Clear backbuffer format override, restore normal behavior
         */
        void ClearBackbufferOverride();

    private:
        // ========================================================================
        // Internal Implementation
        // ========================================================================

        /**
         * @brief Get DSV handle for depth provider
         * @param rtType Depth type (DepthTex or ShadowTex)
         * @param index Depth texture index
         * @return DSV handle
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(RenderTargetType rtType, int index) const;

        /**
         * @brief Perform clear operations (internal)
         * @param cmdList Command list pointer
         */
        void PerformClearOperations(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief Compute state hash for cache comparison
         * @return Hash value
         */
        uint32_t ComputeStateHash() const;

        // ========================================================================
        // Internal Data Structures
        // ========================================================================

        /**
         * @brief Binding state structure
         */
        struct BindingState
        {
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles; ///< RTV handle array
            std::vector<ClearValue>                  clearValues; ///< RTV clear values
            D3D12_CPU_DESCRIPTOR_HANDLE              dsvHandle; ///< DSV handle
            ClearValue                               depthClearValue; ///< DSV clear value
            LoadAction                               loadAction; ///< Load action
            uint32_t                                 stateHash; ///< State hash

            BindingState()
                : dsvHandle{0}
                  , depthClearValue(ClearValue::Depth(1.0f, 0))
                  , loadAction(LoadAction::Load)
                  , stateHash(0)
            {
            }

            uint32_t ComputeHash() const
            {
                uint32_t hash = 0;
                for (const auto& rtv : rtvHandles)
                {
                    hash ^= static_cast<uint32_t>(rtv.ptr & 0xFFFFFFFF);
                }
                hash ^= static_cast<uint32_t>(dsvHandle.ptr & 0xFFFFFFFF);
                return hash;
            }

            void Reset()
            {
                rtvHandles.clear();
                clearValues.clear();
                dsvHandle       = D3D12_CPU_DESCRIPTOR_HANDLE{0};
                depthClearValue = ClearValue::Depth(1.0f, 0);
                loadAction      = LoadAction::Load;
                stateHash       = 0;
            }
        };

        // ========================================================================
        // Provider Pointers (non-owning)
        // ========================================================================

        IRenderTargetProvider* m_colorProvider; ///< ColorTex provider
        IRenderTargetProvider* m_depthProvider; ///< DepthTex provider
        IRenderTargetProvider* m_shadowColorProvider; ///< ShadowColor provider
        IRenderTargetProvider* m_shadowTexProvider; ///< ShadowTex provider

        // ========================================================================
        // State Management
        // ========================================================================

        BindingState m_currentState; ///< Current committed state
        BindingState m_pendingState; ///< Pending state

        // ========================================================================
        // Cached Binding State (for new API)
        // ========================================================================

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_cachedRTVHandles; ///< Cached RTV handles
        D3D12_CPU_DESCRIPTOR_HANDLE              m_cachedDSVHandle; ///< Cached DSV handle
        bool                                     m_hasDepthBinding; ///< Has depth binding flag

        // ========================================================================
        // Performance Statistics (optional, for debug)
        // ========================================================================

        uint32_t m_totalBindCalls; ///< Total bind call count
        uint32_t m_cacheHitCount; ///< State cache hit count
        uint32_t m_actualBindCalls; ///< Actual API call count

        // ========================================================================
        // Format Cache (for state query)
        // ========================================================================

        DXGI_FORMAT m_currentRTFormats[8]; ///< Current bound RT formats
        DXGI_FORMAT m_currentDepthFormat; ///< Current bound depth format

        // ========================================================================
        // Backbuffer Override State
        // ========================================================================

        bool        m_backbufferOverrideActive = false; ///< Whether override is active
        DXGI_FORMAT m_backbufferOverrideFormat = DXGI_FORMAT_UNKNOWN; ///< Override format
    };
} // namespace enigma::graphic
