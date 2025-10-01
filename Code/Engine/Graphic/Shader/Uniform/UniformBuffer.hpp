#pragma once

#include <cstdint>
#include <d3d12.h>
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"

namespace enigma::graphic
{
    /**
     * @brief Render Target 信息封装 - GPU端Bindless Buffer
     *
     * 核心设计 (基于Iris源码分析 - RenderTarget.java):
     * 1. 每个RenderTarget有2个纹理: Main + Alt (Ping-Pong机制)
     * 2. Iris支持16个RenderTarget (colortex0-15)
     * 3. 总计32个纹理 (16 Main + 16 Alt)
     *
     * 教学要点:
     * 1. Ping-Pong渲染: Main和Alt交替作为读/写目标,避免读写冲突
     * 2. Iris架构: stageWritesToMain决定写Main还是Alt (RenderTargets.java:355)
     * 3. Bindless优化: 只用4字节索引指向整个RenderTarget信息封装
     *
     * Iris源码参考:
     * - RenderTarget.java:39-47 - 创建Main/Alt纹理对
     * - RenderTargets.java:355 - Ping-Pong切换逻辑
     * - https://shaders.properties/current/reference/buffers/overview/
     *
     * 内存布局 (总计128字节):
     * - [0-63]:  colortex0-15 Main纹理Bindless索引 (16 × 4 bytes)
     * - [64-127]: colortex0-15 Alt纹理Bindless索引 (16 × 4 bytes)
     */
    struct RenderTargetInfo
    {
        // colortex0-15 Main纹理索引 (16个)
        // 教学要点: Main纹理作为主渲染目标,通常用于正向渲染Pass
        uint32_t colorTexMainIndices[16];

        // colortex0-15 Alt纹理索引 (16个)
        // 教学要点: Alt纹理用于Ping-Pong,避免读写同一纹理导致的未定义行为
        uint32_t colorTexAltIndices[16];

        // 总计: 32 × 4 = 128 bytes ✓
    };

    // 编译期验证: 确保结构体大小为128字节
    static_assert(sizeof(RenderTargetInfo) == 128, "RenderTargetInfo must be exactly 128 bytes for efficient GPU access");

    /**
     * @brief Root Constants - 128字节纯Bindless索引布局 (修订版2)
     *
     * 核心架构设计 (Milestone 3.1 批准方案 + Iris RenderTarget分析):
     * 1. Root Constants = 纯Bindless索引 (32 uint32_t slots)
     * 2. IrisUniformBuffer = GPU Bindless Buffer (~1352 bytes, 98个Iris uniforms)
     * 3. RenderTargetInfo = GPU Bindless Buffer (128 bytes, 32个纹理索引)
     * 4. 所有大数据存储在GPU Buffer, Root Constants只传索引
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构: Root Signature极简化,无Descriptor Table
     * 2. 128字节限制: D3D12_MAX_ROOT_COST = 64 DWORDS (256 bytes总预算, Root Constants占一半)
     * 3. Root Constants "快照"特性: 每次SetGraphicsRoot32BitConstants创建独立数据副本
     * 4. 与传统Descriptor Table区别: 不记录地址,而是数据副本,避免"覆盖"问题
     *
     * 性能优势:
     * - Root Signature切换: 从1000次/帧降至1次/帧 (99.9%优化)
     * - 资源容量: 从数千提升至1,000,000+ (100倍提升)
     * - RenderTarget管理: 从76字节优化到4字节 (95%空间节省)
     *
     * 内存布局 (总计128字节 = 32 uint32_t):
     * - [0-15]:   高频数据 (frameTime, frameCounter, entityId, blockId) - 每帧/物体变化
     * - [16-47]:  核心封装索引 (irisUniformBufferIndex, renderTargetInfoIndex) + 深度/阴影纹理
     * - [48-127]: 预留扩展 (reserved) - 未来功能预留
     */
    struct RootConstants
    {
        // ===== 高频数据 (16 bytes = 4 uint32_t) =====
        // 教学要点: 这些数据每帧或每个物体都会变化,放在Root Constants减少GPU访问开销
        float    frameTime; // 当前帧时间 (秒) - 对应Iris frameTimeCounter
        uint32_t frameCounter; // 帧计数器 - 对应Iris frameCounter
        uint32_t entityId; // 当前绘制实体ID - 对应Iris entityId
        uint32_t blockId; // 当前绘制方块ID - 对应Iris blockEntityId

