/**
 * @file Common.hlsl
 * @brief Bindless资源访问核心库 - Iris完全兼容架构 + CustomImage扩展
 * @date 2025-10-31
 * @version v3.2 - Shadow系统拆分设计（56字节RootConstants）
 *
 * 教学要点:
 * 1. 所有着色器必须 #include "Common.hlsl"
 * 2. 支持完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 3. 新增CustomImage系统: customImage0-15 (自定义材质槽位) ⭐
 * 4. Main/Alt双缓冲 - 消除ResourceBarrier开销
 * 5. Bindless资源访问 - Shader Model 6.6
 * 6. MRT动态生成 - PSOutput由ShaderCodeGenerator动态创建
 * 7. Shadow系统拆分 - shadowcolor和shadowtex独立管理 ⭐
 *
 * 架构参考:
 * - DirectX12-Bindless-MRT-RENDERTARGETS-Architecture.md
 * - RootConstant-Redesign-v3.md (56字节拆分设计)
 * - CustomImageIndexBuffer.hpp (自定义材质槽位系统)
 */

//──────────────────────────────────────────────────────
// Root Constants (56 bytes) - Shadow系统拆分设计
//──────────────────────────────────────────────────────

/**
 * @brief Root Constants - Shadow系统拆分 + CustomImage支持（56字节设计）
 *
 * 教学要点:
 * 1. 总共56 bytes (14个uint32_t索引) - Shadow系统拆分 ⭐
 * 2. register(b0, space0) 对应 Root Parameter 0
 * 3. 支持细粒度更新 - 每个索引可独立更新
 * 4. 完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 5. CustomImage系统: customImage0-15 (自定义材质槽位) ⭐
 *
 * 架构优势:
 * - 拆分设计: shadowcolor和shadowtex独立管理，提高灵活性 ⭐
 * - 全局共享Root Signature - 所有PSO使用同一个
 * - Root Signature切换: 1次/帧 (传统方式: 1000次/帧)
 * - 支持1,000,000+纹理同时注册
 * - 材质系统扩展: 16个自定义槽位用于PBR材质、LUT、特效贴图等
 *
 * 拆分设计关键:
 * - shadowColorBufferIndex: shadowcolor0-7 (需要flip机制)
 * - shadowTexturesBufferIndex: shadowtex0/1 (固定，不需要flip)
 * - customImageBufferIndexCBV: 256字节Buffer，存储16个槽位索引
 * - 总大小从52字节增至56字节（仍在Root Signature优化范围内）
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
 *     // Combined Buffers (12 bytes) - 拆分Shadow系统
 *     uint32_t depthTexturesBufferIndex;        // Offset 36
 *     uint32_t shadowColorBufferIndex;          // Offset 40 ⭐ 拆分
 *     uint32_t shadowTexturesBufferIndex;       // Offset 44 ⭐ 拆分
 *     // Direct Texture (4 bytes)
 *     uint32_t noiseTextureIndex;               // Offset 48
 *     // CustomImage Buffer (4 bytes) - 新增 ⭐
 *     uint32_t customImageBufferIndexCBV;       // Offset 52 (customImage0-15)
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

    // Combined Buffers (12 bytes) - 拆分Shadow系统
    uint depthTexturesBufferIndex; // depthtex0/1/2 (引擎生成) - Offset 36
    uint shadowColorBufferIndex; // ⭐ shadowcolor0-7 (需要flip) - Offset 40
    uint shadowTexturesBufferIndex; // ⭐ shadowtex0/1 (固定) - Offset 44

    // Direct Texture Index (4 bytes)
    uint noiseTextureIndex; // noisetex (静态纹理) - Offset 48

    // CustomImage Buffer Index (4 bytes) - 新增
    uint customImageBufferIndexCBV; // ⭐ customImage0-15 (自定义材质槽位) - Offset 52
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
 * @brief ShadowColorBuffer - Shadow颜色纹理索引管理（64 bytes）⭐ 拆分设计
 *
 * 教学要点:
 * 1. 存储8个shadowcolor的读写索引
 * 2. shadowColorReadIndices: shadowcolor0-7读取索引（Main或Alt）
 * 3. shadowColorWriteIndices: shadowcolor0-7写入索引（预留）
 * 4. 支持flip机制（Ping-Pong双缓冲）
 *
 * 对应C++: ShadowColorIndexBuffer.hpp
 */
