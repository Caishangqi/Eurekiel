#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <d3d12.h>
#include "Engine/Graphic/Target/BufferFlipState.hpp"

namespace enigma::graphic
{
    class D12RenderTarget;
    class D12DepthTexture;
    class PackShadowDirectives;

    // ============================================================================
    // ShadowRenderTargetSettings - 单个Shadow RenderTarget配置
    // ============================================================================

    /**
     * @brief 单个Shadow RenderTarget的创建配置
     *
     * 对应Iris的PackShadowDirectives.SamplingSettings:
     * - format: 纹理格式（默认RGBA8）
     * - hardwareFiltered: 是否启用PCF硬件过滤
     * - enableMipmap: 是否启用Mipmap
     * - linearFilter: 是否使用线性过滤
     * - clearEveryFrame: 是否每帧清除
     */
    struct ShadowRenderTargetSettings
    {
        DXGI_FORMAT format           = DXGI_FORMAT_R8G8B8A8_UNORM; // Shadow RT格式
        bool        hardwareFiltered = false; // PCF硬件过滤（Iris中的HW filtering）
        bool        enableMipmap     = false; // Mipmap支持
        bool        linearFilter     = true; // 线性过滤
        bool        clearEveryFrame  = false; // 是否每帧清除
        const char* debugName        = "UnnamedShadowRT"; // 调试名称
    };

    // ============================================================================
    // ShadowRenderTargetManager - Iris兼容的8 Shadow RenderTarget集中管理器
    // ============================================================================

    /**
     * @class ShadowRenderTargetManager
     * @brief 管理8个shadowcolor RenderTarget实例 + 2个shadowtex深度纹理 + BufferFlipState
     *
     * **Iris架构对应**:
     * - Iris: ShadowRenderTargets.java (8个shadowcolor + 2个shadowtex管理)
     * - DX12: ShadowRenderTargetManager (完全对应)
     *
     * **核心职责**:
     * 1. **RenderTarget生命周期管理** - 懒加载创建/销毁8个shadowcolor实例
     * 2. **深度纹理管理** - 管理shadowtex0(主深度) + shadowtex1(半透明前深度)
     * 3. **BufferFlipState管理** - Main/Alt翻转状态追踪（8个shadowcolor）
     * 4. **Bindless索引查询** - 快速获取Main/Alt纹理索引
     * 5. **GPU常量缓冲上传** - ShadowBufferIndex结构体生成
     * 6. **固定分辨率** - 阴影贴图使用固定分辨率（1024/2048/4096），不随窗口变化
     *
     * **与RenderTargetManager的关键差异**:
     * - RenderTargetManager: 16个colortex + 2个depthtex + 屏幕相关分辨率 + OnResize支持
     * - ShadowRenderTargetManager: 8个shadowcolor + 2个shadowtex + 固定分辨率 + 无OnResize
     *
     * **懒加载设计** (对应Iris getOrCreate模式):
     * - shadowcolor0-7在首次GetOrCreate()时才创建
     * - shadowtex0/1在构造函数中立即创建
     * - 避免创建未使用的shadowcolor纹理
     *
     * **内存布局优化**:
     * - std::array<std::shared_ptr<D12RenderTarget>, 8> (64字节, 64位系统)
     * - 2个std::shared_ptr<D12DepthTexture> (16字节)
     * - ShadowFlipState内部仅1字节 (std::bitset<8>)
     * - 总计约100字节 (极致节省)
     *
     * **教学要点**:
     * 1. **懒加载模式**: 只创建实际使用的shadowcolor，节省GPU内存
     * 2. **固定分辨率**: 阴影贴图不受窗口尺寸影响，独立配置
     * 3. **Iris完全兼容**: 严格遵循ShadowRenderTargets.java的设计
     * 4. **BufferFlipState<8>验证**: 证明模板设计的通用性
     */
    class ShadowRenderTargetManager
    {
    public:
        /**
         * @brief 构造ShadowRenderTargetManager并创建深度纹理
         * @param resolution 阴影贴图固定分辨率 (1024/2048/4096)
         * @param shadowDirectives Shader Pack的阴影配置
         *
         * 教学要点:
         * - shadowtex0/1在构造函数中立即创建
         * - shadowcolor0-7使用懒加载，不在此创建
         * - 固定分辨率，不受窗口尺寸影响
         *
         * 对应Iris:
         * ShadowRenderTargets(WorldRenderingPipeline pipeline, int resolution, PackShadowDirectives shadowDirectives)
         */
        ShadowRenderTargetManager(
            int                         resolution,
            const PackShadowDirectives& shadowDirectives
        );

