/**
 * @file Common.hlsli
 * @brief 全局 HLSL 定义 - 固定布局 + Bindless 访问
 * @date 2025-10-03
 *
 * 教学要点:
 * 1. 所有着色器必须 #include "Common.hlsli"
 * 2. 固定 Input Layout - 全局统一顶点格式
 * 3. Root Constants 布局 - 与 C++ BindlessRootConstants 完全一致
 * 4. Bindless 访问 - ResourceDescriptorHeap[index] 直接访问
 * 5. Shader Model 6.6 语法 - 使用 HLSL 2021
 */

// ============================================================================
// Root Constants 定义 (8 bytes = 2 DWORDs)
// ============================================================================

/**
 * @brief Root Constants - 极简 Bindless 架构
 *
 * 教学要点:
 * 1. 只保留 2 个 uint32_t 索引值 (总共 8 bytes)
 * 2. register(b0, space0) 对应 Root Parameter 0
 * 3. uniformBufferIndex: 指向 UniformBuffer 的描述符索引
 * 4. renderTargetIndicesBase: 指向 RenderTarget 索引数组的描述符索引
 *
 * 架构优势:
 * - Root Signature 极简 - 只需要 8 bytes Root Constants
 * - 所有其他数据 (MVP 矩阵, 深度纹理等) 都在 UniformBuffer 中
 * - 符合真正的 Bindless 架构 - 资源通过索引间接访问
 *
 * C++ 对应:
 * ```cpp
 * struct RootConstants {
 *     uint32_t uniformBufferIndex;
 *     uint32_t renderTargetIndicesBase;
 * };
 * ```
 */
cbuffer RootConstants : register(b0, space0)
{
    uint uniformBufferIndex; // UniformBuffer 索引 (包含所有 per-frame 数据)
    uint renderTargetIndicesBase; // RenderTarget 索引数组基址
};

// ============================================================================
// 固定 Input Layout (顶点格式)
// ============================================================================

/**
 * @brief 顶点着色器输入结构体
 *
 * 教学要点:
 * 1. 固定顶点格式 - 所有几何体使用相同布局
 * 2. 对应 C++ enigma::graphic::Vertex (即 Vertex_PCUTBN)
 * 3. 必须与 C++ 顶点格式内存布局完全一致
 *
 * 布局 (对应 Vertex_PCUTBN):
 * - POSITION: float3 (Vec3 m_position) - 12 bytes, offset 0
 * - COLOR0: uint (Rgba8 m_color) - 4 bytes, offset 12
 * - TEXCOORD0: float2 (Vec2 m_uvTexCoords) - 8 bytes, offset 16
 * - TANGENT: float3 (Vec3 m_tangent) - 12 bytes, offset 24
 * - BITANGENT: float3 (Vec3 m_bitangent) - 12 bytes, offset 36
 * - NORMAL: float3 (Vec3 m_normal) - 12 bytes, offset 48
 * 总大小: 60 bytes/顶点
 */
struct VSInput
{
    float3 Position : POSITION; // 顶点位置 (世界空间)
    uint   Color : COLOR0; // 顶点颜色 (RGBA8 打包)
    float2 TexCoord : TEXCOORD0; // 纹理坐标
    float3 Tangent : TANGENT; // 切线向量
    float3 Bitangent : BITANGENT; // 副切线向量
    float3 Normal : NORMAL; // 法线向量
};

/**
 * @brief 顶点着色器输出 / 像素着色器输入
 *
 * 教学要点:
 * 1. SV_POSITION - 系统语义，裁剪空间位置
 * 2. Color 已解包为 float4 (在顶点着色器中处理)
 * 3. 其他数据自动插值传递到像素着色器
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // 裁剪空间位置 (MVP 变换后)
    float4 Color : COLOR0; // 顶点颜色 (解包后的 RGBA, 插值)
    float2 TexCoord : TEXCOORD0; // 纹理坐标 (插值)
    float3 Tangent : TANGENT; // 切线向量 (插值)
    float3 Bitangent : BITANGENT; // 副切线向量 (插值)
    float3 Normal : NORMAL; // 法线向量 (插值)
    float3 WorldPos : TEXCOORD1; // 世界空间位置 (用于光照)
};

// 像素着色器输入 (与 VSOutput 相同)
typedef VSOutput PSInput;

// ============================================================================
// UniformBuffer 结构体定义
// ============================================================================

/**
 * @brief Iris 内置 uniform 变量
 *
 * 教学要点:
 * 1. 对应 Iris 的 CommonUniforms.java
 * 2. 存储在 StructuredBuffer 中，通过 uniformBufferIndex 访问
 * 3. 每帧更新一次，所有着色器共享
 * 4. 包含所有 per-frame 数据 (MVP 矩阵, 深度纹理索引等)
 */
