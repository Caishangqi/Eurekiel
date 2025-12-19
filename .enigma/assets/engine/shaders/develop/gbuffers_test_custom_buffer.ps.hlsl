#include "../core/Common.hlsl"
#include "../include/developer_uniforms.hlsl"
/**
 * @brief 像素着色器输出结构体 - 多RT输出
 *
 * [TEMPORARY TEST] 临时测试配置 - 输出到colortex4-7
 * 正常情况下应该输出到colortex0-3 (GBuffer标准配置)
 */
struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4 - 主颜色 (Albedo)
    float4 color1 : SV_Target1; // colortex5 - 预留
    float4 color2 : SV_Target2; // colortex6 - 预留
    float4 color3 : SV_Target3; // colortex7 - 预留
};

/**
 * @brief 像素着色器主函数
 * @param input 像素输入 (PSInput from Common.hlsli)
 * @return PSOutput - 多RT输出
 *
 * 教学要点:
 * 1. 多RT输出需要定义结构体，每个成员对应一个SV_Target
 * 2. SV_Target索引对应OMSetRenderTargets绑定的RTV数组索引
 * 3. 当前配置：输出到colortex4-7 (测试用途)
 * 4. 正常配置：应该输出到colortex0-3 (GBuffer标准)
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // 采样CustomImage0纹理（使用线性采样器和UV坐标）
    // CustomImage槽位在CPU侧通过SetCustomImage(0, texture)设置
    float4 texColor = customImage0.Sample(pointSampler, input.TexCoord);

    // [NEW] Multi-draw data independence test
    // Final color = Custom Buffer color (should be Red/Green/Blue for each cube)
    // If Ring Buffer isolation fails, all cubes will show Blue (last uploaded color)
    output.color0 = texColor * input.Color * modelColor * color;

    // [TEST] Other RT outputs for debugging
    output.color1 = color; // Pure custom buffer color
    output.color2 = input.Color * modelColor;
    output.color3 = float4(1.0, 1.0, 1.0, 1.0);

    return output;
}

/**
 * Iris 注释示例:
 * RENDERTARGETS: 0
 *
 * 说明:
 * - 只写入 RT0 (colortex0)
 * - 不写入法线、深度等其他 GBuffer
 * - ShaderDirectiveParser 会解析此注释并配置 PSO
 */
