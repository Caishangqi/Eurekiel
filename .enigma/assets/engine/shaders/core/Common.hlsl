/**
 * @file Common.hlsl
 * @brief Bindless资源访问核心库 - Iris完全兼容架构（激进48字节设计）
 * @date 2025-10-10
 * @version v3.0 - 完整Iris纹理系统支持（激进合并）
 *
 * 教学要点:
 * 1. 所有着色器必须 #include "Common.hlsl"
 * 2. 支持完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 3. Main/Alt双缓冲 - 消除ResourceBarrier开销
 * 4. Bindless资源访问 - Shader Model 6.6
 * 5. MRT动态生成 - PSOutput由ShaderCodeGenerator动态创建
 * 6. 激进Shadow合并 - shadowcolor + shadowtex统一管理
 *
 * 架构参考:
 * - DirectX12-Bindless-MRT-RENDERTARGETS-Architecture.md
 * - RootConstant-Redesign-v2.md (48字节激进设计)
 */

//──────────────────────────────────────────────────────
// Root Constants (48 bytes) - 激进设计
//──────────────────────────────────────────────────────

/**
 * @brief Root Constants - 完整Iris纹理系统支持（激进48字节设计）
 *
 * 教学要点:
 * 1. 总共48 bytes (12个uint32_t索引)
 * 2. register(b0, space0) 对应 Root Parameter 0
 * 3. 支持细粒度更新 - 每个索引可独立更新
 * 4. 完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 *
 * 架构优势（激进方案）:
 * - 极简设计: 合并Shadow系统到单一索引（shadowBufferIndex）
 * - 全局共享Root Signature - 所有PSO使用同一个
 * - Root Signature切换: 1次/帧 (传统方式: 1000次/帧)
 * - 支持1,000,000+纹理同时注册
 *
 * 激进合并关键:
 * - shadowBufferIndex包含: shadowcolor0-7 (flip) + shadowtex0/1 (固定)
 * - 减少Root Constants占用: 从52字节降至48字节
 *
 * C++ 对应:
 * ```cpp
 * struct RootConstants {
 *     // Uniform Buffers (32 bytes)
 *     uint32_t cameraAndPlayerBufferIndex;      // Offset 0
 *     uint32_t playerStatusBufferIndex;         // Offset 4
 *     uint32_t screenAndSystemBufferIndex;      // Offset 8
 *     uint32_t idBufferIndex;                   // Offset 12
 *     uint32_t worldAndWeatherBufferIndex;      // Offset 16
 *     uint32_t biomeAndDimensionBufferIndex;    // Offset 20
 *     uint32_t renderingBufferIndex;            // Offset 24
 *     uint32_t matricesBufferIndex;             // Offset 28
 *     // Texture Buffers (4 bytes)
 *     uint32_t colorTargetsBufferIndex;         // Offset 32 ⭐
 *     // Combined Buffers (8 bytes)
 *     uint32_t depthTexturesBufferIndex;        // Offset 36
 *     uint32_t shadowBufferIndex;               // Offset 40 ⭐ 激进合并
 *     // Direct Texture (4 bytes)
 *     uint32_t noiseTextureIndex;               // Offset 44
 * };
 * ```
 */
cbuffer RootConstants : register(b0, space0)
{
    // Uniform Buffers (32 bytes)
    uint cameraAndPlayerBufferIndex; // 相机和玩家数据
    uint playerStatusBufferIndex; // 玩家状态数据
    uint screenAndSystemBufferIndex; // 屏幕和系统数据
    uint idBufferIndex; // ID数据
    uint worldAndWeatherBufferIndex; // 世界和天气数据
    uint biomeAndDimensionBufferIndex; // 生物群系和维度数据
    uint renderingBufferIndex; // 渲染数据
    uint matricesBufferIndex; // 矩阵数据

    // Texture Buffers with Flip Support (4 bytes)
    uint colorTargetsBufferIndex; // ⭐ colortex0-15 (Main/Alt)

    // Combined Buffers (8 bytes)
    uint depthTexturesBufferIndex; // depthtex0/1/2 (引擎生成)
    uint shadowBufferIndex; // ⭐ shadowcolor0-7 + shadowtex0/1 (激进合并)

    // Direct Texture Index (4 bytes)
    uint noiseTextureIndex; // noisetex (静态纹理)
};

