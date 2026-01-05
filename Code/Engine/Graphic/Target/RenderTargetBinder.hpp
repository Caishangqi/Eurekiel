/**
 * @file RenderTargetBinder.hpp
 * @brief 渲染目标统一绑定器 - 聚合所有RT Manager并提供状态缓存机制
 *
 * 设计要点:
 * 1. 聚合4个Manager（RenderTargetManager, DepthTextureManager, ShadowColorManager, ShadowTextureManager）
 * 2. 提供统一的BindRenderTargets() API，支持RTType分类
 * 3. 状态缓存机制，避免冗余OMSetRenderTargets调用（目标：减少70%+状态切换）
 * 4. 延迟绑定机制，FlushBindings()才实际调用D3D12 API
 * 5. 便捷API（BindGBuffer, BindShadowPass等）
 *
 * 架构特性:
 * - 性能优化：状态Hash快速比较，早期退出机制
 * - 灵活性：支持LoadAction和ClearValue（Task 5集成）
 * - 向后兼容：不修改现有Manager接口
 *
 * 性能目标:
 * - 状态缓存命中率：70%+
 * - Hash计算开销：O(n)，n为RTV数量
 * - 早期退出优化：状态未变化时零开销
 */

#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <d3d12.h>
#include "RTTypes.hpp"

namespace enigma::graphic
{
    // 前向声明
    class RenderTargetManager;
    class DepthTextureManager;
    class ShadowColorManager;
    class ShadowTextureManager;

    /**
     * @class RenderTargetBinder
     * @brief 渲染目标统一绑定器
     *
     * 核心职责:
     * 1. Manager聚合 - 统一管理4个Manager的生命周期
     * 2. 统一绑定接口 - BindRenderTargets()支持RTType分类
     * 3. 状态缓存 - 避免冗余OMSetRenderTargets调用
     * 4. 延迟绑定 - 批量提交减少API调用次数
     * 5. 便捷API - 预定义常用绑定组合
     *
     * 使用示例:
     * ```cpp
     * // 绑定GBuffer（4个ColorTex + 1个DepthTex）
     * binder->BindGBuffer();
     * binder->FlushBindings(cmdList);
     *
     * // 绑定自定义组合
     * std::vector<RTType> rtTypes = {RTType::ColorTex, RTType::ShadowColor};
     * std::vector<int> indices = {0, 0};
     * binder->BindRenderTargets(rtTypes, indices, RTType::DepthTex, 0);
     * binder->FlushBindings(cmdList);
     * ```
     */
    class RenderTargetBinder
    {
    public:
        /**
         * @brief 构造RenderTargetBinder并聚合所有Manager
         * @param rtManager RenderTargetManager指针（管理ColorTex 0-15）
         * @param depthManager DepthTextureManager指针（管理DepthTex 0-2）
         * @param shadowColorManager ShadowColorManager指针（管理ShadowColor 0-7）
         * @param ShadowTextureManager ShadowTextureManager指针（管理ShadowTex 0-1）
         *
         * 教学要点:
         * - 使用原始指针而不是智能指针（Manager由RendererSubsystem管理生命周期）
         * - 不持有Manager所有权，仅保存引用
         */
        RenderTargetBinder(
            RenderTargetManager*  rtManager,
            DepthTextureManager*  depthManager,
            ShadowColorManager*   shadowColorManager,
            ShadowTextureManager* ShadowTextureManager
        );

        ~RenderTargetBinder() = default;

        // 禁用拷贝和移动（Binder类通常作为单例使用）
        RenderTargetBinder(const RenderTargetBinder&)            = delete;
        RenderTargetBinder& operator=(const RenderTargetBinder&) = delete;
        RenderTargetBinder(RenderTargetBinder&&)                 = delete;
        RenderTargetBinder& operator=(RenderTargetBinder&&)      = delete;

        // ========================================================================
        // 统一绑定接口
        // ========================================================================