        // ===== 核心封装索引 (32 bytes = 8 uint32_t) =====
        // 关键设计: 只用2个索引指向所有Iris uniforms和RenderTargets
        uint32_t irisUniformBufferIndex; // 指向IrisUniformBuffer (98个uniforms)
        uint32_t renderTargetInfoIndex; // NEW: 指向RenderTargetInfo (32个纹理)

        // 深度纹理索引 (3个深度纹理,直接访问)
        uint32_t depthTex0Index; // depthtex0 - 主深度缓冲区
        uint32_t depthTex1Index; // depthtex1 - 半透明前深度副本
        uint32_t depthTex2Index; // depthtex2 - 无手部深度副本

        // 阴影纹理索引 (2个阴影纹理)
        uint32_t shadowTex0Index; // shadowtex0 - 阴影深度图
        uint32_t shadowTex1Index; // shadowtex1 - 半透明阴影深度

        // 噪声纹理索引 (1个噪声纹理)
        uint32_t noisetexIndex; // noisetex - 噪声纹理 (用于随机采样)

        // ===== 预留扩展 (80 bytes = 20 uint32_t) =====
        // 教学要点: 大幅增加预留空间,未来可添加更多封装 (如MaterialInfo, LightInfo等)
        uint32_t reserved[20];

        // 总计: 16 + 32 + 80 = 128 bytes ✓
    };

    // 编译期验证: 确保结构体大小为128字节
    static_assert(sizeof(RootConstants) == 128, "RootConstants must be exactly 128 bytes for DirectX 12 Root Constants limit");

