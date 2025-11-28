#pragma once

#include "IWorldRenderingPipeline.hpp"
#include "WorldRenderingPhase.hpp"
#include <memory>
#include <atomic>

// Forward declarations
namespace Engine::Graphic::Resource
{
    class D12RenderTargets;
    class CommandListManager;
}

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的原版Minecraft渲染管线实现
     * 
     * 这个类直接对应Iris源码中的VanillaRenderingPipeline.java，
     * 实现了不使用着色器包的基础Minecraft渲染管线。它作为
     * EnigmaRenderingPipeline的简化版本和后备方案存在。
     * 
     * 教学目标：
     * - 理解原版Minecraft的基础渲染流程
     * - 学习渲染管线的简化实现策略
     * - 掌握接口实现的最小可行产品(MVP)原则
     * - 体验传统前向渲染与现代延迟渲染的区别
     * 
     * Iris架构对应：
     * Java: public class VanillaRenderingPipeline implements WorldRenderingPipeline
     * C++:  class VanillaRenderingPipeline : public IWorldRenderingPipeline
     * 
     * 技术特点：
     * - 前向渲染管线（Forward Rendering）
     * - 简化的光照模型
     * - 最小的GPU资源占用
     * - 快速的渲染路径
     */
    class VanillaRenderingPipeline final : public IWorldRenderingPipeline
    {
    private:
        // ===========================================
        // 核心状态管理
        // ===========================================

        /// 当前渲染阶段状态
        WorldRenderingPhase m_currentPhase = WorldRenderingPhase::NONE;

        /// 管线是否处于激活状态
        std::atomic<bool> m_isActive{false};

        /// 管线是否已初始化
        bool m_isInitialized = false;

        // ===========================================
        // DirectX 12核心资源
        // ===========================================

        /// 渲染目标管理器 - 管理颜色缓冲区和深度缓冲区
        std::unique_ptr<Engine::Graphic::Resource::D12RenderTargets> m_renderTargets;

        /// 命令列表管理器 - 管理GPU命令提交
        std::shared_ptr<Engine::Graphic::Resource::CommandListManager> m_commandManager;

        // ===========================================
        // 渲染配置参数
        // ===========================================

        /// 着色器渲染距离（原版管线使用-1表示默认距离）
        float m_renderDistance = -1.0f;

        /// 是否禁用原版雾效（原版管线始终为false）
        bool m_disableVanillaFog = false;

        /// 是否禁用定向阴影（原版管线始终为false）
        bool m_disableDirectionalShading = false;

    public:
        /**
         * @brief 构造函数
         * 
         * 初始化原版渲染管线，设置基础的DirectX 12资源
         * 和渲染状态。原版管线的初始化相对简单。
         * 
         * @param commandManager 命令列表管理器的共享指针
         * 
         * 教学重点：构造函数中的资源初始化策略
         */
        explicit VanillaRenderingPipeline(
            std::shared_ptr<Engine::Graphic::Resource::CommandListManager> commandManager);

        /**
         * @brief 析构函数
         * 
         * 自动清理所有GPU资源，得益于智能指针的RAII特性，
         * 大部分资源会自动释放。
         */
        ~VanillaRenderingPipeline() override;

        // 禁用拷贝和移动，确保管线的唯一性
        VanillaRenderingPipeline(const VanillaRenderingPipeline&)            = delete;
        VanillaRenderingPipeline& operator=(const VanillaRenderingPipeline&) = delete;
        VanillaRenderingPipeline(VanillaRenderingPipeline&&)                 = delete;
        VanillaRenderingPipeline& operator=(VanillaRenderingPipeline&&)      = delete;

        // ===========================================
        // IWorldRenderingPipeline接口实现
        // ===========================================

        /**
         * @brief 开始世界渲染
         * 
         * 原版管线的实现相对简单：
         * 1. 清理颜色和深度缓冲区
         * 2. 设置基础的渲染状态
         * 3. 准备前向渲染所需的资源
         * 
         * 教学对比：与EnigmaRenderingPipeline的复杂G-Buffer设置形成对比
         */
        void BeginWorldRendering() override;

        /**
         * @brief 结束世界渲染
         * 
         * 完成前向渲染的收尾工作：
         * 1. 执行最终的色彩校正
         * 2. 提交渲染命令到GPU
         * 3. 准备Present操作
         */
        void EndWorldRendering() override;

        /**
         * @brief 设置当前渲染阶段
         * 
         * 原版管线对阶段切换的处理相对简单，主要是：
         * 1. 更新当前阶段状态
         * 2. 设置对应的渲染状态
         * 3. 切换简单的固定管线着色器
         * 
         * @param phase 目标渲染阶段
         * 
         * 教学价值：对比复杂的着色器包阶段管理
         */
        void SetPhase(WorldRenderingPhase phase) override;

        /**
         * @brief 开始渲染Pass
         * 
         * 原版管线通常只有单一Pass，这个方法主要用于
         * 兼容性和接口一致性。
         * 
         * @param passIndex Pass索引（原版管线通常忽略）
         */
        void BeginPass(uint32_t passIndex = 0) override;

        /**
         * @brief 结束渲染Pass
         * 
         * 简单的Pass结束处理，主要是状态重置。
         */
        void EndPass() override;

        // ===========================================
        // 阴影渲染方法（原版简化实现）
        // ===========================================

        /**
         * @brief 开始级联渲染
         * 
         * 原版管线的世界渲染准备工作：
         * 1. 设置相机变换矩阵
         * 2. 配置视锥体剔除
         * 3. 准备基础光照参数
         * 
         * 教学重点：传统渲染管线的简洁性
         */
        void BeginLevelRendering() override;

        /**
         * @brief 渲染阴影贴图
         * 
         * 原版管线的阴影处理非常简化：
         * 1. 基础的环境光遮蔽
         * 2. 简单的方向光阴影
         * 3. 无复杂的级联阴影系统
         * 
         * 教学对比：与现代阴影技术的差异
         */
        void RenderShadows() override;

        /**
         * @brief 结束级联渲染
         * 
         * 恢复主相机设置，清理临时渲染状态。
         */
        void EndLevelRendering() override;

        // ===========================================
        // 高级渲染控制方法
        // ===========================================

        /**
         * @brief 检查是否应该禁用原版雾效
         * 
         * 原版管线永远返回false，因为它就是原版实现。
         * 
         * @return 始终返回false
         * 
         * 教学价值：接口设计中的语义明确性
         */
        bool ShouldDisableVanillaFog() const override;

        /**
         * @brief 检查是否应该禁用原版定向阴影
         * 
         * 原版管线永远返回false，保持原版的定向阴影。
         * 
         * @return 始终返回false
         */
        bool ShouldDisableDirectionalShading() const override;

        /**
         * @brief 获取着色器渲染距离
         * 
         * 原版管线返回-1，表示使用游戏设置中的默认渲染距离。
         * 
         * @return -1.0f（使用默认渲染距离）
         */
        float GetShaderRenderDistance() const override;

        // ===========================================
        // 状态查询方法
        // ===========================================

        /**
         * @brief 获取当前渲染阶段
         * 
         * 返回管线当前的渲染阶段状态。
         * 
         * @return 当前的WorldRenderingPhase枚举值
         */
        WorldRenderingPhase GetCurrentPhase() const override;

        /**
         * @brief 检查管线是否处于激活状态
         * 
         * 查询当前管线是否正在处理渲染任务。
         * 使用原子操作确保线程安全。
         * 
         * @return true如果管线正在渲染
         */
        bool IsActive() const override;

        // ===========================================
        // 资源管理回调
        // ===========================================

        /**
         * @brief 通知帧更新
         * 
         * 原版管线的帧更新处理：
         * 1. 更新基础的uniform变量（时间、相机位置等）
         * 2. 更新简单的动画状态
         * 3. 处理渲染设置的变化
         * 
         * 教学重点：最小化的每帧更新逻辑
         */
        void OnFrameUpdate() override;

        /**
         * @brief 通知资源重载
         * 
         * 原版管线的资源重载相对简单：
         * 1. 重新创建基础的着色器
         * 2. 更新渲染配置
         * 3. 重置渲染状态
         */
        void Reload() override;

        /**
         * @brief 销毁管线资源
         * 
         * 释放所有GPU资源和系统资源。
         * 智能指针会自动处理大部分清理工作。
         * 
         * 教学价值：RAII在资源管理中的优势
         */
        void Destroy() override;

    private:
        // ===========================================
        // 内部辅助方法
        // ===========================================

        /**
         * @brief 初始化渲染管线
         * 
         * 创建所有必要的DirectX 12资源：
         * 1. 渲染目标和深度缓冲区
         * 2. 基础的固定管线着色器
         * 3. 常用的渲染状态对象
         * 
         * @return true如果初始化成功
         */
        bool Initialize();

        /**
         * @brief 设置基础渲染状态
         * 
         * 配置原版管线的默认渲染状态：
         * 1. 深度测试和深度写入
         * 2. Alpha混合模式
         * 3. 光栅化状态
         * 4. 采样器状态
         */
        void SetupDefaultRenderState();

        /**
         * @brief 执行特定阶段的渲染
         * 
         * 根据当前阶段执行对应的渲染操作。
         * 原版管线的阶段处理相对简单直接。
         * 
         * @param phase 要执行的渲染阶段
         */
        void ExecutePhase(WorldRenderingPhase phase);

        /**
         * @brief 清理临时资源
         * 
         * 清理每帧结束后不再需要的临时资源，
         * 为下一帧的渲染做准备。
         */
        void CleanupTemporaryResources();

        /**
         * @brief 验证管线状态
         * 
         * 调试和验证用的方法，检查管线状态的一致性。
         * 主要在Debug模式下使用。
         * 
         * @return true如果管线状态正常
         */
        bool ValidatePipelineState() const;
    };
} // namespace enigma::graphic
