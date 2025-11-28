/**
 * @file ShaderDirectives.hpp
 * @brief Iris 着色器注释指令 - 数据容器 + 解析器
 * @date 2025-10-03
 *
 * 对应 Iris ProgramDirectives.java
 * 职责:
 * 1. 存储从着色器注释解析出的配置信息
 * 2. 提供静态解析方法从 HLSL 源码提取指令
 *
 * HLSL 注释示例:
 * hlsl
 * RENDERTARGETS: 0,1,2
 * DRAWBUFFERS: 0123
 * const int colortex0Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
 * const bool shadowHardwareFiltering = true;
 * 
 */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <tuple>
#include <d3d12.h>

namespace enigma::graphic
{
    /**
     * @struct ShaderDirectives
     * @brief Iris 着色器注释指令容器
     *
     * 教学要点:
     * - 对应 Iris 的 ProgramDirectives.java
     * - 存储从着色器注释解析的所有配置
     * - 影响渲染状态、RT 格式、计算着色器配置等
     */
    struct ShaderDirectives
    {
        // ========================================================================
        // 渲染目标配置
        // ========================================================================

        std::vector<uint32_t> renderTargets; // RENDERTARGETS: 0,1,2,3
        std::string           drawBuffers; // DRAWBUFFERS: 0123 (兼容 OptiFine)

        // ========================================================================
        // RT 格式配置
        // ========================================================================

        std::unordered_map<std::string, DXGI_FORMAT>             rtFormats; // colortex0Format: DXGI_FORMAT_R16G16B16A16_FLOAT
        std::unordered_map<std::string, std::pair<float, float>> rtSizes; // GAUX1SIZE: 0.5 0.5 (相对尺寸)

        // ========================================================================
        // 渲染状态配置
        // ========================================================================

        std::optional<std::string> blendMode; // BLEND: SrcAlpha OneMinusSrcAlpha
        std::optional<std::string> depthTest; // DEPTHTEST: LessEqual
        std::optional<bool>        depthWrite; // DEPTHWRITE: false
        std::optional<std::string> cullFace; // CULLFACE: Back

        // ========================================================================
        // 计算着色器配置
        // ========================================================================

        std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeThreads; // COMPUTE_THREADS: 16,16,1
        std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeSize; // COMPUTE_SIZE: 1920,1080,1

        // ========================================================================
        // 自定义常量
        // ========================================================================

        std::unordered_map<std::string, std::string> customDefines; // const 指令解析结果

        // ========================================================================
        // 方法
        // ========================================================================

        ShaderDirectives() = default;

        /**
         * @brief 重置所有字段
         */
        void Reset();

        /**
         * @brief 从 HLSL 源码解析指令
         * @param source HLSL 源代码字符串
         * @return 解析后的 ShaderDirectives
         *
         * 教学要点:
         * - 解析 风格注释 RENDERTARGETS: ... 
         * - 解析 // const int ... 风格常量定义
         * - 兼容 Iris 注释语法
         *
         * 实现位置: ShaderDirectives.cpp
         */
        static ShaderDirectives Parse(const std::string& source);
    };
} // namespace enigma::graphic