struct UniformBuffer
{
    // ========================================================================
    // 矩阵数据 (从 Root Constants 移入)
    // ========================================================================

    float4x4 gbufferModelView; // 模型-视图矩阵
    float4x4 gbufferProjection; // 投影矩阵
    float4x4 gbufferModelViewInverse; // 模型-视图逆矩阵
    float4x4 gbufferProjectionInverse; // 投影逆矩阵
    float4x4 gbufferMVP; // MVP 组合矩阵 (常用)

    // ========================================================================
    // 纹理索引 (从 Root Constants 移入)
    // ========================================================================

    uint depthtex0Index; // 深度纹理 0
    uint depthtex1Index; // 深度纹理 1
    uint shadowtex0Index; // 阴影纹理 0
    uint shadowtex1Index; // 阴影纹理 1

    // ========================================================================
    // 时间相关
    // ========================================================================

    float frameTimeCounter; // 总时间 (秒)
    float frameTime; // 帧时间间隔 (秒)
    uint  frameCounter; // 帧计数器

    // ========================================================================
    // 相机和视图相关
    // ========================================================================

    float3 cameraPosition; // 相机位置 (世界空间)
    float3 upPosition; // 上方向向量
    float3 sunPosition; // 太阳位置 (世界空间)
    float3 moonPosition; // 月亮位置 (世界空间)

    // ========================================================================
    // Shadow 相关
    // ========================================================================

    float4x4 shadowModelView; // Shadow 模型-视图矩阵
    float4x4 shadowProjection; // Shadow 投影矩阵

    // ========================================================================
    // 屏幕相关
    // ========================================================================

    float2 viewSize; // 视口大小 (像素)
    float  aspectRatio; // 屏幕宽高比

    // ========================================================================
    // 游戏状态
    // ========================================================================

    int   worldTime; // Minecraft 世界时间
    int   moonPhase; // 月相 (0-7)
    float rainStrength; // 雨强度 (0-1)
    float wetness; // 湿度 (0-1)

    // ========================================================================
    // 雾效相关
    // ========================================================================

    float  fogStart; // 雾起始距离
    float  fogEnd; // 雾结束距离
    float3 fogColor; // 雾颜色

    // ========================================================================
    // 保留字段 (对齐)
    // ========================================================================

    float4 reserved[4];
};

// ============================================================================
// Bindless 资源访问辅助函数
// ============================================================================

/**
 * @brief 获取 UniformBuffer
 * @return UniformBuffer 结构体
 *
 * 教学要点:
 * - ResourceDescriptorHeap 是 Shader Model 6.6 的全局描述符堆
 * - 直接用索引访问，无需预绑定
 */
UniformBuffer GetUniformBuffer()
{
    StructuredBuffer<UniformBuffer> uniformBuffers = ResourceDescriptorHeap[uniformBufferIndex];
    return uniformBuffers[0];
}

/**
 * @brief 获取 RenderTarget 纹理
 * @param rtIndex RenderTarget 索引 (0-15 对应 colortex0-15)
 * @return Texture2D 对象
 *
 * 教学要点:
 * - RenderTarget 索引数组存储在 StructuredBuffer 中
 * - 先通过 renderTargetIndicesBase 获取数组
 * - 再通过 rtIndex 获取实际纹理索引
 */
