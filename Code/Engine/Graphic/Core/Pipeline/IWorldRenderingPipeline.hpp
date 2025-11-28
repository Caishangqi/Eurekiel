#pragma once

#include <memory>
#include <functional>

namespace enigma::graphic
{
    // Forward declaration
    enum class WorldRenderingPhase : uint32_t;

    /**
     * @brief Iris兼容的世界渲染管线基础接口
     * 
     * 这个接口直接对应Iris源码中的WorldRenderingPipeline.java，定义了
     * 完整的世界渲染管线生命周期管理。所有的渲染管线实现类都必须
     * 实现此接口，包括VanillaRenderingPipeline和EnigmaRenderingPipeline。
     * 
     * 教学目标：
     * - 理解接口抽象在渲染管线中的应用
     * - 学习管线生命周期的标准化管理
     * - 体会基于接口的多态设计模式
     * 
     * Iris架构对应：
     * Java: public interface WorldRenderingPipeline
     * C++:  class IWorldRenderingPipeline (抽象基类)
     */
    class IWorldRenderingPipeline
    {
    public:
        virtual ~IWorldRenderingPipeline() = default;

        // ===========================================
        // 核心生命周期方法 - 对应Iris接口设计
        // ===========================================

        /**
         * @brief 开始世界渲染
         * 对应Iris: beginWorldRendering()
         * 
         * 初始化渲染管线，设置全局渲染状态，准备所有必要的
         * 渲染目标和资源。这是每帧渲染的起始点。
         */
        virtual void BeginWorldRendering() = 0;

        /**
         * @brief 结束世界渲染
         * 对应Iris: endWorldRendering()
         * 
         * 清理渲染状态，提交最终的渲染结果到后缓冲区。
         * 释放临时资源，为下一帧做准备。
         */
        virtual void EndWorldRendering() = 0;

        /**
         * @brief 设置当前渲染阶段
         * 对应Iris: setPhase(WorldRenderingPhase phase)
         * 
         * 这是Iris管线的核心方法，用于切换不同的渲染阶段。
         * 每个阶段会激活对应的着色器和渲染状态。
         * 
         * @param phase 渲染阶段枚举值
         * 
         * 教学重点：状态机模式在图形管线中的应用
         */
        virtual void SetPhase(WorldRenderingPhase phase) = 0;

        /**
         * @brief 开始渲染Pass
         * 对应Iris: beginPass()
         * 
         * 在特定阶段内开始一个渲染Pass，设置对应的
         * 渲染目标、视口和着色器状态。
         * 
         * @param passIndex Pass索引 (例如: composite1, composite2等)
         */
        virtual void BeginPass(uint32_t passIndex = 0) = 0;

        /**
         * @brief 结束渲染Pass
         * 对应Iris: endPass()
         * 
         * 完成当前Pass的渲染，可能涉及缓冲区切换、
         * Mipmap生成等后处理操作。
         */
        virtual void EndPass() = 0;

        // ===========================================
        // 阴影渲染管线方法
        // ===========================================

        /**
         * @brief 开始级联渲染（主要是Minecraft世界）
         * 对应Iris: beginLevelRendering()
         * 
         * 初始化世界级别的渲染，设置相机矩阵、视锥体剔除
         * 和全局光照参数。这个方法在阴影渲染前调用。
         */
        virtual void BeginLevelRendering() = 0;

        /**
         * @brief 渲染阴影贴图
         * 对应Iris: renderShadows(...)
         * 
         * 执行阴影贴图的渲染，包括级联阴影贴图的生成。
         * 这个阶段会从光源视角渲染场景到深度缓冲区。
         * 
         * 教学价值：级联阴影贴图技术的实践应用
         */
        virtual void RenderShadows() = 0;

        /**
         * @brief 结束级联渲染
         * 对应Iris: endLevelRendering()
         * 
         * 完成世界级别渲染的清理工作，恢复主相机视角。
         */
        virtual void EndLevelRendering() = 0;

        // ===========================================
        // 高级渲染控制方法
        // ===========================================

        /**
         * @brief 检查是否应该禁用原版雾效
         * 对应Iris: shouldDisableVanillaFog()
         * 
         * Iris着色器包可能需要自定义雾效实现，此方法
         * 用于查询是否应该禁用Minecraft的原版雾效。
         * 
         * @return true如果应该禁用原版雾效
         */
        virtual bool ShouldDisableVanillaFog() const = 0;

        /**
         * @brief 检查是否应该禁用原版云渲染
         * 对应Iris: shouldDisableDirectionalShading()
         * 
         * 某些着色器包需要完全控制定向光照，此方法
         * 用于查询是否应该禁用原版的定向阴影。
         * 
         * @return true如果应该禁用定向阴影
         */
        virtual bool ShouldDisableDirectionalShading() const = 0;

        /**
         * @brief 获取着色器渲染距离
         * 对应Iris: getShaderRenderDistance()
         * 
         * 着色器包可能定义自定义的渲染距离，此方法
         * 返回着色器包指定的渲染距离值。
         * 
         * @return 渲染距离，-1表示使用默认距离
         */
        virtual float GetShaderRenderDistance() const = 0;

        // ===========================================
        // 状态查询方法
        // ===========================================

        /**
         * @brief 获取当前渲染阶段
         * 
         * 返回管线当前所处的渲染阶段，主要用于
         * 调试和状态跟踪。
         * 
         * @return 当前的WorldRenderingPhase枚举值
         */
        virtual WorldRenderingPhase GetCurrentPhase() const = 0;

        /**
         * @brief 检查管线是否处于激活状态
         * 
         * 查询当前管线是否正在处理渲染任务。
         * 主要用于并发控制和状态验证。
         * 
         * @return true如果管线正在渲染
         */
        virtual bool IsActive() const = 0;

        // ===========================================
        // 资源管理回调
        // ===========================================

        /**
         * @brief 通知帧更新
         * 对应Iris: onFrameUpdate()
         * 
         * 每帧开始时调用，用于更新时间相关的uniform变量、
         * 动画状态和其他需要逐帧更新的资源。
         */
        virtual void OnFrameUpdate() = 0;

        /**
         * @brief 通知资源重载
         * 对应Iris: reload()
         * 
         * 当着色器包或渲染设置发生变化时调用，
         * 用于重新初始化管线资源。
         */
        virtual void Reload() = 0;

        /**
         * @brief 销毁管线资源
         * 对应Iris: destroy()
         * 
         * 释放所有GPU资源，包括缓冲区、纹理、着色器等。
         * 通常在引擎关闭或管线切换时调用。
         * 
         * 教学重点：RAII在图形资源管理中的应用
         */
        virtual void Destroy() = 0;
    };
} // namespace enigma::graphic
