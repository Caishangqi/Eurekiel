#include "StencilHelper.hpp"

namespace enigma::graphic
{
    void StencilHelper::ConfigureStencilState(
        D3D12_DEPTH_STENCIL_DESC& desc,
        const StencilTestDetail&  detail)
    {
        // [STEP 1] Enable/disable stencil testing
        desc.StencilEnable = detail.enable;

        // [STEP 2] Configure read/write masks
        // ReadMask: Applied when reading stencil values for comparison
        // WriteMask: Applied when writing new stencil values
        desc.StencilReadMask  = detail.stencilReadMask;
        desc.StencilWriteMask = detail.stencilWriteMask;

        // [STEP 3] Configure front face stencil operations
        // Front faces are triangles facing the camera (counterclockwise winding order)
        desc.FrontFace.StencilFunc        = detail.stencilFunc; // Comparison function (e.g., Equal, Always)
        desc.FrontFace.StencilPassOp      = detail.stencilPassOp; // Operation when both stencil and depth tests pass
        desc.FrontFace.StencilFailOp      = detail.stencilFailOp; // Operation when stencil test fails
        desc.FrontFace.StencilDepthFailOp = detail.stencilDepthFailOp; // Operation when stencil passes but depth fails

        // [STEP 4] Configure back face stencil operations
        // Back faces are triangles facing away from camera (clockwise winding order)
        if (detail.useSeparateFrontBack)
        {
            // Use separate configuration for back faces
            // Useful for: Shadow volumes, two-sided stencil algorithms
            desc.BackFace.StencilFunc        = detail.backFaceStencilFunc;
            desc.BackFace.StencilPassOp      = detail.backFaceStencilPassOp;
            desc.BackFace.StencilFailOp      = detail.backFaceStencilFailOp;
            desc.BackFace.StencilDepthFailOp = detail.backFaceStencilDepthFailOp;
        }
        else
        {
            // Mirror front face configuration to back face
            // Most common case: Same stencil behavior for both sides
            desc.BackFace = desc.FrontFace;
        }

        // [STEP 5] Handle depth write control
        // Some stencil operations need to disable depth writes (e.g., outline rendering, shadow volumes)
        // This allows stencil-only operations without affecting depth buffer
        if (!detail.depthWriteEnable)
        {
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        }
        // Note: If depthWriteEnable == true, we don't modify DepthWriteMask
        // The caller or DepthMode system is responsible for setting it to ALL if needed

        // [IMPORTANT] We do NOT modify:
        // - desc.DepthEnable: Controlled by DepthMode system
        // - desc.DepthFunc: Controlled by DepthMode system
        // Modifying these would cause conflicts with depth testing configuration
    }
} // namespace enigma::graphic
