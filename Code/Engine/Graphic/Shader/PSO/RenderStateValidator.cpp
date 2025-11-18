#include "RenderStateValidator.hpp"

namespace enigma::graphic
{
    bool RenderStateValidator::ValidateDrawState(
        const PSOStateCollector::CollectedState& state,
        const char** outErrorMessage
    )
    {
        // [VALIDATION 1] ShaderProgram must be set
        if (!state.program)
        {
            if (outErrorMessage)
                *outErrorMessage = "ShaderProgram not set";
            return false;
        }
        
        // [VALIDATION 2] At least one render target or depth target must be bound
        // X-Ray rendering (no RT output) is valid if depth target exists
        if (state.rtCount == 0 && state.depthFormat == DXGI_FORMAT_UNKNOWN)
        {
            if (outErrorMessage)
                *outErrorMessage = "No render target or depth target bound";
            return false;
        }
        
        return true;
    }
}