    /**
     * @brief Iris Uniform Buffer - GPU端Bindless Buffer存储的完整Uniform数据
     *
     * 🔥 核心设计: 所有98个Iris uniforms存储在GPU Bindless Buffer
     *
     * 教学要点:
     * 1. 数据存储: 使用D3D12_RESOURCE_STATE_GENERIC_READ的StructuredBuffer
     * 2. 对齐要求: Vec3/Vec4使用alignas(16), Vec2使用alignas(8), float/int自然4字节对齐
     * 3. Iris兼容性: 字段名称和语义与Iris完全一致
     * 4. 访问方式: HLSL中通过ResourceDescriptorHeap[irisUniformBufferIndex][0]访问
     *
     * HLSL访问示例:
     * ```hlsl
     * cbuffer RootConstants : register(b0)
     * {
     *     float frameTime;
     *     uint irisUniformBufferIndex;
     *     // ... 其他Root Constants字段
     * };
     *
     * StructuredBuffer<IrisUniformBuffer> irisUniformBuffer = ResourceDescriptorHeap[irisUniformBufferIndex];
     * float3 cameraPos = irisUniformBuffer[0].cameraPosition;
     * float sunAngle = irisUniformBuffer[0].sunAngle;
     * ```
     *
     * Iris源码参考:
     * - https://shaders.properties/current/reference/uniforms/overview/
     * - 8大类: Camera/Player, Player Status, Screen/System, ID, World/Weather, Biome/Dimension, Rendering, Matrices
     * - 共98个uniform变量
     *
     * 编译器警告压制:
     * - C4324: 结构体因alignas而填充 (预期行为,用于HLSL兼容性)
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 压制C4324警告 - 结构体填充是预期行为
    struct IrisUniformBuffer
    {
        // =========================================================================
        // Camera/Player Uniforms (12个字段, ~48 bytes)
        // =========================================================================
        alignas(16) Vec3 cameraPosition; // 相机世界坐标 - vec3 cameraPosition
        alignas(16) Vec3 previousCameraPosition; // 上一帧相机坐标 - vec3 previousCameraPosition
        alignas(16) Vec3 eyePosition; // 眼睛位置 (第三人称偏移) - vec3 eyePosition

        alignas(8) Vec2 eyeBrightness; // 眼睛亮度 (天空光, 方块光) - vec2 eyeBrightness
        alignas(8) Vec2 eyeBrightnessSmooth; // 平滑眼睛亮度 - vec2 eyeBrightnessSmooth

        alignas(4) float centerDepthSmooth; // 平滑中心深度 - float centerDepthSmooth
        alignas(4) float playerMood; // 玩家情绪值 (0-1) - float playerMood

        alignas(16) Vec3 shadowLightPosition; // 阴影光源位置 - vec3 shadowLightPosition
        alignas(16) Vec3 upPosition; // 上方向向量 - vec3 upPosition

        // =========================================================================
        // Player Status Uniforms (16个字段, ~64 bytes)
        // =========================================================================
        float eyeAltitude; // 眼睛海拔高度 - float eyeAltitude
        int   isEyeInWater; // 眼睛是否在水中 (0/1/2: 否/是/熔岩) - int isEyeInWater
        int   hideGUI; // 是否隐藏GUI (0/1) - int hideGUI
        float blindness; // 失明效果强度 (0-1) - float blindness
        float nightVision; // 夜视效果强度 (0-1) - float nightVision
        float darknessFactor; // 黑暗效果因子 - float darknessFactor
        float darknessLightFactor; // 黑暗光照因子 - float darknessLightFactor

        int heldItemId; // 手持物品ID - int heldItemId
        int heldBlockLightValue; // 手持方块光照值 - int heldBlockLightValue
        int heldItemId2; // 副手物品ID - int heldItemId2
        int heldBlockLightValue2; // 副手方块光照值 - int heldBlockLightValue2

        float currentPlayerHealth; // 当前玩家血量 - float currentPlayerHealth
        float maxPlayerHealth; // 最大玩家血量 - float maxPlayerHealth
        float currentPlayerAir; // 当前玩家氧气 - float currentPlayerAir
        float maxPlayerAir; // 最大玩家氧气 - float maxPlayerAir
        int   currentPlayerHunger; // 当前玩家饥饿值 - int currentPlayerHunger
        float currentPlayerSaturation; // 当前玩家饱和度 - float currentPlayerSaturation

        // =========================================================================
        // Screen/System Uniforms (10个字段, ~40 bytes)
        // =========================================================================
        alignas(8) Vec2 screenSize; // 屏幕尺寸 (width, height) - vec2 viewSize
        float           viewWidth; // 屏幕宽度 - float viewWidth
        float           viewHeight; // 屏幕高度 - float viewHeight
        float           aspectRatio; // 屏幕宽高比 - float aspectRatio

        float nearPlane; // 近裁剪面 - float near (重命名避免关键字冲突)
        float farPlane; // 远裁剪面 - float far (重命名避免关键字冲突)

        float frameTime; // 当前帧时间 (秒) - float frameTime
        float frameTimeCounter; // 帧时间累计 - float frameTimeCounter
        int   frameCounter; // 帧计数器 - int frameCounter

        // =========================================================================
        // ID Uniforms (8个字段, ~32 bytes)
        // =========================================================================
        int entityId; // 当前实体ID - int entityId
        int blockEntityId; // 当前方块实体ID - int blockEntityId
        int currentRenderedItemId; // 当前渲染物品ID - int currentRenderedItemId

        int             terrainIconSize; // 地形图标大小 - int terrainIconSize
        int             terrainTextureSize; // 地形纹理大小 - int terrainTextureSize
        alignas(8) Vec2 atlasSize; // 图集尺寸 - vec2 atlasSize

        int entityColor; // 实体颜色 (RGB packed) - int entityColor
        int blockEntityColor; // 方块实体颜色 - int blockEntityColor

        // =========================================================================
        // World/Weather Uniforms (11个字段, ~44 bytes)
        // =========================================================================
        int worldTime; // 世界时间 (tick) - int worldTime
        int worldDay; // 世界天数 - int worldDay
        int moonPhase; // 月相 (0-7) - int moonPhase

        float sunAngle; // 太阳角度 (弧度) - float sunAngle
        float shadowAngle; // 阴影角度 (弧度) - float shadowAngle
        float celestialAngle; // 天体角度 (0-1) - float celestialAngle

        float rainStrength; // 雨强度 (0-1) - float rainStrength
        float wetness; // 湿度 (0-1) - float wetness

        float thunderStrength; // 雷暴强度 (0-1) - float thunderStrength

        alignas(16) Vec3 skyColor; // 天空颜色 - vec3 skyColor
        alignas(16) Vec3 fogColor; // 雾颜色 - vec3 fogColor

        // =========================================================================
        // Biome/Dimension Uniforms (13个字段, ~52 bytes)
        // =========================================================================
        float temperature; // 生物群系温度 - float temperature
        float humidity; // 生物群系湿度 - float humidity
        int   biome; // 生物群系ID - int biome
        int   biomeCategory; // 生物群系类别 - int biomeCategory

        int dimension; // 维度ID (0=主世界, -1=下界, 1=末地) - int dimension
        int hasSkylight; // 是否有天空光 (0/1) - int hasSkylight
        int hasCeiling; // 是否有天花板 (0/1) - int hasCeiling

        float biomePrecipitation; // 生物群系降水强度 - float biomePrecipitation
        int   precipitationType; // 降水类型 (0=无, 1=雨, 2=雪) - int precipitationType

        alignas(16) Vec3 biomeFogColor; // 生物群系雾颜色 - vec3 biomeFogColor
        alignas(16) Vec3 biomeWaterColor; // 生物群系水颜色 - vec3 biomeWaterColor

        float fogDensity; // 雾密度 - float fogDensity
        float fogStart; // 雾起始距离 - float fogStart

        // =========================================================================
        // Rendering Uniforms (12个字段, ~48 bytes)
        // =========================================================================
        float ambientOcclusionLevel; // 环境光遮蔽级别 - float ambientOcclusionLevel
        int   renderStage; // 当前渲染阶段 (0-24) - int renderStage

        float lightningBolt; // 闪电强度 (0-1) - float lightningBolt
        int   bedrockLevel; // 基岩层高度 - int bedrockLevel
        int   heightLimit; // 世界高度限制 - int heightLimit
        int   logicalHeightLimit; // 逻辑高度限制 - int logicalHeightLimit

        float alphaTestRef; // Alpha测试参考值 - float alphaTestRef
        int   blendFunc; // 混合函数 - int blendFunc

        alignas(16) Vec3 chunkOffset; // 区块偏移 - vec3 chunkOffset

        float screenBrightness; // 屏幕亮度 - float screenBrightness
        float gamma; // Gamma值 - float gamma
        int   fogMode; // 雾模式 (0=linear, 1=exp, 2=exp2) - int fogMode

        // =========================================================================
        // Matrices (16个字段, ~1024 bytes)
        // =========================================================================
        // 教学要点: 矩阵使用alignas(16)确保16字节对齐 (HLSL float4x4要求)

        // GBuffer矩阵 (主渲染Pass)
        alignas(16) Mat44 gbufferModelView; // 模型视图矩阵 - mat4 gbufferModelView
        alignas(16) Mat44 gbufferModelViewInverse; // 模型视图逆矩阵 - mat4 gbufferModelViewInverse
        alignas(16) Mat44 gbufferProjection; // 投影矩阵 - mat4 gbufferProjection
        alignas(16) Mat44 gbufferProjectionInverse; // 投影逆矩阵 - mat4 gbufferProjectionInverse

        alignas(16) Mat44 gbufferPreviousModelView; // 上一帧模型视图 - mat4 gbufferPreviousModelView
        alignas(16) Mat44 gbufferPreviousProjection; // 上一帧投影 - mat4 gbufferPreviousProjection

        // Shadow矩阵 (阴影Pass)
        alignas(16) Mat44 shadowModelView; // 阴影模型视图矩阵 - mat4 shadowModelView
        alignas(16) Mat44 shadowModelViewInverse; // 阴影模型视图逆矩阵 - mat4 shadowModelViewInverse
        alignas(16) Mat44 shadowProjection; // 阴影投影矩阵 - mat4 shadowProjection
        alignas(16) Mat44 shadowProjectionInverse; // 阴影投影逆矩阵 - mat4 shadowProjectionInverse

        // 辅助矩阵
        alignas(16) Mat44 modelViewMatrix; // 当前模型视图 (动态) - mat4 modelViewMatrix
        alignas(16) Mat44 projectionMatrix; // 当前投影 (动态) - mat4 projectionMatrix
        alignas(16) Mat44 mvpMatrix; // MVP = Projection * View * Model

        alignas(16) Mat44 textureMatrix; // 纹理矩阵 - mat4 textureMatrix
        alignas(16) Mat44 normalMatrix; // 法线矩阵 (3x3存储在4x4中) - mat3 normalMatrix
        alignas(16) Mat44 colorModulator; // 颜色调制矩阵 - mat4 colorModulator

        // 总计: ~1352+ bytes (包含所有98个Iris uniforms)
    };
#pragma warning(pop) // 恢复C4324警告

    // 编译期验证: 确保结构体不超过合理大小限制 (2KB)
    static_assert(sizeof(IrisUniformBuffer) <= 2048,
                  "IrisUniformBuffer exceeds 2KB, consider splitting into multiple buffers");

    /**
     * @brief Uniform缓冲区管理器 (修订版 - Milestone 3.1)
     *
     * 🔥 核心架构变更:
     * 1. 管理RootConstants (128字节CPU端数据)
     * 2. 管理IrisUniformBuffer (1352字节GPU Bindless Buffer)
     * 3. 自动同步CPU→GPU数据 (每帧UpdateAll()调用)
     *
     * 教学要点:
     * 1. 双层管理: Root Constants (CPU端) + IrisUniformBuffer (GPU端)
     * 2. 每帧更新: 通过SetGraphicsRoot32BitConstants()传递Root Constants
     * 3. GPU Buffer: 使用D3D12_RESOURCE_STATE_GENERIC_READ的StructuredBuffer
     * 4. Bindless注册: IrisUniformBuffer上传后自动注册到全局描述符堆
     *
     * Iris对应类:
     * - CommonUniforms.java: Uniform数据管理
     * - FrameUpdateNotifier.java: 帧更新通知
     *
     * 使用流程:
     * ```cpp
     * UniformBuffer uniformBuffer;
     *
     * // 初始化 (创建GPU Buffer并注册Bindless)
     * uniformBuffer.Initialize(device);
     *
     * // 每帧更新
     * uniformBuffer.UpdateFrameTime(frameTime);
     * uniformBuffer.UpdateCameraPosition(cameraPos);
     * uniformBuffer.UpdateMatrices(mvp, modelView, projection);
     * uniformBuffer.UpdateAll();  // 同步到GPU
     *
     * // 传递到GPU (只需128字节Root Constants)
     * shaderProgram->SetRootConstants(commandList, uniformBuffer.GetRootConstants(), 128);
     * ```
     */
    class UniformBuffer
    {
    public:
        /**
         * @brief Root Constants大小限制
         *
         * 教学要点: DX12硬件限制
         * - D3D12_MAX_ROOT_COST = 64 DWORDS (256 bytes总预算)
         * - Root Constants占用: 1 DWORD per 4 bytes
         * - 128 bytes = 32 DWORDS (占用一半预算)
         */
        static constexpr size_t ROOT_CONSTANTS_SIZE = 128;

