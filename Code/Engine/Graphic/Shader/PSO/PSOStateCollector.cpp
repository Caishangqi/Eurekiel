#include "PSOStateCollector.hpp"

#include "Engine/Graphic/Target/RenderTargetBinder.hpp"

namespace enigma::graphic
{
    PSOStateCollector::CollectedState PSOStateCollector::CollectCurrentState(
        ShaderProgram*             program,
        BlendMode                  blendMode,
        DepthMode                  depthMode,
        const StencilTestDetail&   stencilDetail,
        const RasterizationConfig& rasterizationConfig,
        D3D12_PRIMITIVE_TOPOLOGY   topology,
        RenderTargetBinder*        rtBinder
    )
    {
        CollectedState state{};

        state.program             = program;
        state.blendMode           = blendMode;
        state.depthMode           = depthMode;
        state.topology            = topology;
        state.stencilDetail       = stencilDetail;
        state.rasterizationConfig = rasterizationConfig; // [NEW] Copy rasterization config

        if (rtBinder)
        {
            rtBinder->GetCurrentRTFormats(state.rtFormats.data());
            state.depthFormat = rtBinder->GetCurrentDepthFormat();
            state.rtCount     = static_cast<uint32_t>(state.rtFormats.size());
        }
        else
        {
            state.rtFormats.fill(DXGI_FORMAT_UNKNOWN);
            state.depthFormat = DXGI_FORMAT_UNKNOWN;
            state.rtCount     = 0;
        }

        return state;
    }
}