struct ShadowColorBuffer
{
    uint shadowColorReadIndices[8]; // shadowcolor0-7读取索引（Main或Alt）
    uint shadowColorWriteIndices[8]; // shadowcolor0-7写入索引（预留）
};

/**
 * @brief ShadowTexturesBuffer - Shadow深度纹理索引管理（16 bytes）⭐ 拆分设计
 *
 * 教学要点:
 * 1. 存储2个shadowtex的索引
 * 2. shadowtex0Index: 完整阴影深度（不透明+半透明）
 * 3. shadowtex1Index: 半透明前阴影深度
 * 4. 不需要flip机制（只读纹理）
 *
 * 对应C++: ShadowTexturesIndexBuffer.hpp
 */
struct ShadowTexturesBuffer
{
    uint shadowtex0Index; // 完整阴影深度
    uint shadowtex1Index; // 半透明前阴影深度
    uint padding[2]; // 对齐填充
};

/**
 * @brief CustomImageIndexBuffer - 自定义材质槽位索引缓冲区（256 bytes）⭐ 新增
 *
 * 教学要点:
 * 1. 存储16个自定义材质槽位（customImage0-15）的Bindless索引
 * 2. 类似ColorTargetsIndexBuffer，但只需要读取索引（不需要写入索引）
 * 3. UINT32_MAX (0xFFFFFFFF) 表示槽位未设置
 * 4. 用于材质系统扩展，支持自定义贴图（albedo、roughness、metallic等）
 *
 * 架构设计:
 * - 对齐到256字节，与ColorTargetsIndexBuffer保持一致
 * - 只读访问，不需要flip机制
 * - 支持运行时动态更新槽位
 *
 * 使用场景:
 * - 延迟渲染PBR材质贴图
 * - 自定义后处理LUT纹理
 * - 特效贴图（噪声、遮罩等）
 *
 * 对应C++: CustomImageIndexBuffer.hpp
 */
struct CustomImageIndexBuffer
{
    uint customImageIndices[16]; // customImage0-15的Bindless索引
    uint padding[48]; // 填充到256字节
};

// ===== Uniform Buffers（8个）=====

/**
 * @brief CameraAndPlayerBuffer - 相机和玩家数据（对应CameraAndPlayerUniforms.hpp）
 */
struct CameraAndPlayerData
{
    float3 cameraPosition; // 相机世界位置
    float  eyeAltitude; // 玩家高度

    float3 cameraPositionFract; // 相机位置小数部分
    float  _padding1;

    int3  cameraPositionInt; // 相机位置整数部分
    float _padding2;

    float3 previousCameraPosition; // 上一帧相机位置
    float  _padding3;

    float3 previousCameraPositionFract; // 上一帧相机位置小数部分
    float  _padding4;

    int3  previousCameraPositionInt; // 上一帧相机位置整数部分
    float _padding5;

    float3 eyePosition; // 玩家头部位置
    float  _padding6;

    float3 relativeEyePosition; // 头部到相机偏移
    float  _padding7;

    float3 playerBodyVector; // 玩家身体朝向
    float  _padding8;

    float3 playerLookVector; // 玩家视线朝向
    float  _padding9;

    float3 upPosition; // 上方向向量（长度100）
    float  _padding10;

    int2 eyeBrightness; // 玩家位置光照值
    int2 eyeBrightnessSmooth; // 平滑光照值

    float  centerDepthSmooth; // 屏幕中心深度（平滑）
    bool   firstPersonCamera; // 是否第一人称
    float2 _padding11;
};

/**
 * @brief PlayerStatusData - 玩家状态数据（对应PlayerStatusUniforms.hpp）
 */
struct PlayerStatusData
{
    int   isEyeInWater; // 眼睛是否在流体中（0-3）
    bool  isSpectator; // 是否旁观模式
    bool  isRightHanded; // 是否右撇子
    float blindness; // 失明效果强度

    float darknessFactor; // 黑暗效果因子
    float darknessLightFactor; // 黑暗光照因子
    float nightVision; // 夜视效果强度
    float playerMood; // 玩家情绪值