        /**
         * @brief 绑定渲染目标组合
         * @param rtTypes RT类型数组（ColorTex, ShadowColor等）
         * @param indices 对应的索引数组
         * @param depthType 深度纹理类型（默认DepthTex）
         * @param depthIndex 深度纹理索引（默认0）
         * @param useAlt 是否使用Alt纹理（仅对支持Flipper的RT有效，true）
         * @param loadAction 加载动作（Load/Clear/DontCare，默认Load）
         * @param clearValues RTV清空值数组（可选，与rtTypes对应）
         * @param depthClearValue 深度清空值（可选，默认1.0深度+0模板）
         *
         * 教学要点:
         * - 根据RTType分发到对应Manager获取RTV句柄
         * - 更新pendingState，不立即调用D3D12 API
         * - useAlt参数仅对ShadowColor和ColorTex有效
         * - LoadAction::Clear时在FlushBindings()后自动清空RT
         * - clearValues可选，未提供则使用默认黑色（0,0,0,0）
         *
         * 使用示例:
         * ```cpp
         * std::vector<RTType> rtTypes = {RTType::ColorTex, RTType::ColorTex};
         * std::vector<int> indices = {0, 1};
         * std::vector<ClearValue> clearValues = {
         *     ClearValue::Color(0.2f, 0.3f, 0.4f, 1.0f),
         *     ClearValue::Color(0.0f, 0.0f, 0.0f, 1.0f)
         * };
         * binder->BindRenderTargets(rtTypes, indices, RTType::DepthTex, 0, false,
         *                           LoadAction::Clear, clearValues);
         * ```
         */
        void BindRenderTargets(
            const std::vector<RTType>&     rtTypes,
            const std::vector<int>&        indices,
            RTType                         depthType       = RTType::DepthTex,
            int                            depthIndex      = 0,
            bool                           useAlt          = false, // [FIX] 默认使用Main纹理而非Alt纹理
            LoadAction                     loadAction      = LoadAction::Load,
            const std::vector<ClearValue>& clearValues     = {},
            const ClearValue&              depthClearValue = ClearValue::Depth(1.0f, 0)
        );

        /**
         * @brief 绑定单个渲染目标 + 深度纹理
         * @param rtType RT类型
         * @param rtIndex RT索引
         * @param depthType 深度纹理类型
         * @param depthIndex 深度纹理索引
         * @param useAlt 是否使用Alt纹理
         * @param loadAction 加载动作（Load/Clear/DontCare，默认Load）
         * @param clearValue RTV清空值（可选，默认黑色）
         * @param depthClearValue 深度清空值（可选，默认1.0深度+0模板）
         *
         * 便捷方法，避免创建vector
         */
        void BindSingleRenderTarget(
            RTType            rtType,
            int               rtIndex,
            RTType            depthType       = RTType::DepthTex,
            int               depthIndex      = 0,
            bool              useAlt          = true,
            LoadAction        loadAction      = LoadAction::Load,
            const ClearValue& clearValue      = ClearValue::Color(Rgba8::BLACK),
            const ClearValue& depthClearValue = ClearValue::Depth(1.0f, 0)
        );

        // ========================================================================
        // 便捷绑定API
        // ========================================================================

        /**
         * @brief 绑定GBuffer（4个ColorTex + 1个DepthTex）
         *
         * 典型GBuffer布局:
         * - ColorTex0: Albedo + Metallic
         * - ColorTex1: Normal + Roughness
         * - ColorTex2: Emission + AO
         * - ColorTex3: Velocity
         * - DepthTex0: Depth
         */
        void BindGBuffer();

        /**
         * @brief 绑定阴影Pass（1个ShadowColor + 1个DepthTex）
         * @param shadowIndex ShadowColor索引（0-7）
         * @param depthIndex DepthTex索引（默认0）
         */
        void BindShadowPass(int shadowIndex, int depthIndex = 0);

        /**
         * @brief 绑定合成Pass（1个ColorTex）
         * @param colorIndex ColorTex索引（默认0）
         */
        void BindCompositePass(int colorIndex = 0);

        /**
         * @brief 清除所有绑定（解除所有RT）
         *
         * 教学要点:
         * - 清空pendingState和currentState
         * - 下次FlushBindings()将绑定NULL
         */
        void ClearBindings();

        // ========================================================================
        // 状态管理接口
        // ========================================================================

