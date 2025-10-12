#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include "Engine/Graphic/Target/BufferFlipState.hpp"

namespace enigma::graphic
{
    class D12RenderTarget;

    // ============================================================================
    // RenderTargetSettings - 单个RenderTarget配置
    // ============================================================================

    /**
     * @brief 单个RenderTarget的创建配置
     *
     * 对应Iris的RENDERTARGETS注释解析结果:
     * - "RGBA8" → DXGI_FORMAT_R8G8B8A8_UNORM
     * - "RGBA16F" → DXGI_FORMAT_R16G16B16A16_FLOAT
     * - 尺寸相对屏幕的缩放因子
     */
    struct RenderTargetSettings
    {
        DXGI_FORMAT format            = DXGI_FORMAT_R8G8B8A8_UNORM; // RT格式
        float       widthScale        = 1.0f; // 宽度相对屏幕的缩放 (0.5 = 半分辨率)
        float       heightScale       = 1.0f; // 高度相对屏幕的缩放
        bool        enableMipmap      = false; // 是否启用Mipmap (Milestone 3.0)
        bool        allowLinearFilter = true; // 是否允许线性过滤
        int         sampleCount       = 1; // MSAA采样数 (1 = 无MSAA)
        const char* debugName         = "UnnamedRT"; // 调试名称
    };

    // ============================================================================
    // RenderTargetManager - Iris兼容的16 RenderTarget集中管理器
    // ============================================================================

    /**
     * @class RenderTargetManager
     * @brief 管理16个colortex RenderTarget实例 + BufferFlipState
     *
     * **Iris架构对应**:
     * - Iris: RenderTargets.java (16个colortex管理)
     * - DX12: RenderTargetManager (相同职责)
     *
     * **核心职责**:
     * 1. **RenderTarget生命周期管理** - 创建/销毁16个RT实例
     * 2. **BufferFlipState管理** - Main/Alt翻转状态追踪
     * 3. **Bindless索引查询** - 快速获取Main/Alt纹理索引
     * 4. **GPU常量缓冲上传** - RenderTargetsBuffer结构体生成
     * 5. **窗口尺寸变化响应** - 自动Resize所有RT
     *
     * **内存布局优化**:
     * - std::array<std::shared_ptr<D12RenderTarget>, 16> (128字节, 64位系统)
     * - BufferFlipState内部仅2字节 (std::bitset<16>)
     * - 总计约150字节 (极致节省)
     */
    class RenderTargetManager
    {
    public:
        /**
         * @brief 构造RenderTargetManager并创建16个RenderTarget
         * @param baseWidth 基准屏幕宽度
         * @param baseHeight 基准屏幕高度
         * @param rtSettings 16个RT的配置数组
         *
         * 教学要点:
         * - 使用Builder模式创建每个D12RenderTarget
         * - 自动注册Bindless索引 (调用RegisterBindless())
         * - 设置调试名称 (colorTexN)
         */
        RenderTargetManager(
            int                                         baseWidth,
            int                                         baseHeight,
            const std::array<RenderTargetSettings, 16>& rtSettings
        );

        /**
         * @brief 默认析构函数
         *
         * 教学要点: std::shared_ptr自动管理D12RenderTarget生命周期
         */
        ~RenderTargetManager() = default;

        // ========================================================================
        // RTV访问接口 - 用于OMSetRenderTargets()
        // ========================================================================

        /**
         * @brief 获取指定RT的主RTV句柄 (Main纹理的RTV)
         * @param rtIndex RenderTarget索引 [0-15]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int rtIndex) const;

        /**
         * @brief 获取指定RT的替代RTV句柄 (Alt纹理的RTV)
         * @param rtIndex RenderTarget索引 [0-15]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int rtIndex) const;

        // ========================================================================
        // Bindless索引访问 - 用于着色器ResourceDescriptorHeap访问
        // ========================================================================

        /**
         * @brief 获取指定RT的Main纹理Bindless索引
         * @param rtIndex RenderTarget索引 [0-15]
         * @return uint32_t Bindless索引
         *
         * 教学要点: 着色器通过此索引访问
         * ResourceDescriptorHeap[mainTextureIndex]
         */
        uint32_t GetMainTextureIndex(int rtIndex) const;

        /**
         * @brief 获取指定RT的Alt纹理Bindless索引
         * @param rtIndex RenderTarget索引 [0-15]
         * @return uint32_t Bindless索引
         */
        uint32_t GetAltTextureIndex(int rtIndex) const;

        // ========================================================================
        // BufferFlipState管理 - Main/Alt翻转逻辑
        // ========================================================================