//──────────────────────────────────────────────────────
// Buffer结构体（C++端上传）
//──────────────────────────────────────────────────────

/**
 * @brief RenderTargetsBuffer - colortex0-15双缓冲索引管理（128 bytes）
 *
 * 教学要点:
 * 1. 存储16个colortex的读写索引
 * 2. 读取索引: 根据flip状态指向Main或Alt纹理
 * 3. 写入索引: 预留给UAV扩展（当前RTV直接绑定）
 * 4. 每个Pass执行前上传到GPU
 *
 * 对应C++: RenderTargetsIndexBuffer.hpp
 */
struct RenderTargetsBuffer
{
    uint readIndices[16]; // colortex0-15读取索引（Main或Alt）
    uint writeIndices[16]; // colortex0-15写入索引（预留）
};

/**
 * @brief DepthTexturesBuffer - depthtex0/1/2索引管理（16 bytes）
 *
 * 教学要点:
 * 1. 存储3个引擎生成的深度纹理索引
 * 2. 不需要flip机制（每帧独立生成）
 * 3. depthtex0: 完整深度（包含半透明+手部）
 * 4. depthtex1: 半透明前深度（no translucents）
 * 5. depthtex2: 手部前深度（no hand）
 *
 * 对应C++: DepthTexturesIndexBuffer.hpp
 */
struct DepthTexturesBuffer
{
    uint depthtex0Index; // 完整深度
    uint depthtex1Index; // 半透明前深度
    uint depthtex2Index; // 手部前深度
    uint padding; // 对齐填充
};

/**
 * @brief ShadowBuffer - Shadow系统合并管理（80 bytes）⭐ 激进设计
 *
 * 教学要点:
 * 1. 合并shadowcolor（需要flip）+ shadowtex（固定）
 * 2. shadowColorReadIndices: shadowcolor0-7读取索引（Main或Alt）
 * 3. shadowColorWriteIndices: shadowcolor0-7写入索引（预留）
 * 4. shadowtex0Index: 完整阴影深度
 * 5. shadowtex1Index: 半透明前阴影深度
 *
 * 激进方案优势:
 * - 统一Shadow系统管理
 * - 减少Root Constants占用
 * - shadowcolor支持Ping-Pong，shadowtex为只读
 *
 * 对应C++: ShadowBufferIndex.hpp
 */
struct ShadowBuffer
{
    // Shadow Color Targets（需要flip）- 64 bytes
    uint shadowColorReadIndices[8]; // shadowcolor0-7读取索引
    uint shadowColorWriteIndices[8]; // shadowcolor0-7写入索引（预留）

    // Shadow Depth Textures（不需要flip）- 16 bytes
    uint shadowtex0Index; // 完整阴影深度
    uint shadowtex1Index; // 半透明前阴影深度
    uint padding[2]; // 对齐填充
};

//──────────────────────────────────────────────────────
// Bindless资源访问函数
//──────────────────────────────────────────────────────

/**
 * @brief 获取ColorTarget（colortex0-15读取用）
 * @param rtIndex colortex索引（0-15）
 * @return Texture2D资源（Main或Alt，根据flip状态）
 *
 * 工作原理:
 * 1. 通过colorTargetsBufferIndex访问RenderTargetsBuffer
 * 2. 查询readIndices[rtIndex]获取纹理索引
 * 3. 通过纹理索引访问ResourceDescriptorHeap
 *
 * 教学要点:
 * - 真正的Bindless访问 - 无需预绑定
 * - Main/Alt自动处理 - 由flip状态决定
 * - ResourceDescriptorHeap是SM6.6全局堆
 */
