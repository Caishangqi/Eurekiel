/**
 * @file ShadowTargetManager.hpp
 * @brief ShadowTex管理器 - 管理2个ShadowTex纹理（索引0-1）
 *
 * 设计要点:
 * 1. 使用D12Texture而不是D12RenderTarget（只读纹理）
 * 2. 无Flipper机制，每个索引只有一个纹理
 * 3. 支持用户自定义配置（参考RenderTargetManager模式）
 * 4. GPU Buffer结构：shadowTexIndices[2]
 *
 * 架构对应:
 * - Iris: ShadowTexture管理（只读阴影贴图）
 * - DX12: ShadowTargetManager（D12Texture + Bindless集成）
 *
 * 职责边界:
 * - ShadowTargetManager: 管理ShadowTex纹理（只读）
 * - ShadowColorManager: 管理ShadowColor渲染��标（Main/Alt Flipper）
 */

#pragma once

#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include "RTTypes.hpp"

namespace enigma::graphic
{
    // 前向声明
    class D12Texture;

    /**
     * @class ShadowTargetManager
     * @brief 管理2个ShadowTex纹理实例
     *
     * 核心职责:
     * 1. ShadowTex生命周期管理 - 创建/销毁2个纹理实例（索引0-1）
     * 2. Bindless索引查询 - 快速获取纹理索引
     * 3. 窗口尺寸变化响应 - 自动Resize所有纹理
     *
     * 关键特性:
     * - 使用D12Texture（ShaderResource用途）
     * - 无Flipper机制（只读纹理）
     * - 用户可配置（RTConfig数组参数）
     *
     * 索引管理:
     * - shadowtex0/1 索引由 UniformManager 的 ShadowBufferIndex 统一管理
     * - 本类只负责纹理资源管理，通过 GetShadowTexIndex() 提供索引查询
     */
    class ShadowTargetManager
    {
    public:
        // ========================================================================
        // 常量定义 - ShadowTex数量限制
        // ========================================================================

        static constexpr int MAX_SHADOW_TEXTURES = 2; // 最大ShadowTex数量（Iris兼容）

        /**
         * @brief 构造ShadowTargetManager并创建ShadowTex纹理
         * @param rtConfigs ShadowTex配置数组（2个）
         * @param shadowTexCount 实际激活的ShadowTex数量（默认2个，范围[1,2]）
         *
         * 教学要点:
         * - 使用TextureCreateInfo创建D12Texture
         * - 自动注册Bindless索引
         * - 设置调试名称（shadowTex0, shadowTex1）
         */
        ShadowTargetManager(
            const std::array<RTConfig, 2>& rtConfigs,
            int                            shadowTexCount = MAX_SHADOW_TEXTURES
        );

        ~ShadowTargetManager() = default;

        // 禁用拷贝和移动（Manager类通常作为单例使用）
        ShadowTargetManager(const ShadowTargetManager&)            = delete;
        ShadowTargetManager& operator=(const ShadowTargetManager&) = delete;
        ShadowTargetManager(ShadowTargetManager&&)                 = delete;
        ShadowTargetManager& operator=(ShadowTargetManager&&)      = delete;

        // ========================================================================
        // ShadowTex纹理访问接口
        // ========================================================================

        /**
         * @brief 获取指定索引的ShadowTex纹理
         * @param index ShadowTex索引（0-1）
         * @return 纹理指针，无效索引返回nullptr
         *
         * 教学要点:
         * - 无Flipper机制，直接返回对应索引的纹理
         * - 与ShadowColorManager::GetShadowColor()对比：无alt参数
         */
        std::shared_ptr<D12Texture> GetShadowTarget(int index) const;

        /**
         * @brief 获取指定索引的ShadowTex Bindless索引
         * @param index ShadowTex索引（0-1）
         * @return Bindless索引，无效索引返回0
         *
         * 教学要点:
         * - 直接从纹理获取Bindless索引，避免GPU Buffer查询
         */
        uint32_t GetShadowTexIndex(int index) const;

        /**
         * @brief 获取激活的ShadowTex数量
         * @return 激活数量（1-2）
         */
        int GetActiveShadowTexCount() const { return m_activeShadowTexCount; }

        // ========================================================================
        // 生命周期管理接口
        // ========================================================================

        /**
         * @brief 窗口尺寸变化时重建ShadowTex纹理
         * @param baseWidth 新的基准宽度
         * @param baseHeight 新的基准高度
         *
         * 教学要点:
         * - 销毁所有现有纹理（智能指针自动清理）
         * - 使用新尺寸重建纹理
         * - 索引更新由 UniformManager 负责
         */
        void OnResize(int baseWidth, int baseHeight);

    private:
        // ========================================================================
        // 内部实现方法
        // ========================================================================

        /**
         * @brief 创建所有ShadowTex纹理
         *
         * 教学要点:
         * - 使用TextureCreateInfo配置
         * - Usage设置为ShaderResource（只读纹理）
         * - 自动注册到Bindless系统
         */
        void CreateShadowTargets();

        // ========================================================================
        // 内部数据成员
        // ========================================================================

        /// ShadowTex纹理数组（2个纹理，无Flipper）
        std::vector<std::shared_ptr<D12Texture>> m_shadowTargets;

        /// ShadowTex配置数组（用户提供）
        std::array<RTConfig, 2> m_configs;

        /// 激活的ShadowTex数量（1-2）
        int m_activeShadowTexCount;

        /// 当前基准宽度（用于OnResize）
        int m_baseWidth;

        /// 当前基准高度（用于OnResize）
        int m_baseHeight;
    };
} // namespace enigma::graphic
