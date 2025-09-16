#pragma once

#include "IWorldRenderingPipeline.hpp"
#include <string>
#include <memory>
#include <functional>

namespace enigma::graphic
{
    // Forward declarations
    class ShaderPackManager;
    class UniformManager;

    /**
     * @brief Iris兼容的着色器渲染管线扩展接口
     * 
     * 这个接口直接对应Iris源码中的ShaderRenderingPipeline.java，
     * 它继承自WorldRenderingPipeline并添加了着色器包特有的功能。
     * EnigmaRenderingPipeline将实现此接口以支持完整的Iris着色器系统。
     * 
     * 教学目标：
     * - 理解接口继承在复杂系统中的应用
     * - 学习着色器包管理的架构设计
     * - 掌握渲染管线的功能分层设计
     * 
     * Iris架构对应：
     * Java: public interface ShaderRenderingPipeline extends WorldRenderingPipeline
     * C++:  class IShaderRenderingPipeline : public IWorldRenderingPipeline
     */
    class IShaderRenderingPipeline : public IWorldRenderingPipeline
    {
    public:
        virtual ~IShaderRenderingPipeline() = default;

        // ===========================================
        // 着色器包管理接口
        // ===========================================

        /**
         * @brief 获取着色器包管理器
         * 对应Iris: getShaderMap()
         * 
         * 返回当前着色器管线使用的着色器包管理器，用于
         * 访问所有已加载的着色器程序和相关资源。
         * 
         * @return 着色器包管理器的智能指针
         * 
         * 教学重点：资源管理器模式的应用
         */
        virtual std::shared_ptr<ShaderPackManager> GetShaderPackManager() const = 0;

        /**
         * @brief 获取Uniform变量管理器
         * 对应Iris内部的UniformManager系统
         * 
         * 返回负责管理所有着色器uniform变量的管理器，
         * 包括相机矩阵、时间、光照参数等。
         * 
         * @return Uniform管理器的智能指针
         */
        virtual std::shared_ptr<UniformManager> GetUniformManager() const = 0;

        // ===========================================
        // 着色器程序控制接口
        // ===========================================

        /**
         * @brief 激活指定的着色器程序
         * 对应Iris: useProgram(String programName)
         * 
         * 切换到指定名称的着色器程序，设置对应的渲染状态
         * 和uniform变量绑定。
         * 
         * @param programName 着色器程序名称 (如: "gbuffers_basic", "composite1")
         * @return true如果成功激活程序
         * 
         * 教学价值：着色器程序管理和状态切换
         */
        virtual bool UseProgram(const std::string& programName) = 0;

        /**
         * @brief 检查着色器程序是否存在
         * 对应Iris: hasProgram(String programName)
         * 
         * 查询指定名称的着色器程序是否已加载且可用。
         * 用于动态检查着色器包的功能支持。
         * 
         * @param programName 着色器程序名称
         * @return true如果程序存在且可用
         */
        virtual bool HasProgram(const std::string& programName) const = 0;

        /**
         * @brief 重新编译所有着色器程序
         * 对应Iris: reloadShaders()
         * 
         * 重新从源文件编译所有着色器程序，通常在
         * 着色器包更新或渲染设置改变时调用。
         * 
         * @return true如果重编译成功
         * 
         * 教学重点：着色器热重载机制
         */
        virtual bool ReloadShaders() = 0;

        // ===========================================
        // 帧更新通知接口
        // ===========================================

        /**
         * @brief 注册帧更新通知回调
         * 对应Iris: FrameUpdateNotifier.addListener()
         * 
         * 注册一个在每帧更新时被调用的回调函数，
         * 用于更新时间相关的uniform变量或动画状态。
         * 
         * @param callback 帧更新回调函数
         * 
         * 教学价值：观察者模式在渲染系统中的应用
         */
        virtual void AddFrameUpdateListener(std::function<void()> callback) = 0;

        /**
         * @brief 移除帧更新通知回调
         * 对应Iris: FrameUpdateNotifier.removeListener()
         * 
         * 移除之前注册的帧更新回调，防止内存泄漏
         * 和无效的回调调用。
         * 
         * @param callbackId 回调ID（可以是函数指针或其他标识）
         */
        virtual void RemoveFrameUpdateListener(size_t callbackId) = 0;