        /**
         * @brief GPU Uniform Buffer大小
         *
         * 教学要点: IrisUniformBuffer存储在GPU Bindless Buffer
         */
        static constexpr size_t GPU_UNIFORM_SIZE = sizeof(IrisUniformBuffer);

        /**
         * @brief 构造函数 - 初始化默认值
         *
         * 教学要点: 初始化所有字段为合理默认值，避免未定义行为
         */
        UniformBuffer();

        /**
         * @brief 析构函数 - 释放GPU资源
         *
         * 教学要点: 自动注销Bindless索引,释放GPU Buffer
         */
        ~UniformBuffer();

        // ========================================================================
        // 初始化接口
        // ========================================================================

        /**
         * @brief 初始化Uniform系统 (创建GPU Buffer并注册Bindless)
         * @param device DX12设备
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 创建GPU StructuredBuffer (D3D12_RESOURCE_STATE_GENERIC_READ)
         * 2. 上传IrisUniformBuffer初始数据到GPU
         * 3. 注册到Bindless系统,获取irisUniformBufferIndex
         */
        bool Initialize();

        /**
         * @brief 检查是否已初始化
         * @return 已初始化返回true
         */
        bool IsInitialized() const;

        // ========================================================================
        // Uniform更新接口 - 高频数据 (Root Constants)
        // ========================================================================