        /**
         * @brief 默认析构函数
         *
         * 教学要点: std::shared_ptr自动管理D12RenderTarget/D12DepthTexture生命周期
         */
        ~ShadowRenderTargetManager() = default;

        // ========================================================================
        // RenderTarget访问接口 - 懒加载模式
        // ========================================================================

        /**
         * @brief 获取指定shadowcolor RT（如果不存在返回nullptr）
         * @param index shadowcolor索引 [0-7]
         * @return std::shared_ptr<D12RenderTarget> 可能为nullptr
         *
         * 对应Iris:
         * public RenderTarget get(int index)
         */
        std::shared_ptr<D12RenderTarget> Get(int index) const;

        /**
         * @brief 获取或创建指定shadowcolor RT（懒加载核心方法）
         * @param index shadowcolor索引 [0-7]
         * @return std::shared_ptr<D12RenderTarget> 保证非空
         *
         * 教学要点:
         * - 懒加载核心方法，首次调用时创建纹理
         * - 自动注册Bindless索引
         * - 设置调试名称（shadowcolorN）
         *
         * 对应Iris:
         * public RenderTarget getOrCreate(int index)
         */
        std::shared_ptr<D12RenderTarget> GetOrCreate(int index);

        /**
         * @brief 如果RT不存在则创建（辅助方法）
         * @param index shadowcolor索引 [0-7]
         *
         * 对应Iris:
         * public void createIfEmpty(int index)
         */
        void CreateIfEmpty(int index);

        // ========================================================================
        // 深度纹理访问接口
        // ========================================================================

        /**
         * @brief 获取shadowtex0（主深度缓冲）
         * @return std::shared_ptr<D12DepthTexture>
         *
         * 对应Iris:
         * public DepthTexture getDepthTexture()
         */
        std::shared_ptr<D12DepthTexture> GetShadowTex0() const
        {
            return m_shadowTex0;
        }

        /**
         * @brief 获取shadowtex1（半透明前深度）
         * @return std::shared_ptr<D12DepthTexture>
         *
         * 对应Iris:
         * public DepthTexture getDepthTextureNoTranslucents()
         */
        std::shared_ptr<D12DepthTexture> GetShadowTex1() const
        {
            return m_shadowTex1;
        }

        // ========================================================================
        // RTV访问接口 - 用于OMSetRenderTargets()
        // ========================================================================

        /**
         * @brief 获取指定shadowcolor的主RTV句柄 (Main纹理的RTV)
         * @param index shadowcolor索引 [0-7]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         *
         * 教学要点: 返回CPU句柄用于SetRenderTargets()绑定
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetMainRTV(int index) const;

        /**
         * @brief 获取指定shadowcolor的替代RTV句柄 (Alt纹理的RTV)
         * @param index shadowcolor索引 [0-7]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetAltRTV(int index) const;

        /**
         * @brief 获取shadowtex0的DSV句柄（深度模板视图）
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetShadowTex0DSV() const;

        /**
         * @brief 获取shadowtex1的DSV句柄（深度模板视图）
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetShadowTex1DSV() const;

        // ========================================================================
        // Bindless索引访问 - 用于着色器ResourceDescriptorHeap访问
        // ========================================================================

        /**
         * @brief 获取指定shadowcolor的Main纹理Bindless索引
         * @param index shadowcolor索引 [0-7]
         * @return uint32_t Bindless索引
         *
         * 教学要点: 着色器通过此索引访问
         * ResourceDescriptorHeap[mainTextureIndex]
         *
         * 对应Iris:
         * public int getColorTextureId(int i) // 返回纹理ID，我们返回Bindless索引
         */
        uint32_t GetMainTextureIndex(int index) const;

