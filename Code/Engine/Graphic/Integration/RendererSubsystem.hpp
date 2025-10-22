/**
 * @file RendererSubsystem.hpp
 * @brief Enigma引擎渲染子系统 - DirectX 12延迟渲染管线管理器
 * 
 * 教学重点:
 * 1. 理解新的灵活渲染架构（BeginFrame/Render/EndFrame + 60+ API）
 * 2. 学习DirectX 12的现代化资源管理
 * 3. 掌握引擎子系统的生命周期管理
 * 4. 理解双ShaderPack架构和Fallback机制
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <filesystem>

#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Window/Window.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "../Core/Pipeline/WorldRenderingPhase.hpp"
#include "../Core/RenderState.hpp"
#include "../Immediate/RenderCommand.hpp"
#include "../Immediate/RenderCommandQueue.hpp"
#include "../Shader/ShaderPack/ShaderPack.hpp"
#include "RendererSubsystemConfig.hpp" // Milestone 3.0: 配置系统重构
#include "Engine/Graphic/Target/RenderTargetManager.hpp"

// 使用Enigma核心命名空间中的EngineSubsystem
using namespace enigma::core;

namespace enigma::graphic
{
    // 前向声明 - 避免循环包含
    class RenderCommandQueue;
    class ShaderProgram;
    class ShaderSource;
    class ProgramSet;
    class D12VertexBuffer;
    class D12IndexBuffer;

    /**
     * @brief DirectX 12渲染子系统管理器
     * @details 
     * 这个类是整个延迟渲染系统的入口点和生命周期管理器。它继承自EngineSubsystem，
     * 与Enigma引擎的其他子系统协同工作。
     * 
     * 教学要点:
     * - 子系统模式的设计模式应用
     * - Iris渲染管线的生命周期管理  
     * - DirectX 12设备和命令队列的初始化
     * - 智能指针在资源管理中的应用
     * 
     * DirectX 12特性:
     * - 显式的GPU资源管理
     * - Command List和Command Queue的分离
     * - Bindless描述符堆的使用
     * - 多线程渲染支持的基础架构
     * 
     * @note 这是教学项目，重点在于理解渲染管线概念而非极限性能
     */
    class RendererSubsystem final : public EngineSubsystem
    {
    public:
        /**
         * @brief 渲染统计信息
         * @details 用于性能分析和调试的统计数据
         *
         * Milestone 3.0: Configuration已合并到RendererSubsystemConfig
         * 不再使用内嵌Configuration结构体，统一使用独立的Config类
         */
        struct RenderStatistics
        {
            uint64_t frameIndex           = 0; ///< 当前帧索引
            float    frameTime            = 0.0f; ///< 帧时间(毫秒)
            float    gpuTime              = 0.0f; ///< GPU时间(毫秒)
            uint32_t drawCalls            = 0; ///< 绘制调用数量
            uint32_t trianglesRendered    = 0; ///< 渲染三角形数量
            uint32_t activeShaderPrograms = 0; ///< 活跃着色器程序数量
            size_t   gpuMemoryUsed        = 0; ///< GPU内存使用量(字节)

            // ==================== Immediate模式统计 ====================
            std::map<WorldRenderingPhase, uint64_t> commandsPerPhase; ///< 每个阶段的指令数量
            uint64_t                                totalCommandsSubmitted = 0; ///< 提交的总指令数量
            uint64_t                                totalCommandsExecuted  = 0; ///< 已执行的总指令数量
            float                                   averageCommandTime     = 0.0f; ///< 平均指令执行时间(微秒)
            WorldRenderingPhase                     currentPhase           = WorldRenderingPhase::NONE; ///< 当前渲染阶段

            /**
             * @brief 重置统计信息
             */
            void Reset()
            {
                drawCalls            = 0;
                trianglesRendered    = 0;
                activeShaderPrograms = 0;

                // 重置immediate模式统计
                commandsPerPhase.clear();
                totalCommandsSubmitted = 0;
                totalCommandsExecuted  = 0;
                averageCommandTime     = 0.0f;
                currentPhase           = WorldRenderingPhase::NONE;
            }
        };

    public:
        /**
         * @brief 构造函数
         * @details 初始化基本成员，但不进行重型初始化工作
         *
         * Milestone 3.0: 参数类型从Configuration改为RendererSubsystemConfig
         */
        explicit RendererSubsystem(RendererSubsystemConfig& config);

        /**
         * @brief 析构函数
         * @details 确保所有资源正确释放，遵循RAII原则
         */
        ~RendererSubsystem() override;

        // ==================== EngineSubsystem接口实现 ====================

        /// 使用引擎提供的宏来简化子系统注册
        DECLARE_SUBSYSTEM(RendererSubsystem, "RendererSubsystem", 80)

        /**
         * @brief 早期初始化阶段
         * @details 
         * 在所有子系统的Startup之前调用，用于：
         * - 委托D3D12RenderSystem初始化DirectX 12设备和命令队列
         * - 设置调试和验证层
         * 
         * 教学要点: 理解为什么图形设备需要最早初始化，以及架构分层的重要性
         */
        void Initialize() override;

        /**
         * @brief 主要启动阶段
         * @details
         * 在Initialize阶段之后调用，用于：
         * - 加载引擎默认ShaderPack（必需）
         * - 加载用户ShaderPack（可选）
         * - 初始化渲染资源和状态
         */
        void Startup() override;

        /**
         * @brief 关闭子系统
         * @details
         * 释放所有管理的资源，确保无内存泄漏：
         * - 释放所有高级渲染组件
         * - 委托D3D12RenderSystem进行底层资源清理
         * 
         * @note 析构函数会调用此方法，但显式调用更安全
         */
        void Shutdown() override;

        bool RequiresInitialize() const override { return true; }

        // ==================== 游戏循环接口 - 对应Iris管线生命周期 ====================

        /**
         * @brief 帧开始处理
         * @details
         * 对应Iris管线的setup和begin阶段：
         * - setup1-99: GPU状态初始化、SSBO设置
         * - begin1-99: 每帧参数更新、摄像机矩阵计算
         * 
         * 教学要点:
         * - 理解为什么每帧开始需要更新全局参数
         * - 学习GPU状态管理的重要性
         * - 掌握Command List的开始记录
         */
        void BeginFrame() override;

        void RenderFrame();

        /**
         * @brief 帧结束处理
         * @details
         * 对应Iris管线的final阶段：
         * - final: 最终输出处理
         * - Present: 交换链呈现
         * - 性能统计收集
         * - Command List提交和同步
         * 
         * 教学要点:
         * - 理解双缓冲和垂直同步
         * - 学习GPU/CPU同步的重要性
         */
        void EndFrame() override;

        // ==================== Immediate模式渲染接口 - 用户友好API ====================

        /**
         * @brief 获取渲染指令队列
         * @return RenderCommandQueue指针，如果未初始化返回nullptr
         * @details 
         * 提供对immediate模式渲染指令队列的访问
         * 用户可以通过此接口提交自定义渲染指令
         */
        RenderCommandQueue* GetRenderCommandQueue() const noexcept
        {
            return m_renderCommandQueue.get();
        }

        /**
         * @brief 提交绘制指令到指定阶段
         * @param command 要执行的绘制指令
         * @param phase 目标渲染阶段
         * @param debugTag 调试标签，用于性能分析
         * @return 成功返回true
         * @details 
         * immediate模式的核心接口，支持按阶段分类存储
         */
        bool SubmitRenderCommand(RenderCommandPtr command, WorldRenderingPhase phase, const std::string& debugTag = "");

        /**
         * @brief 提交绘制指令（自动检测阶段）
         * @param command 要执行的绘制指令
         * @param debugTag 调试标签
         * @return 成功返回true
         * @details 使用PhaseDetector自动判断指令应该归属的阶段
         */
        bool SubmitRenderCommand(RenderCommandPtr command, const std::string& debugTag = "");

        /**
         * @brief 设置当前渲染阶段
         * @param phase Iris渲染阶段
         * @details 
         * 用于自动Phase检测和状态管理
         * 影响指令验证和性能分析
         */
        void SetCurrentRenderPhase(WorldRenderingPhase phase);

        /**
         * @brief 获取当前渲染阶段
         * @return 当前的Iris渲染阶段
         */
        WorldRenderingPhase GetCurrentRenderPhase() const;

        /**
         * @brief 获取指定阶段的指令数量
         * @param phase 渲染阶段
         * @return 指令数量
         */
        size_t GetCommandCount(WorldRenderingPhase phase) const;

        /**
         * @brief immediate模式便捷绘制接口 - DrawIndexed
         * @param indexCount 索引数量
         * @param phase 渲染阶段
         * @param startIndexLocation 起始索引位置
         * @param baseVertexLocation 基础顶点位置
         * @return 成功返回true
         * @details 直接创建并提交DrawIndexed指令的便捷方法
         */
        bool DrawIndexed(
            uint32_t            indexCount,
            WorldRenderingPhase phase,
            uint32_t            startIndexLocation = 0,
            int32_t             baseVertexLocation = 0
        );

        /**
         * @brief immediate模式便捷绘制接口 - DrawInstanced
         * @param vertexCountPerInstance 每实例顶点数
         * @param instanceCount 实例数量
         * @param phase 渲染阶段
         * @param startVertexLocation 起始顶点位置
         * @param startInstanceLocation 起始实例位置
         * @return 成功返回true
         */
        bool DrawInstanced(
            uint32_t            vertexCountPerInstance,
            uint32_t            instanceCount,
            WorldRenderingPhase phase,
            uint32_t            startVertexLocation   = 0,
            uint32_t            startInstanceLocation = 0
        );

        /**
         * @brief immediate模式便捷绘制接口 - DrawIndexedInstanced
         * @param indexCountPerInstance 每实例索引数
         * @param instanceCount 实例数量
         * @param phase 渲染阶段
         * @param startIndexLocation 起始索引位置
         * @param baseVertexLocation 基础顶点位置
         * @param startInstanceLocation 起始实例位置
         * @return 成功返回true
         */
        bool DrawIndexedInstanced(
            uint32_t            indexCountPerInstance,
            uint32_t            instanceCount,
            WorldRenderingPhase phase,
            uint32_t            startIndexLocation    = 0,
            int32_t             baseVertexLocation    = 0,
            uint32_t            startInstanceLocation = 0
        );

        // ==================== M2 灵活渲染接口 ====================

        // TODO: M2 - 实现60+ API灵活渲染接口
        // BeginCamera/EndCamera, UseProgram, DrawVertexBuffer等
        // 替代旧的Phase系统（已删除IWorldRenderingPipeline/PipelineManager）

        /**
         * @brief 开始Camera渲染
         * @param camera 要使用的相机
         *
         * 教学要点：
         * - 设置View-Projection矩阵
         * - 更新Camera相关的Uniform Buffer
         * - 对应Iris的setCamera()
         */
        void BeginCamera(const class Camera& camera);

        /**
         * @brief 结束Camera渲染
         *
         * 教学要点：
         * - 清理Camera状态
         * - 对应Iris的endCamera()
         */
        void EndCamera();

        /**
         * @brief 创建VertexBuffer
         * @param size 缓冲区大小（字节）
         * @param stride 顶点步长（字节）
         * @return D12VertexBuffer指针
         *
         * 教学要点：
         * - 创建GPU可见的顶点缓冲区
         * - 使用D3D12RenderSystem::CreateVertexBufferTyped()
         * - 返回D12VertexBuffer*（新架构）
         * - 调用者负责管理生命周期（或由RendererSubsystem内部管理）
         */
        D12VertexBuffer* CreateVertexBuffer(size_t size, unsigned stride);

        /**
         * @brief 设置VertexBuffer到渲染管线
         * @param buffer 要绑定的D12VertexBuffer
         * @param slot 顶点缓冲区槽位（默认0）
         *
         * 教学要点：
         * - 设置当前活动的VertexBuffer
         * - 对应OpenGL的glBindBuffer
         * - DirectX 12使用IASetVertexBuffers()绑定
         * - 委托给D3D12RenderSystem::BindVertexBuffer()
         */
        void SetVertexBuffer(D12VertexBuffer* buffer, uint32_t slot = 0);

        /**
         * @brief 设置IndexBuffer到渲染管线
         * @param buffer 要绑定的D12IndexBuffer
         *
         * 教学要点：
         * - 设置当前活动的IndexBuffer
         * - 对应OpenGL的glBindBuffer(GL_ELEMENT_ARRAY_BUFFER)
         * - DirectX 12使用IASetIndexBuffer()绑定
         * - IndexBuffer只有一个槽位（与VertexBuffer不同）
         * - 委托给D3D12RenderSystem::BindIndexBuffer()
         */
        void SetIndexBuffer(D12IndexBuffer* buffer);

        /**
         * @brief 更新Buffer数据（CPU → GPU）
         * @param buffer 目标D12VertexBuffer
         * @param data CPU数据指针
         * @param size 数据大小（字节）
         * @param offset 偏移量（字节，默认0）
         *
         * 教学要点：
         * - 数据上传到GPU
         * - 调用D12VertexBuffer的Update()方法
         * - 对应DirectX 11的UpdateSubresource
         * - 对应OpenGL的glBufferSubData
         * - 对应Vulkan的vkMapMemory
         *
         * 行业标准参考：
         * - DirectX 11: UpdateSubresource()
         * - OpenGL: glBufferSubData()
         * - Vulkan: vkMapMemory() + memcpy
         * - Metal: contents() + didModifyRange()
         */
        void UpdateBuffer(D12VertexBuffer* buffer, const void* data, size_t size, size_t offset = 0);

        /**
         * @brief 绘制顶点（非索引）
         * @param vertexCount 顶点数量
         * @param startVertex 起始顶点偏移（默认0）
         *
         * 教学要点：
         * - 执行Draw指令
         * - 对应OpenGL的glDrawArrays
         * - 对应DirectX 11的Draw
         * - 使用当前绑定的VertexBuffer
         */
        void Draw(uint32_t vertexCount, uint32_t startVertex = 0);

        /**
         * @brief 执行索引绘制
         * @param indexCount 索引数量
         * @param startIndex 起始索引（默认0）
         * @param baseVertex 基础顶点偏移（默认0）
         *
         * 教学要点：
         * - 执行DrawIndexed指令
         * - 对应OpenGL的glDrawElements
         * - 对应DirectX 11的DrawIndexed
         * - 使用当前绑定的IndexBuffer和VertexBuffer
         */
        void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0);

        /**
         * @brief 绘制全屏四边形
         *
         * 教学要点：
         * - 便捷方法用于后处理
         * - 自动管理顶点数据
         */
        void DrawFullscreenQuad();

        /**
         * @brief 设置混合模式
         * @param mode 混合模式
         *
         * 教学要点：
         * - 控制颜色混合
         * - 对应OpenGL的glBlendFunc
         */
        void SetBlendMode(BlendMode mode);

        /**
         * @brief 设置深度模式
         * @param mode 深度模式
         *
         * 教学要点：
         * - 控制深度测试
         * - 对应OpenGL的glDepthFunc
         */
        void SetDepthMode(DepthMode mode);

        // ==================== GBuffer配置 (Milestone 3.0) ====================

        /**
         * @brief 配置GBuffer的colortex数量
         * @param colorTexCount colortex数量，范围 [1, 16]
         *
         * @note 配置时机：
         *   - 选项1（推荐）: 在Initialize()之前调用ConfigureGBuffer()
         *   - 选项2: 在Initialize()时从配置文件读取并调用
         *   - 选项3: 运行时动态调整（需要重新创建RenderTargetManager）
         *
         * @note 内存优化示例（1920x1080分辨率）：
         *   - 4个colortex:  节省 ~99.4MB (75%)
         *   - 8个colortex:  节省 ~66.3MB (50%)
         *   - 12个colortex: 节省 ~33.2MB (25%)
         *
         * @warning 如果在RenderTargetManager已创建后调用，需要重新创建RenderTargetManager
         */
        void ConfigureGBuffer(int colorTexCount);

        /**
         * @brief 获取当前配置的GBuffer colortex数量
         * @return int 当前数量 [1-16]
         *
         * Milestone 3.0: 从m_configuration读取配置值（不再使用m_config）
         */
        int GetGBufferColorTexCount() const { return m_configuration.gbufferColorTexCount; }

        /**
         * @brief 获取RenderTargetManager实例
         * @return RenderTargetManager指针，如果未初始化返回nullptr
         *
         * Milestone 3.0任务4: 提供对RenderTargetManager的访问
         * - 用于手动控制RT翻转状态
         * - 用于查询Bindless索引
         * - 用于生成Mipmap等高级操作
         */
        RenderTargetManager* GetRenderTargetManager() const noexcept
        {
            return m_renderTargetManager.get();
        }

        // ==================== 配置和状态查询 ====================

        /**
         * @brief 获取当前配置
         * @return 当前的配置参数
         *
         * Milestone 3.0: 返回类型从Configuration改为RendererSubsystemConfig
         */
        const RendererSubsystemConfig& GetConfiguration() const noexcept { return m_configuration; }

        /**
         * @brief Get current loaded ShaderPack
         * @return Pointer to ShaderPack, or nullptr if not loaded
         *
         * Usage Example:
         * @code
         * const ShaderPack* pack = g_theRenderer->GetShaderPack();
         * if (pack && pack->IsValid()) {
         *     const ShaderProgram* program = pack->GetProgram(ProgramId::GBUFFERS_TERRAIN);
         * }
         * @endcode
         *
         * Teaching Note:
         * - Returns const pointer (read-only access, prevents accidental modification)
         * - Check for nullptr before use (defensive programming)
         * - Iris equivalent: IrisRenderingPipeline::getShaderPack()
         */
        const ShaderPack* GetShaderPack() const noexcept { return m_userShaderPack ? m_userShaderPack.get() : m_defaultShaderPack.get(); }

        /**
         * @brief Get user ShaderPack (may be nullptr if not loaded)
         *
         * Returns the user-loaded ShaderPack without fallback to engine default.
         * Use this when you need to check if a user ShaderPack is currently loaded.
         *
         * @return Pointer to user ShaderPack, or nullptr if not loaded
         *
         * @note Dual ShaderPack Architecture:
         * - GetShaderPack() returns user or default (never nullptr)
         * - GetUserShaderPack() returns only user pack (may be nullptr)
         * - GetDefaultShaderPack() returns engine default (never nullptr after Startup)
         *
         * Teaching Note:
         * - Used in unit tests to verify user ShaderPack lifecycle
         * - Enables precise testing of LoadShaderPack()/UnloadShaderPack() behavior
         * - Does not trigger fallback mechanism (unlike GetShaderPack())
         */
        const ShaderPack* GetUserShaderPack() const noexcept { return m_userShaderPack.get(); }

        /**
         * @brief Get engine default ShaderPack (always valid after Startup)
         *
         * Returns the engine default ShaderPack which is loaded during Startup() and
         * remains loaded throughout the application lifetime.
         *
         * @return Pointer to engine default ShaderPack (never nullptr after Startup)
         *
         * @note Dual ShaderPack Architecture:
         * - Loaded once in Startup() from ENGINE_DEFAULT_SHADERPACK_PATH
         * - Never unloaded (persistent for entire application lifetime)
         * - Used as fallback when user ShaderPack is not loaded
         *
         * Teaching Note:
         * - Guarantees shader system always has valid shaders (defensive programming)
         * - Prevents crashes from missing user shader packs
         * - Follows Null Object pattern (always provide valid default)
         */
        const ShaderPack* GetDefaultShaderPack() const noexcept { return m_defaultShaderPack.get(); }

        /**
         * @brief Shortcut to get ShaderSource by ProgramId
         * @param id Program identifier (e.g., ProgramId::GBUFFERS_TERRAIN)
         * @return Pointer to ShaderSource, or nullptr if pack not loaded or program not found
         *
         * Usage Example:
         * @code
         * const ShaderSource* terrain = g_theRenderer->GetShaderSource(ProgramId::GBUFFERS_TERRAIN);
         * if (terrain && terrain->IsValid()) {
         *     // Compile to ShaderProgram (Phase 5.4+)
         *     // Use shader source
         * }
         * @endcode
         *
         * Teaching Note:
         * - Returns shader source code (not compiled program yet)
         * - Phase 5.1-5.3: Only ShaderPack lifecycle implemented
         * - Phase 5.4+: Add compilation system (ShaderSource → ShaderProgram)
         * - Convenience wrapper to avoid repeated null checks
         * - Follows Facade pattern (simplifies common operations)
         *
         * Architecture:
         * - ShaderPack::GetProgramSet() → ProgramSet*
         * - ProgramSet::GetRaw(id) → ShaderSource*
         *
         * Iris equivalent: NewWorldRenderingPipeline::getShaderProgram()
         */
        const ShaderSource* GetShaderSource(ProgramId id) const noexcept;

        /**
         * @brief Get shader program with dual ShaderPack fallback mechanism (Phase 5.2)
         * @param id Program identifier (e.g., ProgramId::GBUFFERS_TERRAIN)
         * @param dimension Dimension name (default: "world0", supports "world-1", "world1", etc.)
         * @return Pointer to ShaderSource, or nullptr if not found in both packs
         *
         * Priority: User ShaderPack → Engine Default ShaderPack → nullptr (3-step fallback)
         *
         * Usage Example:
         * @code
         * const ShaderSource* terrain = g_theRenderer->GetShaderProgram(ProgramId::GBUFFERS_TERRAIN);
         * if (terrain && terrain->IsValid()) {
         *     // Use shader (will automatically fallback to engine default if user pack missing)
         * }
         * @endcode
         *
         * Teaching Note:
         * - Implements in-memory fallback (KISS principle, no file copying)
         * - User ShaderPack can partially override engine defaults
         * - Engine default ShaderPack always loaded (guaranteed fallback)
         * - Iris-compatible architecture (NewWorldRenderingPipeline behavior)
         *
         * Architecture (Milestone 3.0 Phase 5.2 - 2025-10-19):
         * - m_userShaderPack (Priority 1): User-downloaded shader packs
         * - m_defaultShaderPack (Priority 2): Engine built-in shaders
         * - Returns first valid ShaderSource found in priority order
         */
        const ShaderSource* GetShaderProgram(ProgramId id, const std::string& dimension = "world0") const noexcept;

        /**
         * @brief 获取渲染统计信息
         * @return 当前帧的渲染统计
         * @details 用于性能分析和调试
         */
        const RenderStatistics& GetRenderStatistics() const noexcept
        {
            return m_renderStatistics;
        }

        /**
         * @brief 检查子系统是否已准备好渲染
         * @return true表示可以进行渲染
         */
        bool IsReadyForRendering() const noexcept;

        // ==================== Shader Pack管理接口 ====================

        /**
         * @brief 加载Shader Pack
         * @param packPath Shader Pack文件路径
         * @return true表示加载成功
         * @details 
         * 支持运行时Shader Pack切换，用于：
         * - 不同视觉风格的切换
         * - Shader开发和调试
         * - 性能测试
         */
        bool LoadShaderPack(const std::string& packPath);

        /**
         * @brief 卸载当前Shader Pack
         * @details 回退到默认渲染管线
         */
        void UnloadShaderPack();

        /**
         * @brief 重新加载当前Shader Pack
         * @return true表示重新加载成功
         * @details 用于Shader开发期间的热重载
         */
        bool ReloadShaderPack();

        /**
         * @brief 启用/禁用Shader Pack热重载
         * @param enable true表示启用文件监控
         * @details 开发模式下自动检测Shader文件变化并重新加载
         */
        void EnableShaderHotReload(bool enable);

        // ==================== 调试和开发支持 ====================

        /**
         * @brief 获取DirectX 12设备
         * @return D3D12设备指针
         * @details 委托给D3D12RenderSystem的全局设备访问
         * @warning 直接操作设备可能导致状态不一致，建议通过高级接口操作
         */
        ID3D12Device* GetD3D12Device() const noexcept
        {
            return D3D12RenderSystem::GetDevice();
        }

        /**
         * @brief 获取主命令队列
         * @return 命令队列指针
         * @details 委托给D3D12RenderSystem获取CommandListManager的图形队列
         */
        ID3D12CommandQueue* GetCommandQueue() const noexcept;

    private:
        // ==================== ShaderPack Fixed Paths (Internal Constants) ====================
        /**
         * @brief Internal fixed paths for ShaderPack system
         *
         * These are compile-time constants defining the file system structure.
         * NOT intended to be user-configurable (use Configuration::currentShaderPackName instead).
         *
         * Rationale:
         * - Separation of Concerns: Fixed paths vs user choices
         * - KISS Principle: 99% of users never need to change these paths
         * - Iris Compatibility: Maintains standard directory structure
         */

        /**
         * @brief Engine default ShaderPack path (Fallback)
         *
         * This path points to the engine's built-in default shaders.
         * Used as a fallback when no user pack is selected.
         *
         * Architecture Decision (Plan 1):
         * - Engine assets directory is treated as a standard ShaderPack
         * - Directory structure: .enigma/assets/engine/shaders/ (Iris-compatible)
         * - Contains: shaders/core/Common.hlsl + shaders/program/gbuffers_*.hlsl
         * - 100% Iris-compatible (shaders/program/ directory structure)
         *
         * Teaching Note:
         * - constexpr: Compile-time constant, zero runtime overhead
         * - Relative path: Points to ShaderPack root (LoadShaderPackInternal adds /shaders)
         * - Final path: ".enigma/assets/engine/shaders/" (after /shaders appended)
         * - Path will be copied from Engine/.enigma/ to Run/.enigma/ during build
         *
         * Bug Fix (2025-10-19):
         * - Changed from ".enigma/assets/engine/shaders" to ".enigma/assets/engine"
         * - Reason: LoadShaderPackInternal() always appends "/shaders" subdirectory
         * - Previous path caused: .enigma/assets/engine/shaders/shaders (duplicate)
         */
        static constexpr const char* ENGINE_DEFAULT_SHADERPACK_PATH =
            ".enigma/assets/engine";

        /**
         * @brief User ShaderPack search directory
         *
         * This is where users can place custom ShaderPacks.
         * Each subdirectory represents one ShaderPack (Iris standard).
         *
         * Example structure:
         * .enigma/shaderpacks/
         * ├── ComplementaryReimagined/
         * │   └── shaders/
         * │       ├── program/
         * │       └── shaders.properties
         * ├── BSL/
         * │   └── shaders/
         * └── Sildurs/
         *     └── shaders/
         *
         * Teaching Note:
         * - Users can download ShaderPacks from Modrinth/CurseForge
         * - Each pack is self-contained in its own directory
         * - Compatible with Minecraft Iris/Optifine ShaderPacks
         */
        static constexpr const char* USER_SHADERPACK_SEARCH_PATH =
            ".enigma/shaderpacks";

        /**
         * @brief Current ShaderPack name
         *
         * - Empty string: Use ENGINE_DEFAULT_SHADERPACK_PATH
         * - "PackName": Use USER_SHADERPACK_SEARCH_PATH/PackName
         *
         * Teaching Note:
         * - This can be loaded from a config file (e.g., GameConfig.xml)
         * - Allows users to switch ShaderPacks without recompiling
         * - Hot-reload supported via ReloadShaderPack() method
         */
        std::string m_currentPackName;
        // ==================================================================

        // ==================== 内部初始化方法 ====================

        /**
         * @brief ShaderPack path selection logic
         * @return Selected ShaderPack path (user pack or engine default)
         * @details
         * Priority 1: User selected pack (from m_currentPackName)
         * Priority 2: Engine default pack (fallback)
         *
         * Teaching Note:
         * - Implements fallback mechanism for ShaderPack loading
         * - Returns std::filesystem::path for robust path handling
         * - Logs warning if user pack not found
         */
        std::filesystem::path SelectShaderPackPath() const;

        // ==================== ShaderPack Lifecycle Management ====================

        /**
         * @brief Load ShaderPack from specified path (private overload)
         * @param packPath Path to ShaderPack directory
         * @return true if loaded successfully, false otherwise
         *
         * Teaching Note:
         * - Private overload for internal use (filesystem::path parameter)
         * - Public interface uses std::string parameter for user convenience
         * - Implements actual loading logic
         *
         * Implementation in RendererSubsystem.cpp
         */
        bool LoadShaderPackInternal(const std::filesystem::path& packPath);

        // =========================================================================

    private:
        // ==================== 子系统特定对象 (非DirectX核心对象) ====================

        /// 交换链 - 双缓冲显示 (需要窗口句柄，由子系统管理)
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

        // ==================== 渲染系统组件 - 基于Iris架构 ====================

        // TODO: M2 - 添加新的灵活渲染架构成员变量

        /// Shader Pack管理器 - 着色器包系统 (已删除 - Milestone 3.0 重构)
        // std::unique_ptr<ShaderPackManager> m_shaderPackManager;

        /// Dual ShaderPack Architecture (Milestone 3.0 Phase 5.2 - 2025-10-19)
        /// Priority 1: User ShaderPack (user-downloaded shader packs from Configuration::currentShaderPackName)
        /// Priority 2: Engine Default ShaderPack (fallback for missing programs, always required)
        /// Fallback Strategy: User → Engine Default → nullptr (3-step in-memory fallback, KISS principle)
        std::unique_ptr<ShaderPack> m_userShaderPack; // User ShaderPack (may be null)
        std::unique_ptr<ShaderPack> m_defaultShaderPack; // Engine default ShaderPack (always valid)

        // ==================== Immediate模式渲染组件 ====================

        /// 渲染指令队列 - immediate模式核心
        std::unique_ptr<RenderCommandQueue> m_renderCommandQueue;

        // ==================== GBuffer管理组件 (Milestone 3.0任务4) ====================

        /// RenderTarget管理器 - 管理16个colortex RenderTarget (Iris兼容)
        std::unique_ptr<RenderTargetManager> m_renderTargetManager;

        // ==================== 配置和状态 ====================

        /// RendererSubsystem配置（Milestone 3.0: 配置系统重构完成）
        /// - 合并了旧的内嵌Configuration结构体
        /// - 从renderer.yml加载，包含所有14个配置参数
        /// - 遵循"一个子系统一个配置文件"原则
        RendererSubsystemConfig m_configuration;

        /// 渲染统计信息
        mutable RenderStatistics m_renderStatistics;

        /// 初始化状态标志
        bool m_isInitialized = false;
        bool m_isStarted     = false;
        bool m_isShutdown    = false;

        /// 调试状态 (子系统级别的调试，底层调试由D3D12RenderSystem管理)
        bool m_debugRenderingEnabled = false;
    };
} // namespace enigma::graphic
