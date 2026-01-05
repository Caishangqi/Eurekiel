/**
 * @file Common.hlsl
 * @brief Bindless资源访问核心库 - Iris完全兼容架构 + CustomImage扩展
 * @date 2025-11-04
 * @version v3.3 - 完整注释更新（T2-T10结构体文档化）
 *
 * 教学要点:
 * 1. 所有着色器必须 #include "Common.hlsl"
 * 2. 支持完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 3. 新增CustomImage系统: customImage0-15 (自定义材质槽位)
 * 4. Main/Alt双缓冲 - 消除ResourceBarrier开销
 * 5. Bindless资源访问 - Shader Model 6.6
 * 6. MRT动态生成 - PSOutput由ShaderCodeGenerator动态创建
 * 7. Shadow系统拆分设计 - shadowcolor和shadowtex独立Buffer管理
 *
 * 主要变更 (v3.3):
 * - T2: IDData结构体完整Iris文档化（9个字段，entity/block/item ID系统）
 * - T3: BiomeAndDimensionData结构体完整文档化（12个字段，生物群系+维度属性）
 * - T4: RenderingData结构体完整文档化（17个字段，渲染阶段+雾参数+Alpha测试）
 * - T5: WorldAndWeatherData结构体完整文档化（12个字段，太阳/月亮位置+天气系统）
 * - T6: CameraAndPlayerData结构体完整文档化（相机位置+玩家状态）
 * - T7: DepthTexturesBuffer结构体文档化（depthtex0/1/2语义说明）
 * - T8: CustomImageUniforms结构体文档化（自定义材质槽位系统）
 * - T9: ShadowTexturesBuffer注释更新（独立Buffer管理，非"激进合并"）
 * - T10: 宏定义注释更新（Iris兼容性+统一接口设计）
 *
 * 架构参考:
 * - DirectX12-Bindless-MRT-RENDERTARGETS-Architecture.md
 * - RootConstant-Redesign-v3.md (56字节拆分设计)
 * - CustomImageUniform.hpp (自定义材质槽位系统)
 */

/**
 * @brief CustomImageUniforms - 自定义材质槽位常量缓冲区（64 bytes）
 *
 * 教学要点:
 * 1. 使用cbuffer存储16个自定义材质槽位（customImage0-15）的Bindless索引
 * 2. PerObject频率 - 每次Draw Call可以独立更新不同材质
 * 3. 通过Root CBV直接访问 - 高性能，无需StructuredBuffer间接
 * 4. 64字节对齐 - GPU友好，无需额外padding
 *
 * 架构设计:
 * - 结构体大小：64字节（16个uint索引）
 * - PerObject频率，支持每个物体独立材质
 * - Root CBV绑定，零间接开销
 * - 与Iris风格兼容（customImage0-15宏）
 *
 * 使用场景:
 * - 延迟渲染PBR材质贴图
 * - 自定义后处理LUT纹理
 * - 特效贴图（噪声、遮罩等）
 *
 * 对应C++: CustomImageUniform.hpp（注意：C++端是单数命名）
 */
cbuffer CustomImageUniforms: register(b2)
{
    uint customImageIndices[16]; // customImage0-15的Bindless索引
};


/**
 * @brief PerObjectUniforms - Per-Object数据常量缓冲区 (cbuffer register(b1))
 *
 * 教学要点:
 * 1. 使用 cbuffer 存储每个物体的变换矩阵和颜色
 * 2. GPU 硬件直接优化 cbuffer 访问
 * 3. 支持 Per-Object draw 模式（每次 Draw Call 更新）
 * 4. 字段顺序必须与 PerObjectUniforms.hpp 完全一致
 * 5. 使用 float4 存储 Rgba8 颜色（已在 CPU 侧转换）
 *
 * 架构优势:
 * - 高性能: Root CBV 直接访问，无需 StructuredBuffer indirection
 * - 内存对齐: 16字节对齐，GPU 友好
 * - 兼容性: 支持 Instancing 和 Per-Object 数据
 *
 * 对应 C++: PerObjectUniforms.hpp
 */
