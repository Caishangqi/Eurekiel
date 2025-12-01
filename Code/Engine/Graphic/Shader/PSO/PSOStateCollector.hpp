#pragma once

#include "Engine/Graphic/Core/RenderState.hpp"
#include <d3d12.h>
#include <array>

namespace enigma::graphic
{
    // Forward declarations
    class ShaderProgram;
    class RenderTargetBinder;

    /**
     * @class PSOStateCollector
     * @brief PSO State Collector - Collects all PSO state at Draw() time
     * 
     * [DELETE] This class is a candidate for removal in future refactoring.
     * Reason: Unnecessary abstraction layer. All parameters come from RendererSubsystem
     * and are immediately extracted from the returned struct. The collection logic
     * can be inlined directly in Draw/DrawIndexed/DrawInstanced methods.
     * 
     * Pure static helper class following shrimp-rules.md section 10
     * - No state: All methods are static
     * - No instantiation: Constructor deleted
     * - Single responsibility: Collect PSO state only
     */
    class PSOStateCollector
    {
    public:
        /**
         * @brief Collected PSO state structure
         */
        struct CollectedState
        {
            ShaderProgram*             program;
            BlendMode                  blendMode;
            DepthMode                  depthMode;
            StencilTestDetail          stencilDetail;
            RasterizationConfig        rasterizationConfig; // [NEW] Rasterization configuration
            D3D12_PRIMITIVE_TOPOLOGY   topology;
            std::array<DXGI_FORMAT, 8> rtFormats;
            DXGI_FORMAT                depthFormat;
            uint32_t                   rtCount;
        };

        /**
         * @brief Collect current PSO state from renderer and RT binder
         * @param program Current shader program
         * @param blendMode Current blend mode
         * @param depthMode Current depth mode
         * @param stencilDetail Complete stencil test configuration
         * @param rasterizationConfig Rasterization configuration (fill mode, cull mode, etc.)
         * @param topology Current primitive topology
         * @param rtBinder Current render target binder
         * @return Collected PSO state
         */
        static CollectedState CollectCurrentState(
            ShaderProgram*             program,
            BlendMode                  blendMode,
            DepthMode                  depthMode,
            const StencilTestDetail&   stencilDetail,
            const RasterizationConfig& rasterizationConfig,
            D3D12_PRIMITIVE_TOPOLOGY   topology,
            RenderTargetBinder*        rtBinder
        );

    private:
        PSOStateCollector() = delete; // [REQUIRED] Prevent instantiation
    };
}