Texture2D GetRenderTarget(uint rtIndex)
{
    StructuredBuffer<uint> rtIndices    = ResourceDescriptorHeap[renderTargetIndicesBase];
    uint                   textureIndex = rtIndices[rtIndex];
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理 0
 * @return Texture2D 对象
 *
 * 教学要点:
 * - 深度纹理索引存储在 UniformBuffer 中
 * - 先获取 UniformBuffer，再读取索引，最后访问纹理
 */
Texture2D GetDepthTex0()
{
    UniformBuffer uniforms = GetUniformBuffer();
    return ResourceDescriptorHeap[uniforms.depthtex0Index];
}

/**
 * @brief 获取深度纹理 1
 * @return Texture2D 对象
 */
Texture2D GetDepthTex1()
{
    UniformBuffer uniforms = GetUniformBuffer();
    return ResourceDescriptorHeap[uniforms.depthtex1Index];
}

/**
 * @brief 获取阴影纹理 0
 * @return Texture2D 对象
 */
Texture2D GetShadowTex0()
{
    UniformBuffer uniforms = GetUniformBuffer();
    return ResourceDescriptorHeap[uniforms.shadowtex0Index];
}

/**
 * @brief 获取阴影纹理 1
 * @return Texture2D 对象
 */
Texture2D GetShadowTex1()
{
    UniformBuffer uniforms = GetUniformBuffer();
    return ResourceDescriptorHeap[uniforms.shadowtex1Index];
}

// ============================================================================
// 采样器定义
// ============================================================================

/**
 * @brief 全局采样器
 *
 * 教学要点:
 * 1. Shader Model 6.6 也支持采样器直接索引
 * 2. 这里定义静态采样器，也可以通过 SamplerDescriptorHeap[index] 动态访问
 */
SamplerState linearSampler : register(s0); // 线性采样
SamplerState pointSampler : register(s1); // 点采样
SamplerState shadowSampler : register(s2); // Shadow 采样 (比较采样器)

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * @brief 解包 Rgba8 颜色 (uint → float4)
 * @param packedColor 打包的 RGBA8 颜色 (0xAABBGGRR)
 * @return 解包后的 float4 颜色 (0.0-1.0 范围)
 *
 * 教学要点:
 * - C++ 中 Rgba8 以 uint32_t 存储: R | (G << 8) | (B << 16) | (A << 24)
 * - 需要提取各分量并归一化到 [0,1] 范围
 */
float4 UnpackRgba8(uint packedColor)
{
    float r = float((packedColor >> 0) & 0xFF) / 255.0;
    float g = float((packedColor >> 8) & 0xFF) / 255.0;
    float b = float((packedColor >> 16) & 0xFF) / 255.0;
    float a = float((packedColor >> 24) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

/**
 * @brief 标准顶点变换
 * @param input 顶点输入
 * @return 变换后的顶点输出
 *
 * 教学要点:
 * - 从 UniformBuffer 获取 MVP 矩阵
 * - 解包 uint 颜色为 float4
 * - 标准 MVP 变换
 * - 可被所有 Fallback 着色器复用
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    UniformBuffer uniforms = GetUniformBuffer();
    output.Position        = mul(float4(input.Position, 1.0), uniforms.gbufferMVP);
    output.Color           = UnpackRgba8(input.Color);
    output.TexCoord        = input.TexCoord;
    output.Tangent         = input.Tangent;
    output.Bitangent       = input.Bitangent;
    output.Normal          = input.Normal;
    output.WorldPos        = input.Position;

    return output;
}

// ============================================================================
// 常用宏定义
// ============================================================================

// Iris 兼容宏
#define colortex0 GetRenderTarget(0)
#define colortex1 GetRenderTarget(1)
#define colortex2 GetRenderTarget(2)
#define colortex3 GetRenderTarget(3)
#define colortex4 GetRenderTarget(4)
#define colortex5 GetRenderTarget(5)
#define colortex6 GetRenderTarget(6)
#define colortex7 GetRenderTarget(7)
#define colortex8 GetRenderTarget(8)
#define colortex9 GetRenderTarget(9)
#define colortex10 GetRenderTarget(10)
#define colortex11 GetRenderTarget(11)
#define colortex12 GetRenderTarget(12)
#define colortex13 GetRenderTarget(13)
#define colortex14 GetRenderTarget(14)
#define colortex15 GetRenderTarget(15)

#define depthtex0 GetDepthTex0()
#define depthtex1 GetDepthTex1()
#define shadowtex0 GetShadowTex0()
#define shadowtex1 GetShadowTex1()

// 数学常量
#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679