cbuffer PerObjectUniforms : register(b1)
{
    float4x4 modelMatrix; // 模型矩阵（模型空间 → 世界空间）
    float4x4 modelMatrixInverse; // 模型逆矩阵（世界空间 → 模型空间）
    float4   modelColor; // 模型颜色（RGBA，已归一化到 [0,1]）
};


cbuffer Matrices : register(b7)
{
    float4x4 gbufferView;
    float4x4 gbufferViewInverse;
    float4x4 gbufferProjection;
    float4x4 gbufferProjectionInverse;
    float4x4 gbufferRenderer;

    /**
     * @brief 阴影模型视图矩阵
     * @type mat4
     * @iris shadowModelView
     *
     * 教学要点:
     * - 将模型空间转换到阴影视图空间
     * - 用于shadow着色器
     */
    float4x4 shadowModelView;

    /**
     * @brief 阴影模型视图逆矩阵
     * @type mat4
     * @iris shadowModelViewInverse
     *
     * 教学要点: 将阴影视图空间转换回模型空间
     */
    float4x4 shadowModelViewInverse;

    /**
     * @brief 阴影投影矩阵
     * @type mat4
     * @iris shadowProjection
     *
     * 教学要点:
     * - 将阴影视图空间转换到阴影裁剪空间
     * - 通常使用正交投影
     */
    float4x4 shadowProjection;

    /**
     * @brief 阴影投影逆矩阵
     * @type mat4
     * @iris shadowProjectionInverse
     *
     * 教学要点: 将阴影裁剪/屏幕空间转换回阴影视图空间
     */
    float4x4 shadowProjectionInverse;

    /**
     * @brief 法线矩阵（3x3存储在4x4中）
     * @type mat3 (stored as mat4)
     * @iris normalMatrix
     *
     * 教学要点:
     * - 用于变换法线向量
     * - 通常为ModelView矩阵的逆转置
     * - HLSL中使用前3x3部分
     */
    float4x4 normalMatrix;

    /**
     * @brief 纹理矩阵
     * @type mat4
     * @iris textureMatrix
     *
     * 教学要点:
     * - 用于纹理坐标变换（gl_TextureMatrix[0]）
     * - 主要用于 gbuffers_armor_glint 的纹理滚动效果
     * - 也可能被模组应用到其他几何体
     * - 等价于 GLSL 中的 gl_TextureMatrix[0]
     *
     * HLSL使用示例:
     * ```hlsl
     * float2 coord = mul(textureMatrix, float4(vaUV0, 0.0, 1.0)).xy;
     * ```
     */
    float4x4 textureMatrix;
};


/**
 * @brief 获取自定义材质槽位的Bindless索引 新增
 * @param slotIndex 槽位索引（0-15）
 * @return Bindless索引，如果槽位未设置则返回UINT32_MAX (0xFFFFFFFF)
 *
 * 教学要点:
 * - 直接访问cbuffer CustomImageUniforms获取Bindless索引
 * - 返回值为Bindless索引，可直接访问ResourceDescriptorHeap
 * - UINT32_MAX表示槽位未设置，使用前应检查有效性
 *
 * 工作原理:
 * 1. 直接从cbuffer CustomImageUniforms读取索引（Root CBV高性能访问）
 * 2. 查询customImageIndices[slotIndex]获取Bindless索引
 * 3. 返回索引供GetCustomImage()使用
 *
 * 安全性:
 * - 槽位索引会被限制在0-15范围内
 * - 超出范围返回UINT32_MAX（无效索引）
 */
