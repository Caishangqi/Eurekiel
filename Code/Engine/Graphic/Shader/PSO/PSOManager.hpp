/**
 * @file PSOManager.hpp
 * @brief PSO Dynamic Manager - PSO caching system based on rendering state
 * @date 2025-11-01
 *
 * Responsibilities:
 * 1. Dynamically create and cache PSO (based on ShaderProgram + RenderState)
 * 2. Solve the problem of RT format hard coding (support runtime RT format)
 * 3. Integrate SetBlendMode/SetDepthMode status management
 * 4. Provide GetOrCreatePSO unified interface
 *
 *Design decisions:
 * - PSOKey uses the ShaderProgram pointer (simplifying the design and avoiding Hash conflicts)
 * - Use std::unordered_map to cache PSO (O(1) lookup)
 * - 16-byte alignment optimization (Cache-Friendly)
 */

#pragma once

#include "Engine/Graphic/Core/RenderState.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <cstddef>

namespace enigma::graphic
{
    class ShaderProgram;
    class VertexLayout; // [NEW] Forward declaration for VertexLayout
#pragma warning(push)
#pragma warning(disable: 4324)
    /**
     * @struct PSOKey
     * @brief PSO cache key - uniquely identifies a PSO configuration
     *
     * Teaching points:
     * - Use ShaderProgram pointer instead of Hash (simplified design)
     * - Contains all states affecting PSO (RT format, mix, depth)
     * - 16-byte alignment optimizes Cache performance
     *
     * Architecture correspondence:
     * ```
     * Iris DirectX 12
     *----------------------------------------
     * Program + State → PSOKey
     * glUseProgram() → GetOrCreatePSO()
     * ```
     */
    struct alignas(16) PSOKey
    {
        const ShaderProgram* shaderProgram       = nullptr; ///< shader program pointer
        const VertexLayout*  vertexLayout        = nullptr; ///< [NEW] Vertex layout pointer
        DXGI_FORMAT          rtFormats[8]        = {}; ///< 8 RT formats (corresponding to colortex0-7)
        DXGI_FORMAT          depthFormat         = DXGI_FORMAT_UNKNOWN; ///< depth format
        BlendConfig          blendConfig         = BlendConfig::Opaque(); ///< [REFACTORED] blend configuration (replaces BlendMode)
        DepthConfig          depthConfig         = DepthConfig::Enabled(); ///< [REFACTORED] depth configuration (replaces DepthMode)
        StencilTestDetail    stencilDetail       = StencilTestDetail::Disabled(); ///< Stencil test configuration
        RasterizationConfig  rasterizationConfig = RasterizationConfig::CullBack(); ///< Rasterization configuration
        /**
         * @brief equality comparison operator
         * @param other another PSOKey
         * @return equal returns true
         */
        bool operator==(const PSOKey& other) const;

        /**
         * @brief Calculate Hash value
         * @return Hash value
         *
         * Teaching points:
         * - Use std::hash to combine multiple fields
         * - Direct Hash of ShaderProgram pointer (to avoid double calculation)
         */
        std::size_t GetHash() const;
    };

    /**
     * @struct PSOKeyHash
     * @brief PSOKey’s Hash function object
     *
     * Teaching points:
     * - Hash function for std::unordered_map
     * - Call PSOKey::GetHash() to implement
     */
    struct PSOKeyHash
    {
        std::size_t operator()(const PSOKey& key) const
        {
            return key.GetHash();
        }
    };

    /**
     * @class PSOManager
     * @brief PSO Dynamic Manager - unified management of PSO creation and caching
     *
     * Teaching points:
     * 1. Solve the problem of RT format hard coding (dynamically create PSO at runtime)
     * 2. Integrate SetBlendMode/SetDepthMode (state-driven PSO)
     * 3. Caching mechanism to avoid repeated creation (performance optimization)
     * 4. Comply with Iris architecture (Program + State → PSO)
     *
     * Usage example:
     * ```cpp
     * // 1. Set rendering state
     * SetBlendMode(BlendMode::Alpha);
     * SetDepthMode(DepthMode::ReadOnly);
     *
     * // 2. Get or create PSO
     * auto* pso = psoManager.GetOrCreatePSO(
     * shaderProgram,
     * rtFormats,
     * depthFormat
     * );
     *
     * // 3. Use PSO
     * commandList->SetPipelineState(pso);
     * ```
     */
    class PSOManager
    {
    public:
        PSOManager()  = default;
        ~PSOManager() = default;

        // Disable copying
        PSOManager(const PSOManager&)            = delete;
        PSOManager& operator=(const PSOManager&) = delete;

        // Support mobile
        PSOManager(PSOManager&&) noexcept            = default;
        PSOManager& operator=(PSOManager&&) noexcept = default;

        /**
         * @brief Get or create PSO
         * @param shaderProgram shader program
         * @param rtFormats RT format array (8 pieces)
         * @param depthFormat depth format
         * @param blendMode blending mode
         * @param depthMode depth mode
         * @return PSO pointer (returns nullptr on failure)
         *
         * Teaching points:
         * 1. Construct PSOKey search cache
         * 2. Cache hit returns directly
         * 3. Call CreatePSO to create a cache miss.
         * 4. New PSO added to cache
         */
        ID3D12PipelineState* GetOrCreatePSO(
            const ShaderProgram*       shaderProgram,
            const VertexLayout*        layout, // [NEW] Vertex layout
            const DXGI_FORMAT          rtFormats[8],
            DXGI_FORMAT                depthFormat,
            const BlendConfig&         blendConfig, // [REFACTORED] BlendConfig replaces BlendMode
            const DepthConfig&         depthConfig, // [REFACTORED] DepthConfig replaces DepthMode
            const StencilTestDetail&   stencilDetail,
            const RasterizationConfig& rasterizationConfig
        );

        /**
         * @brief Clear PSO cache
         *
         * Teaching points:
         * - Release all cached PSOs
         * - for hot reloading or resource cleanup
         */
        void ClearCache();

    private:
        /**
         * @brief Create a new PSO
         * @param key PSO key
         * @return PSO ComPtr (returns nullptr on failure)
         *
         * Teaching points:
         * 1. Configure PSO descriptor according to PSOKey
         * 2. Set RT format (from key.rtFormats)
         * 3. Set blend mode (from key.blendMode)
         * 4. Set depth mode (from key.depthMode)
         * 5. Call D3D12Device::CreateGraphicsPipelineState
         *
         * Note: ShaderProgram is required to provide shader bytecode access methods
         */
        Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const PSOKey& key);

        /**
         * @brief Configure blend state
         * @param blendDesc blend descriptor
         * @param blendConfig blend configuration
         */
        static void ConfigureBlendState(D3D12_BLEND_DESC& blendDesc, const BlendConfig& blendConfig);

        /**
         * @brief configure depth stencil state
         * @param depthStencilDesc depth stencil descriptor
         * @param depthConfig depth configuration (replaces DepthMode)
         * @param stencilDetail Stencil test configuration
         */
        static void ConfigureDepthStencilState(
            D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
            const DepthConfig&        depthConfig,
            const StencilTestDetail&  stencilDetail
        );

        /**
         * @brief configure rasterization status
         * @param rasterDesc rasterization descriptor
         * @param config Rasterization configuration
         */
        static void ConfigureRasterizerState(D3D12_RASTERIZER_DESC& rasterDesc, const RasterizationConfig& config);

    private:
        /// PSO cache (Key to PSO)
        std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOKeyHash> m_psoCache;
    };
} // namespace enigma::graphic