        /**
         * @brief 获取指定shadowcolor的Alt纹理Bindless索引
         * @param index shadowcolor索引 [0-7]
         * @return uint32_t Bindless索引
         */
        uint32_t GetAltTextureIndex(int index) const;

        /**
         * @brief 获取shadowtex0的Bindless索引
         * @return uint32_t Bindless索引
         */
        uint32_t GetShadowTex0Index() const;

        /**
         * @brief 获取shadowtex1的Bindless索引
         * @return uint32_t Bindless索引
         */
        uint32_t GetShadowTex1Index() const;

        // ========================================================================
        // BufferFlipState管理 - Main/Alt翻转逻辑
        // ========================================================================

        /**
         * @brief 翻转指定shadowcolor的Main/Alt状态
         * @param index shadowcolor索引 [0-7]
         *
         * 业务逻辑:
         * - 当前帧: 读Main写Alt → Flip() → 下一帧: 读Alt写Main
         * - 实现历史帧数据访问 (用于shadow composite)
         *
         * 对应Iris:
         * public void flip(int target)
         */
        void FlipShadowColor(int index)
        {
            m_flipState.Flip(index);
        }

        /**
         * @brief 批量翻转所有shadowcolor (每帧结束时调用)
         */
        void FlipAllShadowColors()
        {
            m_flipState.FlipAll();
        }

        /**
         * @brief 重置所有shadowcolor为初始状态 (读Main写Alt)
         */
        void ResetFlipState()
        {
            m_flipState.Reset();
        }

        /**
         * @brief 检查指定shadowcolor是否已翻转
         * @return false = 读Main写Alt, true = 读Alt写Main
         *
         * 对应Iris:
         * public boolean isFlipped(int target)
         */
        bool IsFlipped(int index) const
        {
            return m_flipState.IsFlipped(index);
        }

        // ========================================================================
        // GPU常量缓冲上传 - ShadowBufferIndex生成
        // ========================================================================

        /**
         * @brief 根据当前FlipState生成ShadowBufferIndex并返回Bindless索引
         * @return uint32_t 已上传常量缓冲的Bindless索引
         *
         * ShadowBufferIndex结构体:
         * struct ShadowBufferIndex {
         *     uint shadowColorReadIndices[8];  // 读索引数组 (采样历史帧)
         *     uint shadowColorWriteIndices[8]; // 写索引数组 (渲染输出)
         *     uint shadowTex0Index;            // shadowtex0索引
         *     uint shadowTex1Index;            // shadowtex1索引
         * };
         *
         * 教学要点:
         * - 根据m_flipState确定每个shadowcolor的读/写索引
         * - 创建D12Buffer并上传数据
         * - 注册Bindless返回索引供着色器访问
         */
        uint32_t CreateShadowBufferIndex();

        // ========================================================================
        // 查询接口
        // ========================================================================

        /**
         * @brief 获取阴影贴图固定分辨率
         * @return int 分辨率 (1024/2048/4096)
         *
         * 对应Iris:
         * public int getResolution()
         */
        int GetResolution() const
        {
            return m_resolution;
        }

        /**
         * @brief 获取shadowcolor数量（固定为8）
         * @return int 数量
         */
        int GetShadowColorCount() const
        {
            return 8;
        }

        /**
         * @brief 检查指定shadowcolor是否启用硬件过滤
         * @param index shadowcolor索引 [0-7]
         * @return bool
         *
         * 对应Iris:
         * public boolean isHardwareFiltered(int i)
         */
        bool IsHardwareFiltered(int index) const;

