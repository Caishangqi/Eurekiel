#pragma once

#include <array>
#include <vector> // Milestone 3.0: 支持动态colortex数量
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include "Engine/Graphic/Target/BufferFlipState.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp" // 配置统一化重构: 使用RTConfig

namespace enigma::graphic
{
    class D12RenderTarget;

    // ============================================================================
    // RenderTargetManager - Iris兼容的16 RenderTarget集中管理器
    // ============================================================================

    /**
     * @class RenderTargetManager
     * @brief 管理动态数量（1-16个）colortex RenderTarget实例 + BufferFlipState
     *
     * **Iris架构对应**:
     * - Iris: RenderTargets.java (16个colortex管理)
     * - DX12: RenderTargetManager (动态数量支持，内存优化)
     *
     * **核心职责**:
     * 1. **RenderTarget生命周期管理** - 创建/销毁动态数量的RT实例（1-16个）
     * 2. **BufferFlipState管理** - Main/Alt翻转状态追踪
     * 3. **Bindless索引查询** - 快速获取Main/Alt纹理索引
     * 4. **GPU常量缓冲上传** - RenderTargetsBuffer结构体生成
     * 5. **窗口尺寸变化响应** - 自动Resize所有RT
     *
     * **内存优化** (Milestone 3.0):
     * - 动态colortex数量：支持1-16个（默认16个）
     * - 内存节省示例（1920x1080, R8G8B8A8）:
     *   - 16个colortex: ~132.6MB
     *   - 4个colortex:  ~33.2MB（节省75%，约99.4MB）
     * - std::vector动态管理，仅创建激活数量的RT
     *
     * **内存布局**:
     * - std::vector<std::shared_ptr<D12RenderTarget>> (动态大小)
     * - BufferFlipState内部仅2字节 (std::bitset<16>)
     */
    class RenderTargetManager
    {
    public:
        // ========================================================================
        // 常量定义 - colortex数量限制
        // ========================================================================

        static constexpr int MAX_COLOR_TEXTURES = 16; // 最大colortex数量（Iris兼容）
        static constexpr int MIN_COLOR_TEXTURES = 1; // 最小colortex数量

        /**
         * @brief 构造RenderTargetManager并创建动态数量的RenderTarget
         * @param baseWidth 基准屏幕宽度
         * @param baseHeight 基准屏幕高度
         * @param rtConfigs RT配置数组（最多16个）- 配置统一化重构: 使用RTConfig
         * @param colorTexCount 实际激活的colortex数量（默认16个，范围[1,16]）
         *
         * **Milestone 3.0 新增参数**:
         * - colorTexCount: 允许动态配置colortex数量，优化内存使用
         *   - 例如：colorTexCount=4 时，只创建4个colortex（节省75%内存）
         *   - 超出范围会自动修正并记录警告
         *
         * 教学要点:
         * - 使用Builder模式创建每个D12RenderTarget
         * - 自动注册Bindless索引 (调用RegisterBindless())
         * - 设置调试名称 (colorTexN)
         * - 仅创建激活数量的RT实例（内存优化）
         */
        RenderTargetManager(
            int                             baseWidth,
            int                             baseHeight,
            const std::array<RTConfig, 16>& rtConfigs,
            int                             colorTexCount = MAX_COLOR_TEXTURES
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

        /**
         * @brief 获取指定RT的格式
         * @param rtIndex RenderTarget索引 [0-15]
         * @return DXGI_FORMAT RT格式
         *
         * 教学要点:
         * - 用于PSO创建时查询RT格式
         * - 如果RT未创建，返回默认格式DXGI_FORMAT_R8G8B8A8_UNORM
         */
        DXGI_FORMAT GetRenderTargetFormat(int rtIndex) const;

        // ========================================================================
        // BufferFlipState管理 - Main/Alt翻转逻辑
        // ========================================================================

        /**
         * @brief 翻转指定RT的Main/Alt状态
         * @param rtIndex RenderTarget索引 [0, m_activeColorTexCount)
         *
         * 业务逻辑:
         * - 当前帧: 读Main写Alt → Flip() → 下一帧: 读Alt写Main
         * - 实现历史帧数据访问 (TAA, Motion Blur等技术需要)
         *
         * M6.2.2实现:
         * - 边界检查：rtIndex必须在[0, m_activeColorTexCount)范围内
         * - 调用m_flipState.Flip(rtIndex)执行翻转
         * - 添加调试日志记录翻转操作
         */
        void FlipRenderTarget(int rtIndex);

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
        // Mipmap生成 - Milestone 3.0延迟渲染管线
        // ========================================================================

        /**
         * @brief 生成所有启用Mipmap的RenderTarget的Mipmap
         * @param cmdList 命令列表
         *
         * 教学要点:
         * - 遍历16个RT，对启用Mipmap的调用GenerateMips()
         * - 对Main和Alt纹理都生成Mipmap
         * - 通常在写入RenderTarget后调用
         *
         * **Iris调用时机**:
         * - 在Composite Pass结束后
         * - 对需要Mipmap的RenderTarget生成多级细节纹理
         */
        void GenerateMipmaps(ID3D12GraphicsCommandList* cmdList);

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

        // ========================================================================
        // 动态colortex数量查询 - Milestone 3.0新增
        // ========================================================================

        /**
         * @brief 获取当前激活的colortex数量
         * @return int 激活数量 [1-16]
         *
         * 教学要点:
         * - 用于着色器验证索引有效性
         * - 用于调试信息输出
         * - 用于动态遍历激活的RT
         */
        int GetActiveColorTexCount() const { return m_activeColorTexCount; }

    private:
        // ========================================================================
        // 私有成员
        // ========================================================================

        // 动态colortex数组 - 支持1-16个colortex，优化内存使用（Milestone 3.0）
        // 例如：配置4个colortex比16个节省约75%内存（~99.4MB @ 1920x1080）
        std::vector<std::shared_ptr<D12RenderTarget>> m_renderTargets;

        // 当前激活的colortex数量，范围 [1, 16]
        int m_activeColorTexCount = MAX_COLOR_TEXTURES;

        RenderTargetFlipState    m_flipState; // Main/Alt翻转状态 (BufferFlipState<16>)
        int                      m_baseWidth; // 基准屏幕宽度
        int                      m_baseHeight; // 基准屏幕高度
        std::array<RTConfig, 16> m_settings; // RT配置缓存（最多16个）

        /**
         * @brief 验证rtIndex有效性 (内部辅助函数)
         * @return true if valid [0, m_activeColorTexCount)
         *
         * **Milestone 3.0更新**:
         * - 原逻辑: [0, 16)
         * - 新逻辑: [0, m_activeColorTexCount)
         * - 确保索引在激活范围内
         */
        bool IsValidIndex(int rtIndex) const
        {
            return rtIndex >= 0 && rtIndex < m_activeColorTexCount;
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