        /**
         * @brief 更新帧时间
         * @param frameTime 当前帧时间 (秒)
         * @param frameCounter 帧计数器
         */
        void UpdateFrameTime(float frameTime, uint32_t frameCounter);

        /**
         * @brief 更新实体ID
         * @param entityId 实体ID
         * @param blockId 方块ID
         */
        void UpdateEntityID(uint32_t entityId, uint32_t blockId);

        // ========================================================================
        // Uniform更新接口 - 相机/玩家 (IrisUniformBuffer)
        // ========================================================================

        /**
         * @brief 更新相机位置
         * @param position 相机世界坐标
         */
        void UpdateCameraPosition(const Vec3& position);
        void UpdateViewMatrices(const Mat44& modelView, const Mat44& projection);

        /**
         * @brief 更新上一帧相机位置
         * @param previousPosition 上一帧相机坐标
         */
        void UpdatePreviousCameraPosition(const Vec3& previousPosition);

        /**
         * @brief 更新眼睛位置
         * @param eyePosition 眼睛位置 (第三人称偏移)
         */
        void UpdateEyePosition(const Vec3& eyePosition);

        // ========================================================================
        // Uniform更新接口 - 矩阵 (IrisUniformBuffer)
        // ========================================================================

        /**
         * @brief 更新MVP矩阵
         * @param mvp MVP矩阵 (Projection * View * Model)
         */
        void UpdateMVPMatrix(const Mat44& mvp);

