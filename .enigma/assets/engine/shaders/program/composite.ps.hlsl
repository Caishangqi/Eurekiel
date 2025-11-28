#include "../core/Common.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0 
};

PSOutput main(PSInput input)
{
    PSOutput output;

    output.color0 = float4(1, 1, 1, 1);

    return output;
}