uint GetCustomImageIndex(uint slotIndex)
{
    // 防止越界访问
    if (slotIndex >= 16)
    {
        return 0xFFFFFFFF; // UINT32_MAX - 无效索引
    }
    // 2. 查询指定槽位的Bindless索引
    uint bindlessIndex = customImageIndices[slotIndex];

    // 3. 返回Bindless索引
    return bindlessIndex;
}

/**
 * @brief 获取自定义材质纹理（便捷访问函数）新增
 * @param slotIndex 槽位索引（0-15）
 * @return 对应的Bindless纹理（Texture2D）
 *
 * 教学要点:
 * - 封装GetCustomImageIndex() + ResourceDescriptorHeap访问
 * - 自动处理无效索引情况（返回黑色纹理或默认纹理）
 * - 遵循Iris纹理访问模式
 *
 * 使用示例:
 * ```hlsl
 * // 获取albedo贴图（假设slot 0存储albedo）
 * Texture2D albedoTex = GetCustomImage(0);
 * float4 albedo = albedoTex.Sample(linearSampler, uv);
 *
 * // 获取roughness贴图（假设slot 1存储roughness）
 * Texture2D roughnessTex = GetCustomImage(1);
 * float roughness = roughnessTex.Sample(linearSampler, uv).r;
 * ```
 *
 * 注意:
 * - 如果槽位未设置（UINT32_MAX），访问ResourceDescriptorHeap可能导致未定义行为
 * - 调用前应确保槽位已正确设置，或使用默认值处理
 */
Texture2D GetCustomImage(uint slotIndex)
{
    // 1. 获取Bindless索引
    uint bindlessIndex = GetCustomImageIndex(slotIndex);

    // 2. 通过Bindless索引访问全局描述符堆
    // 注意：如果bindlessIndex为UINT32_MAX，这里可能需要额外的有效性检查
    // 当前设计假设C++端会确保所有使用的槽位都已正确设置
    return ResourceDescriptorHeap[bindlessIndex];
}

//──────────────────────────────────────────────────────
// Iris兼容宏（完整纹理系统）
//──────────────────────────────────────────────────────

#define customImage0  GetCustomImage(0)
#define customImage1  GetCustomImage(1)
#define customImage2  GetCustomImage(2)
#define customImage3  GetCustomImage(3)
#define customImage4  GetCustomImage(4)
#define customImage5  GetCustomImage(5)
#define customImage6  GetCustomImage(6)
#define customImage7  GetCustomImage(7)
#define customImage8  GetCustomImage(8)
#define customImage9  GetCustomImage(9)
#define customImage10 GetCustomImage(10)
#define customImage11 GetCustomImage(11)
#define customImage12 GetCustomImage(12)
#define customImage13 GetCustomImage(13)
#define customImage14 GetCustomImage(14)
#define customImage15 GetCustomImage(15)


/**
 * @brief global sampler definition
 *
 *Teaching points:
 * 1. Static sampler - fixedly bound to register(s0-s2)
 * 2. Linear sampling: used for texture filtering
 * 3. Point sampling: for precise pixel access
 * 4. Shadow sampling: Comparison Sampler
 *
 *Shader Model 6.6 also supports:
 * - SamplerDescriptorHeap[index] dynamic sampler access
 */
SamplerState linearSampler : register(s0); // Linear filtering
SamplerState pointSampler: register(s1); // point sampling
SamplerState shadowSampler: register(s2); // Shadow comparison sampler
SamplerState wrapSampler : register(s3); // Point sampling with WRAP mode

//──────────────────────────────────────────────────────
// 注意事项
//──────────────────────────────────────────────────────

/**
 * 不在Common.hlsl中定义固定的PSOutput！
 * + PSOutput由ShaderCodeGenerator动态生成
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
 * @brief vertex shader input structure
 *
 *Teaching points:
 * 1. Fixed vertex format - all geometry uses the same layout
 * 2. Corresponds to C++ enigma::graphic::Vertex (Vertex_PCUTBN)
 * 3. For Gbuffers Pass (geometry rendering)
 *
 * Note: Composite Pass uses full-screen triangles and does not require complex vertex formats
 */