        // ===========================================
        // 渲染目标管理接口
        // ===========================================

        /**
         * @brief 获取指定索引的颜色纹理
         * 对应Iris: getRenderTarget(int index)
         * 
         * 访问指定索引的颜色渲染目标（colortex0-15），
         * 用于在着色器中进行纹理采样或作为渲染目标绑定。
         * 
         * @param index 渲染目标索引 (0-15)
         * @return 渲染目标纹理的原始指针（生命周期由管线管理）
         * 
         * 教学重点：多渲染目标(MRT)的资源访问
         */
        virtual void* GetColorTexture(uint32_t index) const = 0;

        /**
         * @brief 获取深度纹理
         * 对应Iris: getDepthTexture()
         * 
         * 访问当前的深度缓冲区纹理，用于深度相关的
         * 后处理效果或阴影计算。
         * 
         * @return 深度纹理的原始指针
         */
        virtual void* GetDepthTexture() const = 0;

        /**
         * @brief 执行缓冲区翻转
         * 对应Iris: BufferFlipper.flip()
         * 
         * 在乒乓缓冲区之间进行切换，通常在composite
         * pass之间调用，用于实现多pass后处理效果。
         * 
         * 教学价值：乒乓缓冲区技术在后处理中的应用
         */
        virtual void FlipBuffers() = 0;

        // ===========================================
        // 着色器包信息查询接口
        // ===========================================

        /**
         * @brief 获取当前着色器包名称
         * 对应Iris: getCurrentShaderPackName()
         * 
         * 返回当前加载的着色器包的名称，主要用于
         * 调试信息显示和日志记录。
         * 
         * @return 着色器包名称字符串
         */
        virtual std::string GetCurrentShaderPackName() const = 0;

        /**
         * @brief 检查是否启用了着色器包
         * 对应Iris: isShaderPackEnabled()
         * 
         * 查询当前是否有有效的着色器包在运行，
         * 如果返回false，则管线应该回退到原版渲染。
         * 
         * @return true如果着色器包已启用且正常工作
         */
        virtual bool IsShaderPackEnabled() const = 0;

        /**
         * @brief 获取着色器包版本信息
         * 对应Iris: getShaderPackVersion()
         * 
         * 返回当前着色器包的版本信息，用于兼容性
         * 检查和特性支持判断。
         * 
         * @return 版本字符串
         */
        virtual std::string GetShaderPackVersion() const = 0;

        // ===========================================
        // 高级渲染控制接口
        // ===========================================

        /**
         * @brief 设置着色器包配置选项
         * 对应Iris: setShaderPackOption(String key, String value)
         * 
         * 动态修改着色器包的配置选项，例如开启/关闭
         * 特定的视觉效果或调整渲染质量。
         * 
         * @param optionName 配置选项名称
         * @param value 配置值
         * @return true如果设置成功
         * 
         * 教学价值：配置系统在图形管线中的设计
         */
        virtual bool SetShaderPackOption(const std::string& optionName,
                                         const std::string& value) = 0;

        /**
         * @brief 获取着色器包配置选项
         * 对应Iris: getShaderPackOption(String key)
         * 
         * 查询着色器包的当前配置选项值。
         * 
         * @param optionName 配置选项名称
         * @return 配置选项值，空字符串表示选项不存在
         */
        virtual std::string GetShaderPackOption(const std::string& optionName) const = 0;

        // ===========================================
        // 调试和性能监控接口
        // ===========================================

        /**
         * @brief 启用着色器调试模式
         * 
         * 开启着色器编译的详细错误信息和性能统计，
         * 主要用于开发和调试阶段。
         * 
         * @param enable true启用调试模式
         */
        virtual void SetDebugMode(bool enable) = 0;

        /**
         * @brief 获取渲染统计信息
         * 
         * 返回当前帧的渲染统计数据，包括Draw Call数量、
         * 三角形数量、纹理绑定次数等性能指标。
         * 
         * @return 格式化的统计信息字符串
         * 
         * 教学价值：渲染性能分析的重要性
         */
        virtual std::string GetRenderingStats() const = 0;
    };
} // namespace enigma::graphic