        /**
         * @brief 刷新绑定状态，实际调用OMSetRenderTargets
         * @param cmdList 命令列表指针
         *
         * 教学要点:
         * - 计算pendingState的Hash
         * - 与currentState的Hash比较，相同则早期退出
         * - 不同则调用OMSetRenderTargets并更新currentState
         *
         * 性能优化:
         * - Hash快速比较：O(n)，n为RTV数量
         * - 早期退出：状态未变化时零开销
         */
        void FlushBindings(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief 强制刷新绑定（跳过Hash检查）
         * @param cmdList 命令列表指针
         *
         * 使用场景:
         * - 外部因素导致状态变化（如ResizeBuffers）
         * - 调试时需要确保状态正确
         */
        void ForceFlushBindings(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief 检查是否有待提交的状态变化
         * @return 是否需要调用FlushBindings
         */
        bool HasPendingChanges() const;

        /**
         * @brief Get current bound RT formats
         * @param outFormats Output format array (8 elements)
         */
        void GetCurrentRTFormats(DXGI_FORMAT outFormats[8]) const;

        /**
         * @brief Get current depth format
         * @return Depth format
         */
        DXGI_FORMAT GetCurrentDepthFormat() const;

    private:
        // ========================================================================
        // 内部实现方法
        // ========================================================================

        /**
         * @brief 获取指定RTType的RTV句柄
         * @param type RT类型
         * @param index RT索引
         * @param useAlt 是否使用Alt纹理
         * @return RTV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(RTType type, int index, bool useAlt) const;

        /**
         * @brief 获取指定RTType的DSV句柄
         * @param type 深度纹理类型
         * @param index 深度纹理索引
         * @return DSV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(RTType type, int index) const;

        /**
         * @brief 执行Clear操作（内部方法）
         * @param cmdList 命令列表指针
         *
         * 教学要点:
         * - 根据loadAction决定是否执行Clear
         * - LoadAction::Clear时清空所有RTV和DSV
         * - LoadAction::DontCare时跳过Clear（性能优化）
         * - LoadAction::Load时不做任何操作
         */
        void PerformClearOperations(ID3D12GraphicsCommandList* cmdList);

        // ========================================================================
        // 内部数据结构
        // ========================================================================

        /**
         * @brief 绑定状态结构体
         *
         * 教学要点:
         * - rtvHandles存储所有RTV句柄
         * - dsvHandle存储深度模板句柄
         * - stateHash用于快速比较状态
         */
        struct BindingState
        {
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles; ///< RTV句柄数组
            std::vector<ClearValue>                  clearValues; ///< RTV清空值数组（与rtvHandles对应）
            D3D12_CPU_DESCRIPTOR_HANDLE              dsvHandle; ///< DSV句柄
            ClearValue                               depthClearValue; ///< DSV清空值
            LoadAction                               loadAction; ///< 加载动作
            uint32_t                                 stateHash; ///< 状态Hash值
            bool                                     useAlt; ///< [FIX] 是否使用Alt纹理标志

            /// 默认构造
            BindingState()
                : dsvHandle{0}
                  , depthClearValue(ClearValue::Depth(1.0f, 0))
                  , loadAction(LoadAction::Load)
                  , stateHash(0)
                  , useAlt(false)
            {
            }

            /**
             * @brief 计算状态Hash值
             * @return Hash值
             *
             * 教学要点:
             * - 使用XOR操作符组合所有句柄的ptr值
             * - O(n)复杂度，n为RTV数量
             * - 简单高效，适合状态比较
             */
            uint32_t ComputeHash() const
            {
                uint32_t hash = 0;
                for (const auto& rtv : rtvHandles)
                {
                    hash ^= static_cast<uint32_t>(rtv.ptr & 0xFFFFFFFF);
                }
                hash ^= static_cast<uint32_t>(dsvHandle.ptr & 0xFFFFFFFF);
                hash ^= (useAlt ? 0x80000000 : 0); // [FIX] 包含useAlt标志
                return hash;
            }

            /**
             * @brief 重置状态
             */
            void Reset()
            {
                rtvHandles.clear();
                clearValues.clear();
                dsvHandle       = D3D12_CPU_DESCRIPTOR_HANDLE{0};
                depthClearValue = ClearValue::Depth(1.0f, 0);
                loadAction      = LoadAction::Load;
                stateHash       = 0;
                useAlt          = false; // [FIX] 重置useAlt标志
            }
        };

        // ========================================================================
        // Manager指针（不持有所有权）
        // ========================================================================

        RenderTargetManager*  m_rtManager; ///< ColorTex管理器
        DepthTextureManager*  m_depthManager; ///< DepthTex管理器
        ShadowColorManager*   m_shadowColorManager; ///< ShadowColor管理器
        ShadowTextureManager* m_ShadowTextureManager; ///< ShadowTex管理器

        // ========================================================================
        // 状态管理
        // ========================================================================

        BindingState m_currentState; ///< 当前已提交的状态
        BindingState m_pendingState; ///< 待提交的状态

        // ========================================================================
        // 性能统计（可选，用于调试和优化）
        // ========================================================================

        uint32_t m_totalBindCalls; ///< 总绑定调用次数
        uint32_t m_cacheHitCount; ///< 状态缓存命中次数
        uint32_t m_actualBindCalls; ///< 实际API调用次数

        // ========================================================================
        // 格式缓存（用于状态查询）
        // ========================================================================

        DXGI_FORMAT m_currentRTFormats[8]; ///< 当前绑定的RT格式
        DXGI_FORMAT m_currentDepthFormat; ///< 当前绑定的深度格式
    };
} // namespace enigma::graphic