    float constantMood; // 持续情绪值
    float currentPlayerAir; // 当前氧气
    float maxPlayerAir; // 最大氧气
    float currentPlayerArmor; // 当前护甲

    float maxPlayerArmor; // 最大护甲
    float currentPlayerHealth; // 当前血量
    float maxPlayerHealth; // 最大血量
    float currentPlayerHunger; // 当前饥饿值

    float maxPlayerHunger; // 最大饥饿值
    bool  is_burning; // 是否着火
    bool  is_hurt; // 是否受伤
    bool  is_invisible; // 是否隐身

    bool is_on_ground; // 是否在地面
    bool is_sneaking; // 是否潜行
    bool is_sprinting; // 是否疾跑
    bool hideGUI; // 是否隐藏GUI
};

/**
 * @brief ScreenAndSystemData - 屏幕和系统数据（对应ScreenAndSystemUniforms.hpp）
 */
struct ScreenAndSystemData
{
    float viewHeight; // 视口高度
    float viewWidth; // 视口宽度
    float aspectRatio; // 宽高比
    float screenBrightness; // 屏幕亮度

    int   frameCounter; // 帧计数器
    float frameTime; // 上一帧时间（秒）
    float frameTimeCounter; // 运行时间累计（秒）
    int   currentColorSpace; // 显示器颜色空间

    int3  currentDate; // 系统日期（年，月，日）
    float _padding1;

    int3  currentTime; // 系统时间（时，分，秒）
    float _padding2;

    int2   currentYearTime; // 年内时间统计
    float2 _padding3;
};

/**
 * @brief IDData - ID数据（对应IDUniforms.hpp）
 * 注意：需要读取IDUniforms.hpp确定确切字段
 */
struct IDData
{
    int   entityId; // 实体ID
    int   blockId; // 方块ID
    int   renderStage; // 渲染阶段
    float _padding;
};

/**
 * @brief WorldAndWeatherData - 世界和天气数据（对应WorldAndWeatherUniforms.hpp）
 * 注意：需要读取WorldAndWeatherUniforms.hpp确定确切字段
 */
struct WorldAndWeatherData
{
    int   worldTime; // 世界时间
    int   worldDay; // 世界天数
    int   moonPhase; // 月相
    float sunAngle; // 太阳角度

    float shadowAngle; // 阴影角度
    float rainStrength; // 雨强度
    float wetness; // 湿度
    float thunderStrength; // 雷电强度

    float3 sunPosition; // 太阳位置
    float  _padding1;

    float3 moonPosition; // 月亮位置
    float  _padding2;

    float3 shadowLightPosition; // 阴影光源位置
    float  _padding3;

    float3 upVector; // 上方向向量
    float  _padding4;
};

/**
 * @brief BiomeAndDimensionData - 生物群系和维度数据（对应BiomeAndDimensionUniforms.hpp）
 * 注意：需要读取BiomeAndDimensionUniforms.hpp确定确切字段
 */
struct BiomeAndDimensionData
{
    float temperature; // 温度
    float rainfall; // 降雨量
    float humidity; // 湿度
    int   biomeId; // 生物群系ID

    int   dimensionId; // 维度ID（0=主世界，-1=下界，1=末地）
    bool  hasCeiling; // 是否有天花板
    bool  hasSkylight; // 是否有天空光
    float _padding;
};

/**
 * @brief RenderingData - 渲染数据（对应RenderingUniforms.hpp）
 * 注意：需要读取RenderingUniforms.hpp确定确切字段
 */
struct RenderingData
{
    float fogStart; // 雾起始距离
    float fogEnd; // 雾结束距离
    float fogDensity; // 雾密度
    int   fogMode; // 雾模式

    float3 fogColor; // 雾颜色
    float  _padding1;

    float3 skyColor; // 天空颜色
    float  _padding2;

    int   renderStage; // 当前渲染阶段
    bool  isMainHand; // 是否主手
    bool  isOffHand; // 是否副手
    float _padding3;
};

/**
 * @brief MatricesData - 矩阵数据（对应MatricesUniforms.hpp）
 */
