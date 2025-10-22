/**
 * @file TestTriangleShader.hpp
 * @brief 临时硬编码Shader用于三角形绘制测试
 * @date 2025-10-21
 *
 * TODO(M3): Replace with ShaderProgram system
 * 这是临时实现，仅用于验证PSO系统和顶点绘制流程
 * 正式实现应该使用ShaderPackManager加载.vsh/.fsh文件
 *
 * 教学要点:
 * - 硬编码Shader字符串用于快速原型验证
 * - 最小化Shader逻辑（只处理Position和Color）
 * - 屏幕空间坐标（-1到1的NDC坐标）
 */

#pragma once

namespace enigma::graphic::test
{
    /**
     * @brief 获取测试三角形顶点着色器源码
     *
     * 输入: Vertex_PCUTBN格式 (60 bytes)
     * 输出: SV_Position + Color
     *
     * 逻辑:
     * - 直接输出Position作为SV_Position（NDC坐标）
     * - 传递Color到像素着色器
     * - 忽略UV、Tangent、Bitangent、Normal（测试不需要）
     */
    inline const char* GetTestTriangleVS()
    {
        return R"(
// ========================================
// Test Triangle Vertex Shader
// Target: SM 6.6
// ========================================

struct VSInput
{
    float3 position  : POSITION;   // Vec3 (12 bytes, offset 0)
    float4 color     : COLOR;      // Rgba8 -> float4 (4 bytes, offset 12)
    float2 uv        : TEXCOORD0;  // Vec2 (8 bytes, offset 16)
    float3 tangent   : TANGENT;    // Vec3 (12 bytes, offset 24)
    float3 bitangent : BINORMAL;   // Vec3 (12 bytes, offset 36)
    float3 normal    : NORMAL;     // Vec3 (12 bytes, offset 48)
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // 直接输出屏幕空间坐标（NDC坐标，范围-1到1）
    output.position = float4(input.position, 1.0f);

    // 传递顶点颜色
    output.color = input.color;

    return output;
}
)";
    }

    /**
     * @brief 获取测试三角形像素着色器源码
     *
     * 输入: Color
     * 输出: SV_Target (RGBA8)
     *
     * 逻辑:
     * - 直接返回插值后的颜色
     */
    inline const char* GetTestTrianglePS()
    {
        return R"(
// ========================================
// Test Triangle Pixel Shader
// Target: SM 6.6
// ========================================

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

float4 PSMain(PSInput input) : SV_Target
{
    // 直接返回顶点颜色（已经过光栅化插值）
    return input.color;
}
)";
    }
} // namespace enigma::graphic::test