        /**
         * @brief 更新模型视图矩阵
         * @param modelView 模型视图矩阵
         */
        void UpdateModelViewMatrix(const Mat44& modelView);

        /**
         * @brief 更新投影矩阵
         * @param projection 投影矩阵
         */
        void UpdateProjectionMatrix(const Mat44& projection);

        // ========================================================================
        // Uniform更新接口 - 世界/天气 (IrisUniformBuffer)
        // ========================================================================

        /**
         * @brief 更新太阳/阴影角度
         * @param sunAngle 太阳角度 (弧度)
         * @param shadowAngle 阴影角度 (弧度)
         */
        void UpdateSunAngles(float sunAngle, float shadowAngle);

        /**
         * @brief 更新世界时间
         * @param worldTime 世界时间 (tick)
         * @param worldDay 世界天数
         */
        void UpdateWorldTime(int worldTime, int worldDay);
        void UpdateCelestialAngles(float sunAngle, float shadowAngle, int32_t moonPhase);

        // ========================================================================
        // Bindless资源索引更新接口
        // ========================================================================

        /**
         * @brief 更新RenderTarget信息 (colortex0-15 Main/Alt纹理)
         * @param colorTexMainIndices colortex0-15 Main纹理索引数组 (16个)
         * @param colorTexAltIndices colortex0-15 Alt纹理索引数组 (16个)
         *
         * 教学要点:
         * 1. Iris Ping-Pong机制: Main/Alt交替作为读/写目标
         * 2. 自动上传到GPU RenderTargetInfo Buffer
         * 3. Root Constants中只存储renderTargetInfoIndex (4字节)
         */
        void UpdateRenderTargetInfo(
            const uint32_t colorTexMainIndices[16],
            const uint32_t colorTexAltIndices[16]
        );

