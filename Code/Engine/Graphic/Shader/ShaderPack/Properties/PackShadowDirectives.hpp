/**
 * @file PackShadowDirectives.hpp
 * @brief Iris阴影配置指令 - 占位符实现
 * @date 2025-10-11
 *
 * 对应 Iris PackShadowDirectives.java
 * 职责:
 * 1. 存储从shaders.properties解析出的阴影配置
 * 2. 提供shadowcolor格式、过滤模式、Mipmap等配置查询
 *
 * HLSL注释示例:
 * ```hlsl
 * const int shadowMapResolution = 2048;
 * const bool shadowHardwareFiltering = true;
 * const bool shadowcolor0Mipmap = true;
 * const bool shadowcolor0Clear = false;
 * ```
 *
 * ⚠️ 占位符警告:
 * - 这是临时占位符，提供ShadowRenderTargetManager所需的最小接口
 * - 完整实现需要解析shaders.properties中的shadow配置
 * - 后续需要实现: Parse()方法、更多shadow配置字段
 * - 详见: F:\p4\Personal\SD\Thesis\.claude\plan\shadow-rendertarget-manager-implementation-plan.md
 */

#pragma once

#include <d3d12.h>
#include <array>
#include <string>

namespace enigma::graphic
{
    /**
     * @class PackShadowDirectives
     * @brief Iris阴影配置指令容器 - 占位符实现
     *
     * 对应Iris源码:
     * - PackShadowDirectives.java (存储阴影配置)
     * - SamplingSettings.java (shadowcolor采样配置)
     *
     * 教学要点:
     * - 对应Iris的PackShadowDirectives.java
     * - 存储从shaders.properties解析的阴影配置
     * - 影响shadowcolor格式、过滤模式、分辨率等
     *
     * ⚠️ 占位符状态:
     * - 当前提供默认值，未实现解析逻辑
     * - shadowcolor0-7统一配置（简化实现）
     * - 后续需要支持per-buffer配置
     */
    class PackShadowDirectives
    {
    public:
        // ========================================================================
        // 阴影贴图基础配置
        // ========================================================================

        /// 阴影贴图分辨率 (1024/2048/4096)
        /// 对应Iris: shadowMapResolution
        int shadowMapResolution = 2048;

        /// 是否启用阴影
        /// 对应Iris: shadowEnabled
        bool shadowEnabled = true;

        // ========================================================================
        // shadowcolor格式配置 - 占位符实现（统一配置）
        // ========================================================================

        /// shadowcolor统一格式（占位符：所有shadowcolor使用同一格式）
        /// 对应Iris: shadowcolor0Format, shadowcolor1Format, ...
        /// 默认: RGBA8 (DXGI_FORMAT_R8G8B8A8_UNORM)
        DXGI_FORMAT shadowColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        /// shadowcolor硬件过滤（占位符：所有shadowcolor使用同一配置）
        /// 对应Iris: shadowHardwareFiltering0, shadowHardwareFiltering1, ...
        /// 默认: false (手动PCF)
        bool shadowHardwareFiltering = false;

        /// shadowcolor Mipmap（占位符：所有shadowcolor使用同一配置）
        /// 对应Iris: shadowcolor0Mipmap, shadowcolor1Mipmap, ...
        /// 默认: false (无Mipmap)
        bool shadowColorMipmap = false;

        /// shadowcolor每帧清除（占位符：所有shadowcolor使用同一配置）
        /// 对应Iris: shadowcolor0Clear, shadowcolor1Clear, ...
        /// 默认: false (保留历史帧)
        bool shadowColorClear = false;

        // ========================================================================
        // shadowtex深度格式配置
        // ========================================================================

        /// shadowtex0/1深度格式
        /// 对应Iris: shadowDepthBuffers
        /// 默认: D32_FLOAT (高精度深度，支持采样)
        DXGI_FORMAT shadowDepthFormat = DXGI_FORMAT_D32_FLOAT;

        /// shadowtex硬件过滤（深度比较采样）
        /// 对应Iris: shadowtexHardwareFiltering
        /// 默认: true (GPU硬件PCF)
        bool shadowDepthHardwareFiltering = true;

        /// shadowtex Mipmap
        /// 对应Iris: shadowtexMipmap
        /// 默认: false (深度通常不需要Mipmap)
        bool shadowDepthMipmap = false;

