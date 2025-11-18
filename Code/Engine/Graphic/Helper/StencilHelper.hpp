#pragma once

#include <d3d12.h>
#include "Engine/Graphic/Core/RenderState.hpp"

namespace enigma::graphic
{
    /**
     * @class StencilHelper
     * @brief Pure static utility class for stencil state configuration
     * 
     * @details
     * Converts StencilTestDetail to D3D12_DEPTH_STENCIL_DESC stencil fields.
     * This class is non-instantiable and stateless, following the Helper class design pattern.
     * 
     * Design Pattern:
     * - Pure static methods only
     * - No state storage (no member variables)
     * - Deleted constructors to prevent instantiation
     * - Single responsibility: Stencil state conversion
     * 
     * Responsibilities:
     * - Configure D3D12_DEPTH_STENCIL_DESC stencil-related fields
     * - Handle front/back face stencil operation differences
     * - Manage depth write mask based on stencil configuration
     * 
     * Non-Responsibilities:
     * - Does NOT modify DepthEnable or DepthFunc (managed by DepthMode system)
     * - Does NOT manage stencil buffer creation or lifecycle
     * - Does NOT cache or store state
     * 
     * @note Reference BufferHelper for pure static class design pattern
     */
    class StencilHelper
    {
    public:
        StencilHelper()                                = delete; // Prevent instantiation
        StencilHelper(const StencilHelper&)            = delete; // Prevent copy
        StencilHelper& operator=(const StencilHelper&) = delete; // Prevent assignment

        /**
         * @brief Configure D3D12_DEPTH_STENCIL_DESC stencil section
         * @param desc Target depth-stencil descriptor to modify
         * @param detail Stencil test configuration from application
         * 
         * @details
         * Configures the following D3D12_DEPTH_STENCIL_DESC fields:
         * - StencilEnable: Enable/disable stencil testing
         * - StencilReadMask: Bitmask for reading stencil values
         * - StencilWriteMask: Bitmask for writing stencil values
         * - FrontFace.StencilFunc: Front face comparison function
         * - FrontFace.StencilPassOp: Front face pass operation
         * - FrontFace.StencilFailOp: Front face fail operation
         * - FrontFace.StencilDepthFailOp: Front face depth fail operation
         * - BackFace.*: Back face operations (mirrors front or uses separate config)
         * - DepthWriteMask: Optionally disabled based on depthWriteEnable flag
         * 
         * Front/Back Face Logic:
         * - If detail.useSeparateFrontBack == false: BackFace = FrontFace (mirror)
         * - If detail.useSeparateFrontBack == true: BackFace uses separate configuration
         * 
         * Depth Write Interaction:
         * - If detail.depthWriteEnable == false: Sets DepthWriteMask to ZERO
         * - Otherwise: Leaves DepthWriteMask unchanged
         * 
         * @warning
         * This function does NOT modify:
         * - DepthEnable (managed by DepthMode system)
         * - DepthFunc (managed by DepthMode system)
         * 
         * Modifying these fields would cause conflicts with the depth testing system.
         * Only DepthWriteMask is conditionally modified based on stencil configuration.
         * 
         * @note
         * Call this function after initializing D3D12_DEPTH_STENCIL_DESC to default values.
         * It only modifies stencil-related fields, leaving depth test configuration intact.
         */
        static void ConfigureStencilState(
            D3D12_DEPTH_STENCIL_DESC& desc,
            const StencilTestDetail&  detail);
    };
} // namespace enigma::graphic