        /**
         * @brief 翻转指定RT的Main/Alt状态
         * @param rtIndex RenderTarget索引 [0-15]
         *
         * 业务逻辑:
         * - 当前帧: 读Main写Alt → Flip() → 下一帧: 读Alt写Main
         * - 实现历史帧数据访问 (TAA, Motion Blur等技术需要)
         */
        void FlipRenderTarget(int rtIndex)
        {
            m_flipState.Flip(rtIndex);
        }

        /**
         * @brief 批量翻转所有RT (每帧结束时调用)
         */
        void FlipAllRenderTargets()
        {
            m_flipState.FlipAll();
        }

        /**
         * @brief 重置所有RT为初始状态 (读Main写Alt)
         */
        void ResetFlipState()
        {
            m_flipState.Reset();
        }

        /**
         * @brief 检查指定RT是否已翻转
         * @return false = 读Main写Alt, true = 读Alt写Main
         */
        bool IsFlipped(int rtIndex) const
        {
            return m_flipState.IsFlipped(rtIndex);
        }

        // ========================================================================
        // GPU常量缓冲上传 - RenderTargetsBuffer生成
        // ========================================================================

        /**
         * @brief 根据当前FlipState生成RenderTargetsBuffer并上传到GPU
         * @return uint32_t 已上传常量缓冲的Bindless索引
         *
         * RenderTargetsBuffer结构体:
         * struct RenderTargetsBuffer {
         *     uint readIndices[16];  // 读索引数组 (采样历史帧)
         *     uint writeIndices[16]; // 写索引数组 (渲染输出)
         * };
         *
         * 教学要点:
         * - 根据m_flipState确定每个RT的读/写索引
         * - 创建D12Buffer并上传数据
         * - 注册Bindless返回索引供着色器访问
         */
        uint32_t CreateRenderTargetsBuffer();

        // ========================================================================
        // 窗口尺寸变化响应 - 自动Resize所有RT
        // ========================================================================

        /**
         * @brief 响应窗口尺寸变化,Resize所有RT
         * @param newBaseWidth 新屏幕宽度
         * @param newBaseHeight 新屏幕高度
         *
         * 教学要点:
         * - 遍历16个RT,调用ResizeIfNeeded()
         * - 考虑widthScale/heightScale缩放因子
         * - 重新注册Bindless索引
         */
        void OnResize(int newBaseWidth, int newBaseHeight);

        // ========================================================================
        // 调试支持 - 查询RT状态
        // ========================================================================

        /**
         * @brief 获取指定RT的调试信息
         * @param rtIndex RenderTarget索引 [0-15]
         * @return std::string 格式化的调试信息
         */
        std::string GetDebugInfo(int rtIndex) const;

        /**
         * @brief 获取所有RT的概览信息
         * @return std::string 格式化的概览表格
         */
        std::string GetAllRenderTargetsInfo() const;

    private:
        // ========================================================================
        // 私有成员
        // ========================================================================

        std::array<std::shared_ptr<D12RenderTarget>, 16> m_renderTargets; // 16个RT实例
        RenderTargetFlipState                            m_flipState; // Main/Alt翻转状态 (BufferFlipState<16>)
        int                                              m_baseWidth; // 基准屏幕宽度
        int                                              m_baseHeight; // 基准屏幕高度
        std::array<RenderTargetSettings, 16>             m_settings; // RT配置缓存

        /**
         * @brief 验证rtIndex有效性 (内部辅助函数)
         * @return true if valid [0-15]
         */
        bool IsValidIndex(int rtIndex) const
        {
            return rtIndex >= 0 && rtIndex < 16;
        }

        // ========================================================================
        // 教学要点总结
        // ========================================================================

        /**
         * 1. **Iris架构映射**:
         *    - Iris: RenderTargets.java (16 colortex管理)
         *    - DX12: RenderTargetManager.hpp (完全对应)
         *
         * 2. **BufferFlipState极致优化**:
         *    - std::bitset<16> 仅2字节
         *    - O(1)翻转操作
         *    - 支持GPU直接上传 (ToUInt16())
         *
         * 3. **Bindless索引查询**:
         *    - GetMainTextureIndex() / GetAltTextureIndex()
         *    - 着色器通过索引直接访问纹理
         *    - 零Root Signature切换开销
         *
         * 4. **RenderTargetsBuffer上传**:
         *    - 根据FlipState生成读/写索引数组
         *    - 每帧更新GPU常量缓冲
         *    - 着色器通过索引数组访问正确的Main/Alt纹理
         *
         * 5. **智能指针内存管理**:
         *    - std::shared_ptr自动管理RT生命周期
         *    - 支持多处引用同一RT
         *    - 析构时自动释放GPU资源
         */
    };
} // namespace enigma::graphic