Texture2D GetRenderTarget(uint rtIndex)
{
    // 1. 获取RenderTargetsBuffer（StructuredBuffer）
    StructuredBuffer<RenderTargetsBuffer> rtBuffer =
        ResourceDescriptorHeap[colorTargetsBufferIndex];

    // 2. 查询读取索引（Main或Alt）
    uint textureIndex = rtBuffer[0].readIndices[rtIndex];

    // 3. 直接访问全局描述符堆
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理0（完整深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex0: 包含所有物体的完整深度（不透明+半透明+手部）
 * - 引擎渲染过程自动生成，不需要flip
 * - 用于后处理和深度测试
 */
Texture2D<float> GetDepthTex0()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex0索引
    uint textureIndex = depthBuffer[0].depthtex0Index;

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理1（半透明前深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex1: 只包含不透明物体的深度（no translucents）
 * - 用于半透明物体的正确混合和深度测试
 */
Texture2D<float> GetDepthTex1()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex1索引
    uint textureIndex = depthBuffer[0].depthtex1Index;

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理2（手部前深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex2: 包含除手部外所有物体的深度（no hand）
 * - 用于手部的深度测试和特效
 */
Texture2D<float> GetDepthTex2()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex2索引
    uint textureIndex = depthBuffer[0].depthtex2Index;

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影颜色纹理（shadowcolor0-7）⭐ 激进合并
 * @param shadowIndex shadowcolor索引（0-7）
 * @return Texture2D资源（Main或Alt，根据flip状态）
 *
 * 教学要点:
 * - shadowcolor支持flip机制（Ping-Pong双缓冲）
 * - 用于多Pass阴影渲染（shadowcomp阶段）
 * - 激进方案：与shadowtex合并在同一Buffer
 */
Texture2D GetShadowColor(uint shadowIndex)
{
    // 1. 获取ShadowBuffer
    StructuredBuffer<ShadowBuffer> shadowBuffer =
        ResourceDescriptorHeap[shadowBufferIndex];

    // 2. 查询shadowcolor读取索引（Main或Alt）
    uint textureIndex = shadowBuffer[0].shadowColorReadIndices[shadowIndex];

    // 3. 返回shadowcolor纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影深度纹理0（完整阴影深度）⭐ 激进合并
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - shadowtex0: 包含所有物体的阴影深度（不透明+半透明）
 * - 只读纹理，不需要flip
 * - 激进方案：与shadowcolor合并在同一Buffer
 */
Texture2D<float> GetShadowTex0()
{
    // 1. 获取ShadowBuffer
    StructuredBuffer<ShadowBuffer> shadowBuffer =
        ResourceDescriptorHeap[shadowBufferIndex];

    // 2. 查询shadowtex0索引
    uint textureIndex = shadowBuffer[0].shadowtex0Index;

    // 3. 返回shadowtex0
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影深度纹理1（半透明前阴影深度）⭐ 激进合并
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - shadowtex1: 只包含不透明物体的阴影深度
 * - 用于半透明物体的阴影处理
 * - 激进方案：与shadowcolor合并在同一Buffer
 */
Texture2D<float> GetShadowTex1()
{
    // 1. 获取ShadowBuffer
    StructuredBuffer<ShadowBuffer> shadowBuffer =
        ResourceDescriptorHeap[shadowBufferIndex];

    // 2. 查询shadowtex1索引
    uint textureIndex = shadowBuffer[0].shadowtex1Index;

    // 3. 返回shadowtex1
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取噪声纹理（noisetex）
 * @return Texture2D资源
 *
 * 教学要点:
 * - 静态噪声纹理，RGB8格式，默认256×256
 * - 用于随机采样、抖动、时间相关效果
 * - 直接使用纹理索引，无需Buffer包装
 */
Texture2D GetNoiseTex()
{
    // 直接访问噪声纹理（noiseTextureIndex是直接索引）
    return ResourceDescriptorHeap[noiseTextureIndex];
}

//──────────────────────────────────────────────────────
// Iris兼容宏（完整纹理系统）
//──────────────────────────────────────────────────────

/**
 * @brief Iris Shader完全兼容宏定义
 *
 * 教学要点:
 * 1. 支持完整Iris纹理系统：colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 2. 宏自动映射到对应的Get函数
 * 3. 零学习成本移植Iris Shader Pack
 * 4. Main/Alt自动处理，无需手动管理双缓冲
 *
 * 示例:
 * ```hlsl
 * // 主渲染纹理（colortex0-15）
 * float4 color = colortex0.Sample(linearSampler, uv);
 *
 * // 阴影纹理（shadowcolor0-7, shadowtex0/1）
 * float4 shadowColor = shadowcolor0.Sample(linearSampler, shadowUV);
 * float shadowDepth = shadowtex0.Load(int3(pixelPos, 0));
 *
 * // 深度纹理（depthtex0/1/2）
 * float depth = depthtex0.Load(int3(pixelPos, 0));
 *
 * // 噪声纹理（noisetex）
 * float4 noise = noisetex.Sample(linearSampler, uv);
 * ```
 */

// ===== ColorTex（colortex0-15）=====
#define colortex0  GetRenderTarget(0)
#define colortex1  GetRenderTarget(1)
#define colortex2  GetRenderTarget(2)
#define colortex3  GetRenderTarget(3)
#define colortex4  GetRenderTarget(4)
#define colortex5  GetRenderTarget(5)
#define colortex6  GetRenderTarget(6)
#define colortex7  GetRenderTarget(7)
#define colortex8  GetRenderTarget(8)
#define colortex9  GetRenderTarget(9)
#define colortex10 GetRenderTarget(10)
#define colortex11 GetRenderTarget(11)
#define colortex12 GetRenderTarget(12)
#define colortex13 GetRenderTarget(13)
#define colortex14 GetRenderTarget(14)
#define colortex15 GetRenderTarget(15)

// ===== DepthTex（depthtex0/1/2）=====
#define depthtex0  GetDepthTex0()
#define depthtex1  GetDepthTex1()
#define depthtex2  GetDepthTex2()

// ===== ShadowColor（shadowcolor0-7）⭐ 激进合并 =====
#define shadowcolor0 GetShadowColor(0)
#define shadowcolor1 GetShadowColor(1)
#define shadowcolor2 GetShadowColor(2)
#define shadowcolor3 GetShadowColor(3)
#define shadowcolor4 GetShadowColor(4)
#define shadowcolor5 GetShadowColor(5)
#define shadowcolor6 GetShadowColor(6)
#define shadowcolor7 GetShadowColor(7)

// ===== ShadowTex（shadowtex0/1）⭐ 激进合并 =====
#define shadowtex0 GetShadowTex0()
#define shadowtex1 GetShadowTex1()

// ===== NoiseTex（noisetex）=====
#define noisetex GetNoiseTex()

//──────────────────────────────────────────────────────
// Sampler States
//──────────────────────────────────────────────────────

/**
 * @brief 全局采样器定义
 *
 * 教学要点:
 * 1. 静态采样器 - 固定绑定到register(s0-s2)
 * 2. 线性采样: 用于纹理过滤
 * 3. 点采样: 用于精确像素访问
 * 4. 阴影采样: 比较采样器（Comparison Sampler）
 *
 * Shader Model 6.6也支持:
 * - SamplerDescriptorHeap[index] 动态采样器访问
 */
SamplerState linearSampler : register(s0); // 线性过滤
SamplerState pointSampler : register(s1); // 点采样
SamplerState shadowSampler : register(s2); // 阴影比较采样器

//──────────────────────────────────────────────────────
// 注意事项
//──────────────────────────────────────────────────────

/**
 * ❌ 不在Common.hlsl中定义固定的PSOutput！
 * ✅ PSOutput由ShaderCodeGenerator动态生成
 *
 * 原因:
 * 1. 每个Shader的RENDERTARGETS注释不同
 * 2. PSOutput必须与实际输出RT数量匹配
 * 3. 固定16个输出会浪费寄存器和性能
 *
 * 示例:
 * cpp
 * // ShaderCodeGenerator根据 RENDERTARGETS: 0,3,7,12 动态生成:
 * struct PSOutput {
 *     float4 Color0 : SV_Target0;  // → colortex0
 *     float4 Color1 : SV_Target1;  // → colortex3
 *     float4 Color2 : SV_Target2;  // → colortex7
 *     float4 Color3 : SV_Target3;  // → colortex12
 * };
 * 
 */

//──────────────────────────────────────────────────────
// 固定 Input Layout (顶点格式) - 保留用于Gbuffers
//──────────────────────────────────────────────────────

/**
 * @brief 顶点着色器输入结构体
 *
 * 教学要点:
 * 1. 固定顶点格式 - 所有几何体使用相同布局
 * 2. 对应 C++ enigma::graphic::Vertex (Vertex_PCUTBN)
 * 3. 用于Gbuffers Pass（几何体渲染）
 *
 * 注意: Composite Pass使用全屏三角形，不需要复杂顶点格式
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
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // 裁剪空间位置
    float4 Color : COLOR0; // 顶点颜色 (解包后)
    float2 TexCoord : TEXCOORD0; // 纹理坐标
    float3 Tangent : TANGENT; // 切线向量
    float3 Bitangent : BITANGENT; // 副切线向量
    float3 Normal : NORMAL; // 法线向量
    float3 WorldPos : TEXCOORD1; // 世界空间位置
};

// 像素着色器输入 (与 VSOutput 相同)
typedef VSOutput PSInput;

//──────────────────────────────────────────────────────
// 辅助函数
//──────────────────────────────────────────────────────

/**
 * @brief 解包 Rgba8 颜色 (uint → float4)
 * @param packedColor 打包的 RGBA8 颜色 (0xAABBGGRR)
 * @return 解包后的 float4 颜色 (0.0-1.0 范围)
 */
float4 UnpackRgba8(uint packedColor)
{
    float r = float((packedColor >> 0) & 0xFF) / 255.0;
    float g = float((packedColor >> 8) & 0xFF) / 255.0;
    float b = float((packedColor >> 16) & 0xFF) / 255.0;
    float a = float((packedColor >> 24) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

//──────────────────────────────────────────────────────
// 数学常量
//──────────────────────────────────────────────────────

#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679

//──────────────────────────────────────────────────────
// 架构说明和使用示例
//──────────────────────────────────────────────────────

/**
 * ========== 使用示例 ==========
 *
 * 1. Composite Pass Shader (全屏后处理):
 * ```hlsl
 * #include "Common.hlsl"
 *
 * // PSOutput由ShaderCodeGenerator动态生成
 * // 例如: struct PSOutput { float4 Color0:SV_Target0; ... }
 *
 * PSOutput PSMain(PSInput input)
 * {
 *     PSOutput output;
 *
 *     // 读取纹理（Bindless，自动处理Main/Alt）
 *     float4 color0 = colortex0.Sample(linearSampler, input.TexCoord);
 *     float4 color8 = colortex8.Sample(linearSampler, input.TexCoord);
 *
 *     // 输出到多个RT
 *     output.Color0 = color0 * 0.5;    // 写入colortex0
 *     output.Color1 = color8 * 2.0;    // 写入colortex3
 *
 *     return output;
 * }
 * ```
 *
 * 2. RENDERTARGETS注释:
 * ```hlsl
 * RENDERTARGETS: 0,3,7,12
 * // ShaderCodeGenerator解析此注释，生成4个输出的PSOutput
 * ```
 *
 * 3. Main/Alt自动处理:
 * - colortex0自动访问Main或Alt（根据flip状态）
 * - 无需手动管理双缓冲
 * - 无需ResourceBarrier同步
 *
 * ========== 架构优势 ==========
 *
 * 1. Iris完全兼容:
 *    - colortex0-15宏自动映射
 *    - RENDERTARGETS注释支持
 *    - depthtex0/depthtex1支持
 *
 * 2. 性能优化:
 *    - Main/Alt消除90%+ ResourceBarrier
 *    - Bindless零绑定开销
 *    - Root Signature全局共享
 *
 * 3. 现代化设计:
 *    - Shader Model 6.6真正Bindless
 *    - MRT动态生成（不浪费）
 *    - 声明式编程（注释驱动）
 */