        /**
         * @brief 更新深度/阴影/噪声纹理索引 (Root Constants直接存储)
         * @param depthTexIndices depthtex0-2索引数组 (3个)
         * @param shadowTexIndices shadowtex0-1索引数组 (2个)
         * @param noisetexIndex noisetex索引
         *
         * 教学要点: 这些纹理数量少,直接存储在Root Constants
         */
        void UpdateDepthShadowNoiseIndices(
            const uint32_t depthTexIndices[3],
            const uint32_t shadowTexIndices[2],
            uint32_t       noisetexIndex
        );

        // ========================================================================
        // 批量更新接口
        // ========================================================================

        /**
         * @brief 更新所有Uniform到GPU (每帧必须调用)
         *
         * 教学要点:
         * 1. 将IrisUniformBuffer数据上传到GPU Buffer
         * 2. 更新Root Constants中的irisUniformBufferIndex
         * 3. 标记数据已同步
         */
        bool UpdateAll();

        // ========================================================================
        // 数据访问接口
        // ========================================================================

        /**
         * @brief 获取Root Constants数据指针 (用于SetGraphicsRoot32BitConstants)
         * @return 指向RootConstants的指针
         *
         * 教学要点: 只读访问，传递给SetGraphicsRoot32BitConstants()
         */
        const RootConstants* GetRootConstants() const { return &m_rootConstants; }

        /**
         * @brief 获取Root Constants大小 (应始终为128)
         * @return 数据大小 (字节)
         */
        size_t GetRootConstantsSize() const { return sizeof(RootConstants); }

        /**
         * @brief 获取GPU Uniform Buffer指针 (调试用)
         * @return 指向IrisUniformBuffer的指针
         */
        const IrisUniformBuffer* GetGPUUniformData() const { return &m_gpuUniformData; }

        /**
         * @brief 获取GPU Uniform Buffer大小
         * @return 数据大小 (字节)
         */
        size_t GetGPUUniformSize() const { return sizeof(IrisUniformBuffer); }

        /**
         * @brief 获取IrisUniformBuffer的Bindless索引
         * @return Bindless索引
         */
        uint32_t GetIrisUniformBufferIndex() const;

        /**
         * @brief 获取RenderTargetInfo的Bindless索引
         * @return Bindless索引
         *
         * 教学要点: 这个索引存储在Root Constants的renderTargetInfoIndex字段
         */
        uint32_t GetRenderTargetInfoIndex() const;

        /**
         * @brief 重置为默认值
         *
         * 教学要点: 用于场景切换或管线重载
         */
        void Reset();

    private:
        // CPU端数据
        RootConstants     m_rootConstants; // Root Constants (128字节)
        IrisUniformBuffer m_gpuUniformData; // GPU Uniform Buffer (1352字节)
        RenderTargetInfo  m_renderTargetInfo; // GPU RenderTarget Info (128字节)

        // GPU资源 (Bindless Buffer)
        class D12Buffer* m_gpuUniformBuffer; // GPU端IrisUniformBuffer StructuredBuffer
        class D12Buffer* m_renderTargetInfoBuffer; // GPU端RenderTargetInfo StructuredBuffer
        bool             m_isInitialized; // 初始化标记
        bool             m_isDirty; // IrisUniformBuffer数据脏标记
        bool             m_renderTargetInfoDirty; // RenderTargetInfo数据脏标记

        // 禁用拷贝 (遵循RAII原则)
        UniformBuffer(const UniformBuffer&)            = delete;
        UniformBuffer& operator=(const UniformBuffer&) = delete;
    };
} // namespace enigma::graphic
