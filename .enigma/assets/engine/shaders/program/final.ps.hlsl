#include "../core/core.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0 
};

PSOutput main(PSInput input)
{
    PSOutput output;
    output.color0 = colortex0.Sample(sampler0, input.TexCoord);
    return output;
}