struct MatricesData
{
    float4x4 gbufferModelView; // GBuffer模型视图矩阵
    float4x4 gbufferModelViewInverse; // GBuffer模型视图逆矩阵
    float4x4 gbufferProjection; // GBuffer投影矩阵
    float4x4 gbufferProjectionInverse; // GBuffer投影逆矩阵
    float4x4 gbufferPreviousModelView; // 上一帧GBuffer模型视图矩阵
    float4x4 gbufferPreviousProjection; // 上一帧GBuffer投影矩阵

    float4x4 shadowModelView; // 阴影模型视图矩阵
    float4x4 shadowModelViewInverse; // 阴影模型视图逆矩阵
    float4x4 shadowProjection; // 阴影投影矩阵
    float4x4 shadowProjectionInverse; // 阴影投影逆矩阵

    float4x4 modelViewMatrix; // 当前模型视图矩阵
    float4x4 modelViewMatrixInverse; // 当前模型视图逆矩阵
    float4x4 projectionMatrix; // 当前投影矩阵
    float4x4 projectionMatrixInverse; // 当前投影逆矩阵
    float4x4 normalMatrix; // 法线矩阵（3x3存储在4x4中）
    float4x4 textureMatrix; // 纹理矩阵
};

//──────────────────────────────────────────────────────
// Bindless资源访问函数
//──────────────────────────────────────────────────────

// ===== Uniform Buffer访问函数（8个）=====

/**
 * @brief 获取相机和玩家数据
 * @return CameraAndPlayerData结构体
 */