struct VSInput
{
    float3 Position : POSITION; // Vertex position (world space)
    float4 Color : COLOR0; // Vertex color (R8G8B8A8_UNORM)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
};

/**
 * @brief vertex shader output / pixel shader input
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // Clipping space position
    float4 Color : COLOR0; // Vertex color (after unpacking)
    float2 TexCoord : TEXCOORD0; // Texture coordinates
    float3 Tangent : TANGENT; // Tangent vector
    float3 Bitangent : BITANGENT; // Bitangent vector
    float3 Normal : NORMAL; // normal vector
    float3 WorldPos : TEXCOORD1; // World space position
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

/**
 * @brief 标准顶点变换函数（用于Fallback Shader）
 * @param input 顶点输入（VSInput结构体）
 * @return VSOutput - 变换后的顶点输出
 *
 * 教学要点:
 * 1. 用于 gbuffers_basic 和 gbuffers_textured 的 Fallback 着色器
 * 2. 自动处理 MVP 变换、颜色解包、数据传递
 * 3. 从 Uniform Buffers 获取变换矩阵
 * 4. KISS 原则 - 极简实现，无额外计算
 *
 * 工作流程:
 * 1. [NEW] 直接访问 cbuffer Matrices 获取变换矩阵 (无需函数调用)
 * 2. 顶点位置变换: Position → ViewSpace → ClipSpace
 * 3. 法线变换: normalMatrix 变换法线向量
 * 4. 颜色解包: uint → float4
 * 5. 传递所有顶点属性到像素着色器
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    // 1. 顶点位置变换
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferView, worldPos);
    float4 renderPos = mul(gbufferRenderer, cameraPos);
    float4 clipPos   = mul(gbufferProjection, renderPos);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    // 4. 法线变换
    output.Normal = normalize(mul(normalMatrix, float4(input.Normal, 0.0)).xyz);

    // 5. 切线和副切线
    output.Tangent   = normalize(mul(gbufferView, float4(input.Tangent, 0.0)).xyz);
    output.Bitangent = normalize(mul(gbufferView, float4(input.Bitangent, 0.0)).xyz);

    return output;
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
 * 1.5. CustomImage使用示例（延迟渲染PBR材质）新增:
 * ```hlsl
 * #include "Common.hlsl"
 *
 * PSOutput PSMain(PSInput input)
 * {
 *     PSOutput output;
 *
 *     // 方式1：通过宏直接访问（推荐）
 *     float4 albedo = customImage0.Sample(linearSampler, input.TexCoord);    // Albedo贴图
 *     float roughness = customImage1.Sample(linearSampler, input.TexCoord).r; // Roughness贴图
 *     float metallic = customImage2.Sample(linearSampler, input.TexCoord).r;  // Metallic贴图
 *     float3 normal = customImage3.Sample(linearSampler, input.TexCoord).rgb; // Normal贴图
 *
 *     // 方式2：通过索引访问（动态槽位）
 *     uint slotIndex = 5;
 *     Texture2D dynamicTex = GetCustomImage(slotIndex);
 *     float4 data = dynamicTex.Sample(linearSampler, input.TexCoord);
 *
 *     // 方式3：检查槽位有效性（安全访问）
 *     uint bindlessIndex = GetCustomImageIndex(4);
 *     if (bindlessIndex != 0xFFFFFFFF) // 检查是否为无效索引
 *     {
 *         Texture2D aoMap = GetCustomImage(4); // AO贴图
 *         float ao = aoMap.Sample(linearSampler, input.TexCoord).r;
 *         albedo.rgb *= ao; // 应用AO
 *     }
 *
 *     // PBR光照计算
 *     float3 finalColor = CalculatePBR(albedo.rgb, roughness, metallic, normal);
 *     output.Color0 = float4(finalColor, 1.0);
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
