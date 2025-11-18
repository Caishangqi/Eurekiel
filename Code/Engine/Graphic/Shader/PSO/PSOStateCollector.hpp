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
     * Pure static helper class following shrimp-rules.md §10
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
            ShaderProgram*              program;
            BlendMode                   blendMode;
            DepthMode                   depthMode;
            StencilTestDetail           stencilDetail;
            D3D12_PRIMITIVE_TOPOLOGY    topology;
            std::array<DXGI_FORMAT, 8>  rtFormats;
            DXGI_FORMAT                 depthFormat;
            uint32_t                    rtCount;
        };

        /**
         * @brief Collect current PSO state from renderer and RT binder
         * @param program Current shader program
         * @param blendMode Current blend mode
         * @param depthMode Current depth mode
         * @param stencilDetail Complete stencil test configuration
         * @param topology Current primitive topology
         * @param rtBinder Current render target binder
         * @return Collected PSO state
         */
        static CollectedState CollectCurrentState(
            ShaderProgram*               program,
            BlendMode                    blendMode,
            DepthMode                    depthMode,
            const StencilTestDetail&     stencilDetail,
            D3D12_PRIMITIVE_TOPOLOGY     topology,
            RenderTargetBinder*          rtBinder
        );

    private:
        PSOStateCollector() = delete;  // [REQUIRED] Prevent instantiation
    };
}