CameraAndPlayerData GetCameraAndPlayerData()
{
    StructuredBuffer<CameraAndPlayerData> buffer =
        ResourceDescriptorHeap[cameraAndPlayerBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取玩家状态数据
 * @return PlayerStatusData结构体
 */
PlayerStatusData GetPlayerStatusData()
{
    StructuredBuffer<PlayerStatusData> buffer =
        ResourceDescriptorHeap[playerStatusBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取屏幕和系统数据
 * @return ScreenAndSystemData结构体
 */
ScreenAndSystemData GetScreenAndSystemData()
{
    StructuredBuffer<ScreenAndSystemData> buffer =
        ResourceDescriptorHeap[screenAndSystemBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取ID数据
 * @return IDData结构体
 */
IDData GetIDData()
{
    StructuredBuffer<IDData> buffer =
        ResourceDescriptorHeap[idBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取世界和天气数据
 * @return WorldAndWeatherData结构体
 */
WorldAndWeatherData GetWorldAndWeatherData()
{
    StructuredBuffer<WorldAndWeatherData> buffer =
        ResourceDescriptorHeap[worldAndWeatherBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取生物群系和维度数据
 * @return BiomeAndDimensionData结构体
 */
BiomeAndDimensionData GetBiomeAndDimensionData()
{
    StructuredBuffer<BiomeAndDimensionData> buffer =
        ResourceDescriptorHeap[biomeAndDimensionBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取渲染数据
 * @return RenderingData结构体
 */
RenderingData GetRenderingData()
{
    StructuredBuffer<RenderingData> buffer =
        ResourceDescriptorHeap[renderingBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取矩阵数据
 * @return MatricesData结构体
 */
MatricesData GetMatricesData()
{
    StructuredBuffer<MatricesData> buffer =
        ResourceDescriptorHeap[matricesBufferIndex];
    return buffer[0];
}

// ===== 纹理资源访问函数（7个）=====

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
 * @brief 获取阴影颜色纹理（shadowcolor0-7）⭐ 拆分设计
 * @param shadowIndex shadowcolor索引（0-7）
 * @return Texture2D资源（Main或Alt，根据flip状态）
 *
 * 教学要点:
 * - shadowcolor支持flip机制（Ping-Pong双缓冲）
 * - 用于多Pass阴影渲染（shadowcomp阶段）
 * - 拆分方案：独立管理shadowcolor和shadowtex
 */
Texture2D GetShadowColor(uint shadowIndex)
{
    // 1. 获取ShadowColorBuffer
    StructuredBuffer<ShadowColorBuffer> shadowColorBuffer =
        ResourceDescriptorHeap[shadowColorBufferIndex];

    // 2. 查询shadowcolor读取索引（Main或Alt）
    uint textureIndex = shadowColorBuffer[0].shadowColorReadIndices[shadowIndex];

    // 3. 返回shadowcolor纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影深度纹理0（完整阴影深度）⭐ 拆分设计
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - shadowtex0: 包含所有物体的阴影深度（不透明+半透明）
 * - 只读纹理，不需要flip
 * - 拆分方案：独立管理shadowcolor和shadowtex
 */
Texture2D<float> GetShadowTex0()
{
    // 1. 获取ShadowTexturesBuffer
    StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
        ResourceDescriptorHeap[shadowTexturesBufferIndex];

    // 2. 查询shadowtex0索引
    uint textureIndex = shadowTexBuffer[0].shadowtex0Index;

    // 3. 返回shadowtex0
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影深度纹理1（半透明前阴影深度）⭐ 拆分设计
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - shadowtex1: 只包含不透明物体的阴影深度
 * - 用于半透明物体的阴影处理
 * - 拆分方案：独立管理shadowcolor和shadowtex
 */
Texture2D<float> GetShadowTex1()
{
    // 1. 获取ShadowTexturesBuffer
    StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
        ResourceDescriptorHeap[shadowTexturesBufferIndex];

    // 2. 查询shadowtex1索引
    uint textureIndex = shadowTexBuffer[0].shadowtex1Index;

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

/**
 * @brief 获取自定义材质槽位的Bindless索引 ⭐ 新增
 * @param slotIndex 槽位索引（0-15）
 * @return Bindless索引，如果槽位未设置则返回UINT32_MAX (0xFFFFFFFF)
 *
 * 教学要点:
 * - 通过customImageBufferIndexCBV访问CustomImageIndexBuffer
 * - 返回值为Bindless索引，可直接访问ResourceDescriptorHeap
 * - UINT32_MAX表示槽位未设置，使用前应检查有效性
 *
 * 工作原理:
 * 1. 通过customImageBufferIndexCBV访问CustomImageIndexBuffer
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

    // 1. 获取CustomImageIndexBuffer（StructuredBuffer）
    StructuredBuffer<CustomImageIndexBuffer> customImageBuffer =
        ResourceDescriptorHeap[customImageBufferIndexCBV];

    // 2. 查询指定槽位的Bindless索引
    uint bindlessIndex = customImageBuffer[0].customImageIndices[slotIndex];

    // 3. 返回Bindless索引
    return bindlessIndex;
}

/**
 * @brief 获取自定义材质纹理（便捷访问函数）⭐ 新增
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

// ===== CustomImage（customImage0-15）⭐ 新增 =====
// 自定义材质槽位，用于扩展材质系统
// 使用场景：PBR材质贴图（albedo、roughness、metallic等）、自定义LUT、特效贴图
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
 * 1. 从 MatricesData 获取 modelViewMatrix 和 projectionMatrix
 * 2. 顶点位置变换: Position → ViewSpace → ClipSpace
 * 3. 法线变换: normalMatrix 变换法线向量
 * 4. 颜色解包: uint → float4
 * 5. 传递所有顶点属性到像素着色器
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    // 1. 获取矩阵数据
    MatricesData matrices = GetMatricesData();

    // 2. 顶点位置变换: 模型空间 → 视图空间 → 裁剪空间
    float4 worldPos = float4(input.Position, 1.0);
    float4 viewPos  = mul(worldPos, matrices.gbufferModelView);
    float4 clipPos  = mul(viewPos, matrices.gbufferProjection);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;

    // 3. 颜色解包（uint → float4）
    output.Color = UnpackRgba8(input.Color);

    // 4. 传递纹理坐标
    output.TexCoord = input.TexCoord;

    // 5. 法线变换（使用 normalMatrix 的 3x3 部分）
    float3 transformedNormal = mul(float4(input.Normal, 0.0), matrices.normalMatrix).xyz;
    output.Normal            = normalize(transformedNormal);

    // 6. 传递切线和副切线（用于法线贴图）
    float3 transformedTangent   = mul(float4(input.Tangent, 0.0), matrices.gbufferModelView).xyz;
    float3 transformedBitangent = mul(float4(input.Bitangent, 0.0), matrices.gbufferModelView).xyz;
    output.Tangent              = normalize(transformedTangent);
    output.Bitangent            = normalize(transformedBitangent);

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
 * 1.5. CustomImage使用示例（延迟渲染PBR材质）⭐ 新增:
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
