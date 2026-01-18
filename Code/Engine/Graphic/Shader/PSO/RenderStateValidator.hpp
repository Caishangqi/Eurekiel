#pragma once

#include "Engine/Graphic/Core/RenderState.hpp"
#include <d3d12.h>
#include <array>

// [DELETE] PSOStateCollector removed - using local DrawState struct instead

namespace enigma::graphic
{
    // Forward declaration
    class ShaderProgram;

    /**
     * @class RenderStateValidator
     * @brief Render State Validator - Validates Draw() state completeness
     * 
     * Pure static helper class following shrimp-rules.md §10
     * - No state: All methods are static
     * - No instantiation: Constructor deleted
     * - Single responsibility: Validate PSO state only
     */
    class RenderStateValidator
    {
    public:
        /**
         * @brief Draw state structure for validation
         * [NEW] Local definition - replaces dependency on PSOStateCollector::CollectedState
         */
        struct DrawState
        {
            ShaderProgram*             program;
            BlendConfig                blendConfig; // [REFACTORED] BlendConfig replaces BlendMode
            DepthConfig                depthConfig; // [REFACTORED] DepthConfig replaces DepthMode
            StencilTestDetail          stencilDetail;
            RasterizationConfig        rasterizationConfig;
            D3D12_PRIMITIVE_TOPOLOGY   topology;
            std::array<DXGI_FORMAT, 8> rtFormats;
            DXGI_FORMAT                depthFormat;
            uint32_t                   rtCount;

            // [DELETE] Removed PSOStateCollector conversion constructor - no longer needed
            DrawState() = default; // Default constructor
        };

        /**
         * @brief Validate Draw() state completeness
         * @param state Draw state to validate
         * @param outErrorMessage Optional error message output
         * @return true if state is valid, false otherwise
         */
        static bool ValidateDrawState(
            const DrawState& state,
            const char**     outErrorMessage = nullptr
        );

    private:
        RenderStateValidator() = delete; // [REQUIRED] Prevent instantiation
    };
}
