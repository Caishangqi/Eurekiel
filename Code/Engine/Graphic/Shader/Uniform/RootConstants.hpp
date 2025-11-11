#pragma once

#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief Root Constants结构体 - 完整Iris纹理系统支持（激进48字节设计）
     *
     * 教学要点:
     * 1. 与Common.hlsl中的RootConstants cbuffer完全对应（48 bytes = 12个uint32_t）
     * 2. 存储12个Bindless索引，指向不同的GPU StructuredBuffer或Texture
     * 3. 每个索引占用4字节，可通过SetGraphicsRoot32BitConstant独立更新
     * 4. 完整支持Iris所有纹理类型：colortex, shadowcolor, depthtex, shadowtex, noisetex
     *
     * 架构优势（激进方案）:
     * - 极简设计：合并Shadow系统到单一索引（shadowBufferIndex）
     * - 细粒度控制：每个Buffer索引可独立更新
     * - Iris完全兼容：支持8大类Uniform + 5类纹理系统
     * - 极致性能：Root Signature全局共享，切换次数从1000次/帧降至1次/帧
     *
     * 纹理分类设计（激进合并）:
     * 1. 需要Flip的纹理（Ping-Pong双缓冲）:
     *    - colorTargets: colortex0-15（主渲染）
     *    - shadowBuffer: shadowcolor0-7（阴影渲染，flip部分）
     * 2. 不需要Flip的纹理（只读或引擎生成）:
     *    - depthTextures: depthtex0/1/2（引擎渲染过程生成）
     *    - shadowBuffer: shadowtex0/1（阴影深度，固定部分）
     *    - noiseTexture: 静态噪声纹理（RGB8, 256×256）
     *
     * 激进方案关键:
     * - shadowBufferIndex指向80字节的ShadowBufferIndex结构
     * - 同时包含shadowcolor（需要flip）+ shadowtex（固定）
     * - 减少Root Constants占用：从52字节降至48字节
     *
     * 对应HLSL (Common.hlsl):
     * ```hlsl
     * cbuffer RootConstants : register(b0, space0) {
     *     // Uniform Buffers (32 bytes)
     *     uint cameraAndPlayerBufferIndex;       // Offset 0
     *     uint playerStatusBufferIndex;          // Offset 4
     *     uint screenAndSystemBufferIndex;       // Offset 8
     *     uint idBufferIndex;                    // Offset 12
     *     uint worldAndWeatherBufferIndex;       // Offset 16
     *     uint biomeAndDimensionBufferIndex;     // Offset 20
     *     uint renderingBufferIndex;             // Offset 24
     *     uint matricesBufferIndex;              // Offset 28
     *
     *     // Texture Buffers with Flip Support (4 bytes)
     *     uint colorTargetsBufferIndex;          // Offset 32  colortex0-15
     *
     *     // Combined Buffers (8 bytes)
     *     uint depthTexturesBufferIndex;         // Offset 36 (depthtex0/1/2)
     *     uint shadowBufferIndex;                // Offset 40  shadowcolor0-7 + shadowtex0/1（激进合并）
     *
     *     // Direct Texture Index (4 bytes)
     *     uint noiseTextureIndex;                // Offset 44 (直接纹理索引)
     * };
     * ```
     *
     * 使用示例:
     * ```cpp
     * RootConstants constants;
     * constants.cameraAndPlayerBufferIndex = 10;
     * constants.colorTargetsBufferIndex = 25;        // ColorTargetsIndexBuffer (128 bytes)
     * constants.depthTexturesBufferIndex = 26;       // DepthTexturesIndexBuffer (16 bytes)
     * constants.shadowBufferIndex = 27;              // ShadowBufferIndex (80 bytes)  合并
     * constants.noiseTextureIndex = 5000;            // 直接指向noise texture
     *
     * // 完整更新
     * cmdList->SetGraphicsRoot32BitConstants(0, 12, &constants, 0);
     *
     * // 细粒度更新（只更新shadowBufferIndex）
     * cmdList->SetGraphicsRoot32BitConstant(
     *     0,
     *     constants.shadowBufferIndex,
     *     BindlessRootSignature::OFFSET_SHADOW_BUFFER_INDEX
     * );
     * ```
     *
     * @note 此结构体必须与BindlessRootSignature.hpp中的OFFSET_*常量严格对应
     * @note 此结构体必须与Common.hlsl中的RootConstants cbuffer严格对应
     */
    struct RootConstants
    {
        /**
         * @brief Camera/Player Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - cameraPosition, previousCameraPosition, eyePosition
         * - eyeBrightness, eyeBrightnessSmooth
         * - centerDepthSmooth, playerMood
         * - shadowLightPosition, upPosition
         *
         * 对应HLSL Offset: 0 (BindlessRootSignature::OFFSET_CAMERA_AND_PLAYER_BUFFER_INDEX)
         */
        uint32_t cameraAndPlayerBufferIndex;

        /**
         * @brief Player Status Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - isEyeInWater, hideGUI, blindness, nightVision
         * - heldItemId, heldBlockLightValue
         * - currentPlayerHealth, maxPlayerHealth
         * - currentPlayerAir, currentPlayerHunger等
         *
         * 对应HLSL Offset: 1 (BindlessRootSignature::OFFSET_PLAYER_STATUS_BUFFER_INDEX)
         */
        uint32_t playerStatusBufferIndex;

        /**
         * @brief Screen/System Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - screenSize, viewWidth, viewHeight, aspectRatio
         * - nearPlane, farPlane
         * - frameTime, frameTimeCounter, frameCounter
         *
         * 对应HLSL Offset: 2 (BindlessRootSignature::OFFSET_SCREEN_AND_SYSTEM_BUFFER_INDEX)
         */
        uint32_t screenAndSystemBufferIndex;

        /**
         * @brief ID Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - entityId, blockEntityId, currentRenderedItemId
         * - terrainIconSize, terrainTextureSize, atlasSize
         * - entityColor, blockEntityColor
         *
         * 对应HLSL Offset: 3 (BindlessRootSignature::OFFSET_ID_BUFFER_INDEX)
         */
        uint32_t idBufferIndex;

        /**
         * @brief World/Weather Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - worldTime, worldDay, moonPhase
         * - sunAngle, shadowAngle, celestialAngle
         * - rainStrength, wetness, thunderStrength
         * - skyColor, fogColor
         *
         * 对应HLSL Offset: 4 (BindlessRootSignature::OFFSET_WORLD_AND_WEATHER_BUFFER_INDEX)
         */
        uint32_t worldAndWeatherBufferIndex;

        /**
         * @brief Biome/Dimension Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - temperature, humidity, biome, biomeCategory
         * - dimension, hasSkylight, hasCeiling
         * - biomePrecipitation, precipitationType
         * - biomeFogColor, biomeWaterColor
         * - fogDensity, fogStart
         *
         * 对应HLSL Offset: 5 (BindlessRootSignature::OFFSET_BIOME_AND_DIMENSION_BUFFER_INDEX)
         */
        uint32_t biomeAndDimensionBufferIndex;

        /**
         * @brief Rendering Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - ambientOcclusionLevel, renderStage
         * - lightningBolt, bedrockLevel, heightLimit
         * - alphaTestRef, blendFunc
         * - chunkOffset, screenBrightness, gamma, fogMode
         *
         * 对应HLSL Offset: 6 (BindlessRootSignature::OFFSET_RENDERING_BUFFER_INDEX)
         */
        uint32_t renderingBufferIndex;

        /**
         * @brief Matrices Uniforms Buffer索引
         *
         * 指向的Buffer包含:
         * - gbufferModelView, gbufferModelViewInverse
         * - gbufferProjection, gbufferProjectionInverse
         * - shadowModelView, shadowProjection
         * - mvpMatrix, textureMatrix, normalMatrix等
         *
         * 对应HLSL Offset: 7 (BindlessRootSignature::OFFSET_MATRICES_BUFFER_INDEX)
         */
        uint32_t matricesBufferIndex;

        /**
         * @brief ColorTargets Buffer索引  主渲染核心
         *
         * 指向的Buffer包含（ColorTargetsIndexBuffer，128 bytes）:
         * - readIndices[16]: colortex0-15读取索引（Main或Alt）
         * - writeIndices[16]: colortex0-15写入索引（预留）
         *
         * 教学要点:
         * 1. 这是主渲染管线的核心索引
         * 2. readIndices根据flip状态自动指向Main或Alt纹理
         * 3. 消除90%+ ResourceBarrier开销
         *
         * 对应HLSL Offset: 8 (BindlessRootSignature::OFFSET_COLOR_TARGETS_BUFFER_INDEX)
         */
        uint32_t colorTargetsBufferIndex;

        /**
         * @brief DepthTextures Buffer索引
         *
         * 指向的Buffer包含（DepthTexturesIndexBuffer，16 bytes）:
         * - depthtex0Index: 完整深度（包含半透明+手部）
         * - depthtex1Index: 半透明前深度（no translucents）
         * - depthtex2Index: 手部前深度（no hand）
         * - padding: 对齐填充
         *
         * 教学要点:
         * 1. 所有深度纹理由引擎渲染过程自动生成
         * 2. 不需要flip机制（每帧独立生成）
         * 3. 用于后处理和深度测试
         *
         * 对应HLSL Offset: 9 (BindlessRootSignature::OFFSET_DEPTH_TEXTURES_BUFFER_INDEX)
         */
        uint32_t depthTexturesBufferIndex;

        /**
         * @brief Shadow Color Buffer索引
         * - shadowcolor0Index:
         * - shadowcolor1Index:
         * - to shadowsolor7Index:
         */
        uint32_t shadowColorBufferIndex;

        /**
         * @brief Shadow Texture Buffer索引
         * - shadowtex0Index:
         * - shadowtex1Index:
         * - shadowtex2Index:
         */
        uint32_t shadowTexturesBufferIndex;

        /**
         * @brief NoiseTexture直接索引
         *
         * 直接指向noisetex的Bindless纹理索引（不是Buffer，是Texture2D）
         *
         * 教学要点:
         * 1. 静态噪声纹理，RGB8格式，默认256×256
         * 2. 用于随机采样、抖动、时间相关效果
         * 3. 不需要Buffer包装，直接使用纹理索引
         *
         * 对应HLSL Offset: 11 (BindlessRootSignature::OFFSET_NOISE_TEXTURE_INDEX)
         */
        uint32_t noiseTextureIndex;

        /**
         * @brief CustomImage Index Buffer索引  自定义材质支持
         *
         * 指向的Buffer包含（CustomImageIndexBuffer，256 bytes）:
         * - customImageIndices[16]: customImage0-15的Bindless索引
         * - padding[48]: 对齐填充到256字节
         *
         * 教学要点:
         * 1. 支持16个自定义材质槽位，用户可上传任意纹理
         * 2. 通过UploadCustomTexture() API动态填充
         * 3. HLSL端通过customImage0-15宏直接访问
         * 4. 支持运行时动态更新和替换
         *
         * 对应HLSL Offset: 12 (新增，需要更新BindlessRootSignature.hpp)
         */
        uint32_t customImageBufferIndex;

        // ===== 总计: 14 × 4 = 56 bytes =====

        /**
         * @brief 默认构造函数 - 初始化为0
         */
        RootConstants();
    };

    // 编译期验证: 确保结构体大小为56字节
    static_assert(sizeof(RootConstants) == 56,
                  "RootConstants must be exactly 56 bytes to match HLSL RootConstants cbuffer");

    // 编译期验证: 确保每个字段都是4字节对齐
    static_assert(alignof(RootConstants) == 4,
                  "RootConstants must be 4-byte aligned for SetGraphicsRoot32BitConstants");
} // namespace enigma::graphic
