#pragma once

#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include "Engine/Graphic/Target/BufferFlipState.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"

namespace enigma::graphic
{
    class D12RenderTarget;
    class D12Buffer;

    // ============================================================================
    // ShadowColorManager - Iris兼容的8个ShadowColor渲染目标管理器
    // ============================================================================

    /**
     * @class ShadowColorManager
     * @brief 管理8个ShadowColor渲染目标实例 + BufferFlipState
     *
     * **Iris架构对应**:
     * - Iris: ShadowRenderTargets管理 (shadowcolor0-7)
     * - DX12: ShadowColorManager (完全对应)
     *
     * **核心职责**:
     * 1. **ShadowColor生命周期管理** - 创建/销毁8个RT实例
     * 2. **BufferFlipState管理** - Main/Alt翻转状态追踪
     * 3. **Bindless索引查询** - 快速获取Main/Alt纹理索引
     * 4. **GPU常量缓冲上传** - ShadowColorBuffer结构体生成
     * 5. **窗口尺寸变化响应** - 自动Resize所有RT
     *
     * **技术特性**:
     * - 使用D12RenderTarget，支持Main/Alt双纹理Flipper
     * - GPU Buffer结构：shadowColorIndices[8] + shadowColorAltIndices[8]
     * - 与ShadowTex共享Bindless offset 10
     * - 支持用户自定义配置（分辨率、格式、LoadAction等）
     */
    class ShadowColorManager
    {
    public:
        // ========================================================================
        // 常量定义
        // ========================================================================

        static constexpr int MAX_SHADOW_COLORS = 8; ///< 最大ShadowColor数量（Iris兼容）

        /**
         * @brief 构造ShadowColorManager并创建ShadowColor渲染目标
         * @param rtConfigs RT配置数组（最多8个）
         * @param shadowColorCount 实际激活的ShadowColor数量（默认8个，范围[0,8]）
         *
         * 教学要点:
         * - 参考RenderTargetManager构造函数设计
         * - 使用Builder模式创建每个D12RenderTarget
         * - 自动注册Bindless索引
         * - 设置调试名称 (shadowcolorN_Main/Alt)
         */
        ShadowColorManager(
            const std::array<RTConfig, 8>& rtConfigs,
            int                            shadowColorCount = MAX_SHADOW_COLORS
        );

        /**
         * @brief 默认析构函数
         *
         * 教学要点: std::shared_ptr自动管理D12RenderTarget生命周期
         */
        ~ShadowColorManager() = default;

        // ========================================================================
        // ShadowColor访问接口
        // ========================================================================

        /**
         * @brief 获取指定索引的ShadowColor渲染目标
         * @param index ShadowColor索引 [0-7]
         * @param alt 是否获取Alt纹理（false=Main, true=Alt）
         * @return std::shared_ptr<D12RenderTarget>
         */
        std::shared_ptr<D12RenderTarget> GetShadowColor(int index, bool alt = false) const;

        // ========================================================================
        // RTV访问接口 - 用于OMSetRenderTargets()
        // ========================================================================

        /**
         * @brief 获取指定ShadowColor的主RTV句柄
         * @param index ShadowColor索引 [0-7]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) const;

        /**
         * @brief 获取指定ShadowColor的替代RTV句柄
         * @param index ShadowColor索引 [0-7]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) const;

        // ========================================================================
        // Bindless索引访问 - 用于着色器ResourceDescriptorHeap访问
        // ========================================================================

        /**
         * @brief 获取指定ShadowColor的Main纹理Bindless索引
         * @param index ShadowColor索引 [0-7]
         * @return uint32_t Bindless索引
         */
        uint32_t GetMainTextureIndex(int index) const;

        /**
         * @brief 获取指定ShadowColor的Alt纹理Bindless索引
         * @param index ShadowColor索引 [0-7]
         * @return uint32_t Bindless索引
         */
        uint32_t GetAltTextureIndex(int index) const;

        // ========================================================================
        // BufferFlipState管理 - Main/Alt翻转逻辑
        // ========================================================================

        /**
         * @brief 翻转指定ShadowColor的Main/Alt状态
         * @param index ShadowColor索引 [0-7]
         */
        void FlipShadowColor(int index)
        {
            m_flipState.Flip(index);
        }

        /**
         * @brief 批量翻转所有ShadowColor (每帧结束时调用)
         */
        void FlipAllShadowColors()
        {
            m_flipState.FlipAll();
        }

        /**
         * @brief 重置所有ShadowColor为初始状态 (读Main写Alt)
         */
        void ResetFlipState()
        {
            m_flipState.Reset();
        }

        /**
         * @brief 检查指定ShadowColor是否已翻转
         * @return false = 读Main写Alt, true = 读Alt写Main
         */
        bool IsFlipped(int index) const
        {
            return m_flipState.IsFlipped(index);
        }

        // ========================================================================
        // GPU常量缓冲上传 - ShadowColorBuffer生成
        // ========================================================================

        /**
         * @brief 根据当前FlipState生成ShadowColorBuffer并上传到GPU
         * @return uint32_t 已上传常量缓冲的Bindless索引
         *
         * ShadowColorBuffer结构体:
         * struct ShadowColorBuffer {
         *     uint readIndices[8];  // 读索引数组
         *     uint writeIndices[8]; // 写索引数组
         * };
         *
         * 教学要点:
         * - 根据m_flipState确定每个ShadowColor的读/写索引
         * - 创建D12Buffer并上传数据
         * - 注册Bindless返回索引供着色器访问
         */
        uint32_t CreateShadowColorBuffer();

        // ========================================================================
        // 窗口尺寸变化响应 - 自动Resize所有RT
        // ========================================================================

        /**
         * @brief 响应窗口尺寸变化,Resize所有ShadowColor
         * @param newBaseWidth 新屏幕宽度
         * @param newBaseHeight 新屏幕高度
         */
        void OnResize(int newBaseWidth, int newBaseHeight);

        // ========================================================================
        // 调试支持
        // ========================================================================

        /**
         * @brief 获取当前激活的ShadowColor数量
         * @return int 激活数量 [0-8]
         */
        int GetActiveShadowColorCount() const { return m_activeShadowColorCount; }

        /**
         * @brief 获取指定ShadowColor的调试信息
         * @param index ShadowColor索引 [0-7]
         * @return std::string 格式化的调试信息
         */
        std::string GetDebugInfo(int index) const;

    private:
        // ========================================================================
        // 私有成员
        // ========================================================================

        // ShadowColor数组 - 每个ShadowColor有Main和Alt两个纹理
        // 索引规则: Main = i*2, Alt = i*2+1
        std::vector<std::shared_ptr<D12RenderTarget>> m_shadowColors;

        // 当前激活的ShadowColor数量，范围 [0, 8]
        int m_activeShadowColorCount = MAX_SHADOW_COLORS;

        BufferFlipState<8>         m_flipState; // Main/Alt翻转状态
        int                        m_baseWidth; // 基准屏幕宽度
        int                        m_baseHeight; // 基准屏幕高度
        std::array<RTConfig, 8>    m_configs; // RT配置缓存
        std::unique_ptr<D12Buffer> m_gpuBuffer; // GPU常量缓冲

        /**
         * @brief 验证索引有效性
         * @return true if valid [0, m_activeShadowColorCount)
         */
        bool IsValidIndex(int index) const
        {
            return index >= 0 && index < m_activeShadowColorCount;
        }

        /**
         * @brief 创建所有ShadowColor渲染目标
         */
        void CreateShadowColors();
    };
} // namespace enigma::graphic