        // ========================================================================
        // 阴影渲染配置
        // ========================================================================

        /// 阴影裁剪距离
        /// 对应Iris: shadowDistance
        /// 默认: 120.0 (方块单位)
        float shadowDistance = 120.0f;

        /// 是否使用深度裁剪面
        /// 对应Iris: shadowClipFrustum
        /// 默认: true
        bool shadowClipFrustum = true;

        /// 阴影采样数（用于软阴影）
        /// 对应Iris: shadowSamples
        /// 默认: 1 (硬阴影)
        int shadowSamples = 1;

        // ========================================================================
        // 方法
        // ========================================================================

        PackShadowDirectives() = default;

        /**
         * @brief 重置所有字段为默认值
         */
        void Reset();


        /**
         * @brief 获取指定shadowcolor的格式（占位符：统一返回shadowColorFormat）
         * @param index shadowcolor索引 [0-7]
         * @return DXGI_FORMAT
         *
         * ⚠️ 占位符实现: 所有shadowcolor使用统一格式
         * 完整实现需要: std::array<DXGI_FORMAT, 8> shadowColorFormats
         */
        DXGI_FORMAT GetShadowColorFormat(int index) const;

        /**
         * @brief 检查指定shadowcolor是否启用硬件过滤（占位符：统一返回shadowHardwareFiltering）
         * @param index shadowcolor索引 [0-7]
         * @return bool
         *
         * ⚠️ 占位符实现: 所有shadowcolor使用统一配置
         * 完整实现需要: std::array<bool, 8> shadowHardwareFilteringFlags
         */
        bool IsShadowColorHardwareFiltered(int index) const;

        /**
         * @brief 检查指定shadowcolor是否启用Mipmap（占位符：统一返回shadowColorMipmap）
         * @param index shadowcolor索引 [0-7]
         * @return bool
         *
         * ⚠️ 占位符实现: 所有shadowcolor使用统一配置
         * 完整实现需要: std::array<bool, 8> shadowColorMipmapFlags
         */
        bool IsShadowColorMipmapEnabled(int index) const;

        /**
         * @brief 检查指定shadowcolor是否每帧清除（占位符：统一返回shadowColorClear）
         * @param index shadowcolor索引 [0-7]
         * @return bool
         *
         * ⚠️ 占位符实现: 所有shadowcolor使用统一配置
         * 完整实现需要: std::array<bool, 8> shadowColorClearFlags
         */
        bool ShouldShadowColorClearEveryFrame(int index) const;

        /**
         * @brief 从shaders.properties解析阴影配置（⚠️ 未实现 - 占位符）
         * @param propertiesContent shaders.properties文件内容
         * @return 解析后的PackShadowDirectives
         *
         * ⚠️ 占位符实现: 返回默认配置
         *
         * 完整实现需要解析:
         * - shadowMapResolution=2048
         * - shadowHardwareFiltering=true
         * - shadowcolor0Format=RGBA8
         * - shadowcolor0Mipmap=false
         * - shadowcolor0Clear=false
         * - etc.
         *
         * 实现位置: PackShadowDirectives.cpp（待创建）
         */
        static PackShadowDirectives Parse(const std::string& propertiesContent);

        /**
         * @brief 获取调试信息字符串
         * @return 格式化的配置信息
         */
        std::string GetDebugInfo() const;
    };

    // ========================================================================
    // 教学要点总结
    // ========================================================================
    /**
     * 1. **Iris架构对应**:
     *    - Iris: PackShadowDirectives.java (完整解析器)
     *    - DX12: PackShadowDirectives.hpp (占位符实现)
     *
     * 2. **占位符设计原则**:
     *    - 提供ShadowRenderTargetManager所需的最小接口
     *    - 使用统一配置简化实现（所有shadowcolor相同配置）
     *    - 返回默认值保证系统可运行
     *
     * 3. **待实现功能**:
     *    - Parse()方法: 解析shaders.properties中的shadow配置
     *    - Per-buffer配置: 每个shadowcolor独立格式/过滤模式
     *    - 高级功能: shadow entity、shadow cutout、shadow composite配置
     *
     * 4. **实施优先级**:
     *    - P0 (当前): 占位符实现，保证编译通过
     *    - P1 (Phase 7): 完整Parse()实现，支持per-buffer配置
     *    - P2 (Milestone 3.1): 高级shadow特性支持
     */
} // namespace enigma::graphic