        // ========================================================================
        // 调试支持 - 查询shadowcolor状态
        // ========================================================================

        /**
         * @brief 获取指定shadowcolor的调试信息
         * @param index shadowcolor索引 [0-7]
         * @return std::string 格式化的调试信息
         */
        std::string GetDebugInfo(int index) const;

        /**
         * @brief 获取所有shadowcolor的概览信息
         * @return std::string 格式化的概览表格
         */
        std::string GetAllShadowTargetsInfo() const;

    private:
        // ========================================================================
        // 私有成员
        // ========================================================================

        std::array<std::shared_ptr<D12RenderTarget>, 8> m_shadowColorTargets; // 8个shadowcolor实例（懒加载）
        std::shared_ptr<D12DepthTexture>                m_shadowTex0; // shadowtex0 - 主深度
        std::shared_ptr<D12DepthTexture>                m_shadowTex1; // shadowtex1 - 半透明前深度
        ShadowFlipState                                 m_flipState; // Main/Alt翻转状态 (BufferFlipState<8>)
        int                                             m_resolution; // 固定分辨率 (1024/2048/4096)
        const PackShadowDirectives&                     m_shadowDirectives; // Shader Pack阴影配置（引用）
        std::array<ShadowRenderTargetSettings, 8>       m_settings; // shadowcolor配置缓存

        /**
         * @brief 创建指定shadowcolor RT（内部辅助函数）
         * @param index shadowcolor索引 [0-7]
         *
         * 教学要点:
         * - 从m_shadowDirectives读取配置
         * - 使用Builder模式创建D12RenderTarget
         * - 自动注册Bindless索引
         * - 设置调试名称（shadowcolorN）
         */
        void CreateShadowColorRT(int index);

        /**
         * @brief 验证shadowcolor索引有效性 (内部辅助函数)
         * @return true if valid [0-7]
         */
        bool IsValidIndex(int index) const
        {
            return index >= 0 && index < 8;
        }

        // ========================================================================
        // 教学要点总结
        // ========================================================================

        /**
         * 1. **Iris架构映射**:
         *    - Iris: ShadowRenderTargets.java (8 shadowcolor + 2 shadowtex管理)
         *    - DX12: ShadowRenderTargetManager.hpp (完全对应)
         *
         * 2. **懒加载模式**:
         *    - GetOrCreate()首次调用时创建纹理
         *    - 节省GPU内存（只创建实际使用的shadowcolor）
         *    - shadowtex0/1立即创建（必需）
         *
         * 3. **BufferFlipState<8>极致优化**:
         *    - std::bitset<8> 仅1字节
         *    - O(1)翻转操作
         *    - 支持GPU直接上传 (ToUInt8())
         *
         * 4. **固定分辨率设计**:
         *    - 阴影贴图不受窗口尺寸影响
         *    - 无OnResize()方法
         *    - 独立配置（1024/2048/4096）
         *
         * 5. **Bindless索引查询**:
         *    - GetMainTextureIndex() / GetAltTextureIndex()
         *    - GetShadowTex0Index() / GetShadowTex1Index()
         *    - 着色器通过索引直接访问纹理
         *    - 零Root Signature切换开销
         *
         * 6. **ShadowBufferIndex上传**:
         *    - 根据FlipState生成读/写索引数组
         *    - 包含shadowtex0/1索引
         *    - 每帧更新GPU常量缓冲
         *    - 着色器通过索引数组访问正确的Main/Alt纹理
         *
         * 7. **智能指针内存管理**:
         *    - std::shared_ptr自动管理RT生命周期
         *    - 支持多处引用同一RT
         *    - 析构时自动释放GPU资源
         *
         * 8. **与RenderTargetManager的差异**:
         *    - RenderTargetManager: 16个colortex + 屏幕相关
         *    - ShadowRenderTargetManager: 8个shadowcolor + 固定分辨率
         *    - 两者共享BufferFlipState<N>模板设计
         */
    };
} // namespace enigma::graphic
