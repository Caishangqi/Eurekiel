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
#include "../Shader/ShaderCache.hpp"
#include "RendererSubsystemConfig.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp"
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
    class DepthTextureManager;
    class PSOManager;

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

            /**
             * @brief 重置统计信息
             */
            void Reset();
        };

    public:
#pragma region Lifecycle Management
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
#pragma endregion

#pragma region ShaderPack Management
        /**
         * @brief 获取渲染指令队列
         * @return RenderCommandQueue指针，如果未初始化返回nullptr
         * @details 
         * 提供对immediate模式渲染指令队列的访问
         * 用户可以通过此接口提交自定义渲染指令
         */
        RenderCommandQueue* GetRenderCommandQueue() const noexcept;

        /**
         * @brief 通过ProgramId获取ShaderProgram（新增重载）
         * @param id 程序ID
         * @return std::shared_ptr<ShaderProgram> ShaderProgram智能指针（未找到返回nullptr）
         *
         * 教学要点:
         * - 从ShaderCache获取ShaderProgram
         * - 支持ProgramId查找
         * - 向后兼容：保持现有API不变
         */
        std::shared_ptr<ShaderProgram> GetShaderProgram(ProgramId id);


        const ShaderSource* GetShaderProgram(ProgramId id, const std::string& dimension) const noexcept;
        /**
         * @brief Load a ShaderPack from the specified path
         * @param packPath Path to the ShaderPack directory
         * @return true if loaded successfully, false otherwise
         *
         * Teaching Note:
         * - Based on Iris.java::loadExternalShaderpack() implementation
         * - Validates ShaderPack structure before loading
         * - Unloads previous ShaderPack if any
         * - Triggers pipeline rebuild after successful load
         */
        bool LoadShaderPack(const std::string& packPath);

        /**
         * @brief Unload the currently loaded ShaderPack
         *
         * Teaching Note:
         * - Based on Iris.java::resetShaderPack() implementation
         * - Safely releases ShaderPack resources
         * - Resets to engine default state
         * - Triggers pipeline rebuild
         */
        void UnloadShaderPack();
#pragma endregion

#pragma region Shader Compilation
        /**
         * @brief 从文件编译着色器程序（便捷API）
         * @param vsPath 顶点着色器文件路径
         * @param psPath 像素着色器文件路径
         * @param programName 程序名称（可选，默认从文件名提取）
         * @return 编译成功返回 ShaderProgram 智能指针，失败返回 nullptr
         * 
         * @details 使用标准流程：HLSL 文件 → ShaderSource → ShaderProgramBuilder → ShaderProgram
         *          自动解析 ProgramDirectives（DRAWBUFFERS、BLEND 等）
         * 
         * 功能：
         * - 自动读取 HLSL 文件
         * - 自动解析 ProgramDirectives（`// DRAWBUFFERS:0157` 等）
         * - 自动包含 Common.hlsl（引擎核心路径）
         * - 使用默认编译选项（优化开启，调试信息关闭）
         * - 自动推导入口点（VSMain, PSMain）
         * - 自动推导目标版本（vs_6_6, ps_6_6）
         * 
         * 编译流程：
         * 1. 读取 HLSL 文件内容
         * 2. 创建 ShaderSource 对象（自动解析 Directives）
         * 3. 创建 ShaderProgramBuilder
         * 4. 调用 DXC 编译器生成字节码
         * 5. 返回 ShaderProgram 对象
         * 
         * 错误处理：
         * - 文件不存在 → 返回 nullptr，记录错误日志
         * - 编译失败 → 返回 nullptr，记录编译错误
         * - Directives 解析失败 → 使用默认值，记录警告
         * 
         * 使用示例：
         * @code
         * // 基本用法（推荐）
         * auto program = g_theRendererSubsystem->CreateShaderProgramFromFiles(
         *     "shaders/gbuffers_basic.vs.hlsl",
         *     "shaders/gbuffers_basic.ps.hlsl"
         * );
         * if (program) {
         *     // 使用 program 进行渲染
         *     g_theRendererSubsystem->UseProgram(program);
         * }
         * 
         * // 指定程序名称
         * auto program2 = g_theRendererSubsystem->CreateShaderProgramFromFiles(
         *     "shaders/test.vert.hlsl",
         *     "shaders/test.frag.hlsl",
         *     "TestProgram"
         * );
         * @endcode
         * 
         * 教学要点：
         * - 理解从文件到 ShaderProgram 的完整流程
         * - 学习 ProgramDirectives 的自动解析
         * - 掌握错误处理的最佳实践
         * 
         * @note 着色器文件中的注释指令必须放在文件顶部
         * @see CreateShaderProgramFromSource, ShaderSource, ShaderProgramBuilder
         */
        std::shared_ptr<ShaderProgram> CreateShaderProgramFromFiles(
            const std::filesystem::path& vsPath,
            const std::filesystem::path& psPath,
            const std::string&           programName = ""
        );

        /**
         * @brief 从文件编译着色器程序（高级API，支持自定义编译选项）
         * @param vsPath 顶点着色器文件路径
         * @param psPath 像素着色器文件路径
         * @param programName 程序名称
         * @param options 编译选项（优化级别、调试信息、宏定义等）
         * @return 编译成功返回 ShaderProgram 智能指针，失败返回 nullptr
         * 
         * @details 高级版本，支持完全自定义编译选项
         * 
         * 功能：
         * - 支持自定义优化级别（O0/O1/O2/O3）
         * - 支持调试信息开关（PDB 生成）
         * - 支持自定义宏定义（如 ENABLE_SHADOWS=1）
         * - 支持自定义入口点（非标准 VSMain/PSMain）
         * - 支持自定义目标版本（vs_6_0 到 vs_6_6）
         * - 支持自定义 Include 路径
         * 
         * 使用示例：
         * @code
         * // 调试模式编译（带调试信息，无优化）
         * ShaderCompileOptions debugOptions = ShaderCompileOptions::Debug();
         * auto program = g_theRendererSubsystem->CreateShaderProgramFromFiles(
         *     "shaders/test.vs.hlsl",
         *     "shaders/test.ps.hlsl",
         *     "TestProgram",
         *     debugOptions
         * );
         * 
         * // 自定义宏定义
         * ShaderCompileOptions customOptions = ShaderCompileOptions::Default();
         * customOptions.defines["ENABLE_SHADOWS"] = "1";
         * customOptions.defines["MAX_LIGHTS"] = "8";
         * auto program2 = g_theRendererSubsystem->CreateShaderProgramFromFiles(
         *     "shaders/lighting.vs.hlsl",
         *     "shaders/lighting.ps.hlsl",
         *     "LightingProgram",
         *     customOptions
         * );
         * @endcode
         * 
         * 教学要点：
         * - 理解编译选项对性能和调试的影响
         * - 学习宏定义在着色器中的应用
         * - 掌握调试和发布版本的编译策略
         * 
         * @see ShaderCompileOptions, CreateShaderProgramFromFiles
         */
        std::shared_ptr<ShaderProgram> CreateShaderProgramFromFiles(
            const std::filesystem::path& vsPath,
            const std::filesystem::path& psPath,
            const std::string&           programName,
            const ShaderCompileOptions&  options
        );
        const ShaderSource* GetShaderSource(ProgramId id) const noexcept;

        /**
         * @brief 从源码字符串编译着色器程序（核心实现）
         * @param vsSource 顶点着色器源码字符串
         * @param psSource 像素着色器源码字符串
         * @param programName 程序名称
         * @param options 编译选项（可选，默认使用 Default 配置）
         * @return 编译成功返回 ShaderProgram 智能指针，失败返回 nullptr
         * 
         * @details 使用标准流程：ShaderSource → ShaderProgramBuilder → ShaderProgram
         *          自动解析 ProgramDirectives（DRAWBUFFERS、BLEND 等）
         * 
         * 功能：
         * - 直接从内存中的源码字符串编译
         * - 自动解析 ProgramDirectives（`// DRAWBUFFERS:0157` 等）
         * - 支持自定义编译选项
         * - 自动推导入口点和目标版本
         * 
         * 使用示例：
         * @code
         * // 从字符串编译（适合动态生成的着色器）
         * std::string vsSource = R"(
         *     // DRAWBUFFERS:01
         *     struct VSInput { float3 pos : POSITION; };
         *     struct VSOutput { float4 pos : SV_Position; };
         *     VSOutput VSMain(VSInput input) {
         *         VSOutput output;
         *         output.pos = float4(input.pos, 1.0);
         *         return output;
         *     }
         * )";
         * 
         * std::string psSource = R"(
         *     struct PSOutput { float4 color : SV_Target0; };
         *     PSOutput PSMain() {
         *         PSOutput output;
         *         output.color = float4(1, 0, 0, 1);
         *         return output;
         *     }
         * )";
         * 
         * auto program = g_theRendererSubsystem->CreateShaderProgramFromSource(
         *     vsSource,
         *     psSource,
         *     "DynamicProgram"
         * );
         * @endcode
         * 
         * 教学要点：
         * - 理解从源码到字节码的编译流程
         * - 学习 ProgramDirectives 的解析机制
         * - 掌握动态着色器生成的技巧
         * 
         * @see CreateShaderProgramFromFiles, ShaderSource, ShaderProgramBuilder
         */
        std::shared_ptr<ShaderProgram> CreateShaderProgramFromSource(
            const std::string&          vsSource,
            const std::string&          psSource,
            const std::string&          programName,
            const ShaderCompileOptions& options = ShaderCompileOptions::Default()
        );
#pragma endregion

#pragma region Rendering API
        /**
         * @brief 使用ShaderProgram并绑定RenderTarget（Mode A + Mode B）
         * @param shaderProgram Shader程序指针
         * @param rtOutputs RT输出索引列表（Mode A），为空时从DRAWBUFFERS读取
         *
         * @details
         * M6.2核心API - 支持两种RT绑定模式：
         * - Mode A: 参数指定RT输出 - UseProgram(program, {0,1,2,3})
         * - Mode B: 显式绑定 - BindRenderTargets() + UseProgram(program)
         *
         * 实现逻辑：
         * 1. 确定RT输出：优先使用rtOutputs参数，否则读取DRAWBUFFERS指令
         * 2. 如果drawBuffers非空，调用RenderTargetBinder绑定RT
         * 3. 调用ShaderProgram.Use()设置PSO和Root Signature
         * 4. 调用RenderTargetBinder.FlushBindings()刷新RT绑定
         *
         * @code
         * // Mode A示例：参数指定RT输出（适合简单场景）
         * renderer->UseProgram(gbuffersProgram, {0, 1, 2, 3}); // 绑定colortex0-3
         * renderer->DrawScene();
         *
         * // Mode B示例：显式绑定（适合复杂场景）
         * renderer->BindRenderTargets({RTType::ColorTex, RTType::ColorTex}, {0, 1});
         * renderer->UseProgram(gbuffersProgram); // 自动从DRAWBUFFERS读取
         * renderer->DrawScene();
         *
         * // Flip示例：历史帧访问（TAA、Motion Blur）
         * renderer->UseProgram(compositeProgram, {0}); // 写入colortex0
         * renderer->DrawFullscreenQuad();
         * renderer->FlipRenderTarget(0); // 翻转colortex0，下一帧读取历史数据
         * @endcode
         *
         * 教学要点：
         * - 理解DirectX 12的PSO（Pipeline State Object）是不可变的
         * - 学习动态OMSetRenderTargets()的标准做法
         * - 掌握Iris DRAWBUFFERS指令的解析和应用
         */
        void UseProgram(std::shared_ptr<ShaderProgram> shaderProgram, const std::vector<uint32_t>& rtOutputs = {});

        /**
         * @brief 使用ShaderProgram（ProgramId重载版本）
         * @param programId Shader程序ID（从ShaderPack获取）
         * @param rtOutputs RT输出索引列表（Mode A），为空时从DRAWBUFFERS读取
         * 
         * @details
         * 便捷重载版本，通过ShaderPackManager自动获取ShaderProgram指针
         * 内部调用UseProgram(ShaderProgram*, rtOutputs)
         */
        void UseProgram(ProgramId programId, const std::vector<uint32_t>& rtOutputs = {});

        /**
         * @brief 翻转指定RenderTarget的Main/Alt状态
         * @param rtIndex RenderTarget索引 [0, activeColorTexCount)
         *
         * @details
         * M6.2.3 - RenderTarget翻转API
         *
         * 业务逻辑：
         * - 当前帧：读Main写Alt → Flip() → 下一帧：读Alt写Main
         * - 实现历史帧数据访问（TAA、Motion Blur等技术需要）
         *
         * 实现：
         * - 直接调用RenderTargetBinder->GetRenderTargetManager()->FlipRenderTarget(rtIndex)
         * - 边界检查由RenderTargetManager内部处理
         *
         * 教学要点：
         * - 理解双缓冲Ping-Pong机制
         * - 学习历史帧数据在时序算法中的应用
         */
        void FlipRenderTarget(int rtIndex);

        /**
         * @brief 开始Camera渲染 - EnigmaCamera重载版本
         * @param camera 要使用的EnigmaCamera
         *
         * 教学要点：
         * - 从EnigmaCamera读取数据并设置到UniformManager
         * - 数据流：EnigmaCamera → RendererSubsystem::BeginCamera → UniformManager → GPU
         * - 调用UploadDirtyBuffersToGPU()确保数据上传到GPU
         * - 设置视口等渲染状态
         */
        void BeginCamera(const class EnigmaCamera& camera);

        /**
         * @brief 结束Camera渲染
         *
         * 教学要点：
         * - 清理Camera状态
         * - 对应Iris的endCamera()
         */
        void EndCamera(const EnigmaCamera& camera);

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

        // ========================================================================
        // 即时顶点绘制API - 双模式支持
        // ========================================================================

        /**
         * @brief 绘制顶点数组（即时数据模式 - vector版本）
         * @param vertices 顶点数组
         *
         * 教学要点：
         * - 即时数据模式：从CPU内存直接绘制
         * - 自动管理：内部使用临时缓冲区（m_immediateVBO）
         * - 性能权衡：适合少量、动态变化的几何体
         * - 参考DX11Renderer::DrawVertexArray实现
         */
        void DrawVertexArray(const std::vector<Vertex>& vertices);

        /**
         * @brief 绘制顶点数组（即时数据模式 - 指针版本）
         * @param vertices 顶点指针
         * @param count 顶点数量
         *
         * 教学要点：
         * - 底层实现：vector版本调用此方法
         * - 数据上传：Map缓冲区 → 复制数据 → Unmap
         * - 缓冲区管理：自动扩展（2倍增长策略）
         */
        void DrawVertexArray(const Vertex* vertices, size_t count);

        /**
         * @brief 绘制索引顶点数组（即时数据模式 - vector版本）
         * @param vertices 顶点数组
         * @param indices 索引数组
         *
         * 教学要点：
         * - 索引绘制：减少顶点重复，节省内存
         * - 双缓冲区：同时管理VBO和IBO
         * - 适用场景：网格、UI、调试几何体
         */
        void DrawVertexArray(const std::vector<Vertex>& vertices, const std::vector<unsigned>& indices);

        /**
         * @brief 绘制索引顶点数组（即时数据模式 - 指针版本）
         * @param vertices 顶点指针
         * @param vertexCount 顶点数量
         * @param indices 索引指针
         * @param indexCount 索引数量
         *
         * 教学要点：
         * - 完整实现：vector版本调用此方法
         * - 数据验证：检查指针有效性和数量
         * - 错误处理：使用ERROR_RECOVERABLE记录错误
         */
        void DrawVertexArray(const Vertex* vertices, size_t vertexCount, const unsigned* indices, size_t indexCount);

        /**
         * @brief 绘制顶点缓冲区（缓冲区句柄模式 - 非索引）
         * @param vbo 顶点缓冲区智能指针
         * @param vertexCount 顶点数量
         *
         * 教学要点：
         * - 缓冲区句柄模式：使用用户管理的缓冲区
         * - 生命周期：用户负责缓冲区创建和销毁
         * - 性能优势：避免每帧上传数据
         * - 适用场景：静态或低频更新的几何体
         */
        void DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, size_t vertexCount);

        /**
         * @brief 绘制顶点缓冲区（缓冲区句柄模式 - 索引）
         * @param vbo 顶点缓冲区智能指针
         * @param ibo 索引缓冲区智能指针
         * @param indexCount 索引数量
         *
         * 教学要点：
         * - 完整缓冲区绘制：VBO + IBO
         * - 智能指针：自动管理资源生命周期
         * - 参考DX11Renderer::DrawVertexIndexed实现
         */
        void DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, const std::shared_ptr<D12IndexBuffer>& ibo, size_t indexCount);

        /**
         * @brief 绘制全屏四边形
         *
         * 教学要点：
         * - 便捷方法用于后处理
         * - 自动管理顶点数据
         */
        void DrawFullscreenQuad();

        /**
         * @brief 使用Shader处理后输出到BackBuffer（M6.3核心API）
         * @param finalProgram final阶段Shader（如色调映射、后处理）
         * @param inputRTs 输入RT索引列表（保留参数，Bindless架构下自动访问）
         * 
         * @details
         * **Iris对应关系**：
         * - 对应Iris的final.fsh语义
         * - 执行最终屏幕输出前的后处理（色调映射、Bloom、DOF等）
         * - 输出到gl_FragColor（等价于我们的BackBuffer）
         * 
         * **常见使用场景**：
         * 1. 色调映射（Tone Mapping）：HDR→LDR转换
         * 2. 后处理效果：Bloom、DOF、Motion Blur、Vignette
         * 3. 颜色校正：Gamma校正、色彩分级
         * 4. 抗锯齿：FXAA、SMAA后处理
         * 
         * **输入（读取colortex）**：
         * - Bindless架构：colortex索引已在ColorTargetsIndexBuffer中
         * - Shader自动访问：allTextures[colorTargets.readIndices[0]]
         * - 无需手动绑定SRV（与传统架构不同）
         * 
         * **输出（写入BackBuffer）**：
         * - OMSetRenderTargets(backBufferRTV) 指定输出目标
         * - Shader的SV_Target输出到BackBuffer（不是colortex）
         * 
         * **执行流程**：
         * 1. 绑定BackBuffer为RTV（输出目标）
         * 2. UseProgram(finalProgram)设置PSO
         * 3. DrawFullscreenQuad()绘制
         * 
         * @code
         * // 基础用法：Iris final.fsh等价实现
         * auto finalProgram = GetShaderProgram(ProgramId::FINAL);
         * renderer->PresentWithShader(finalProgram); // Shader自动读取colortex0-15
         * 
         * // 完整示例：色调映射 + Gamma校正
         * renderer->BeginFrame();
         * // ... 渲染场景到colortex0 ...
         * auto finalProgram = GetShaderProgram(ProgramId::FINAL);
         * renderer->PresentWithShader(finalProgram); // 后处理并输出到屏幕
         * renderer->EndFrame(); // Present到屏幕
         * @endcode
         * 
         * **教学要点**：
         * - 理解Bindless架构的自动纹理访问机制
         * - 学习DirectX 12的OMSetRenderTargets()动态绑定
         * - 掌握全屏后处理的标准流程
         * - 理解Iris final.fsh在延迟渲染管线中的作用
         * 
         * **注意事项**：
         * - BackBuffer格式固定为DXGI_FORMAT_R8G8B8A8_UNORM
         * - finalProgram必须输出到SV_Target0
         * - 确保在EndFrame()之前调用（Present之前）
         * 
         * @note inputRTs参数保留用于未来的验证或优化，当前Bindless架构下无需显式绑定
         */
        void PresentWithShader(std::shared_ptr<ShaderProgram> finalProgram,
                               const std::vector<uint32_t>&   inputRTs = {0});

        /**
         * @brief 直接拷贝RT到BackBuffer（M6.3便捷API）
         * @param rtIndex RT索引 [0, activeColorTexCount)
         * @param rtType RT类型（默认ColorTex，可选ShadowColor/DepthTex）
         * 
         * @details
         * **使用场景**：
         * 1. 快速调试：直接查看任意RT内容（GBuffer、Shadow、Depth等）
         * 2. 简单输出：无需后处理的场景（如UI渲染结果）
         * 3. 性能测试：跳过Shader开销，测试纯拷贝性能
         * 
         * **实现方式**：
         * - 使用ID3D12GraphicsCommandList::CopyResource()执行GPU拷贝
         * - 比PresentWithShader()更快（无Shader开销）
         * - 但无法进行后处理（色调映射、Gamma校正等）
         * 
         * **执行流程**：
         * 1. 参数验证（rtIndex范围检查）
         * 2. 资源状态转换（RT→COPY_SOURCE, BackBuffer→COPY_DEST）
         * 3. CopyResource()执行GPU拷贝
         * 4. 恢复资源状态
         * 
         * @code
         * // 基础用法：输出colortex0到屏幕
         * renderer->PresentRenderTarget(0); // 最简单的输出方式
         * 
         * // 调试用法：查看GBuffer内容
         * renderer->PresentRenderTarget(1); // 查看colortex1（如法线）
         * renderer->PresentRenderTarget(2); // 查看colortex2（如深度）
         * 
         * // 输出Shadow Map
         * renderer->PresentRenderTarget(0, RTType::ShadowColor);
         * @endcode
         * 
         * **教学要点**：
         * - 理解DirectX 12的CopyResource()操作
         * - 学习资源状态转换（Resource Barrier）的重要性
         * - 掌握调试渲染管线的技巧
         * 
         * **注意事项**：
         * - RT和BackBuffer必须格式兼容（都是R8G8B8A8_UNORM）
         * - 分辨率必须一致，否则拷贝失败
         * - 不执行任何后处理（直接拷贝原始数据）
         * 
         * **性能对比**：
         * - PresentRenderTarget(): ~0.1ms（纯拷贝）
         * - PresentWithShader(): ~0.5-2ms（含Shader执行）
         */
        void PresentRenderTarget(int rtIndex, RTType rtType = RTType::ColorTex);

        /**
         * @brief 拷贝自定义纹理到BackBuffer（M6.3高级API）
         * @param texture 任意D12Texture（必须非空）
         * 
         * @details
         * **使用场景**：
         * 1. 输出非RT纹理：加载的PNG/JPG图片、视频帧
         * 2. 截图功能：输出保存的截图纹理到屏幕
         * 3. 高级自定义输出：动态生成的纹理、计算着色器结果
         * 4. 纹理预览：在编辑器中预览任意纹理
         * 
         * **与其他API的区别**：
         * - PresentRenderTarget(): 输入来自RenderTargetManager（GBuffer）
         * - PresentCustomTexture(): 输入来自任意D12Texture（最灵活）
         * - PresentWithShader(): 使用Shader处理（支持后处理）
         * 
         * **实现方式**：
         * - 使用ID3D12GraphicsCommandList::CopyResource()执行GPU拷贝
         * - 资源状态假设：texture初始状态为PIXEL_SHADER_RESOURCE
         * - 拷贝后恢复原始状态，不影响后续使用
         * 
         * **执行流程**：
         * 1. 参数验证（texture非空检查）
         * 2. 资源状态转换（texture→COPY_SOURCE, BackBuffer→COPY_DEST）
         * 3. CopyResource()执行GPU拷贝
         * 4. 恢复资源状态
         * 
         * @code
         * // 基础用法：输出加载的图片
         * auto splashScreen = LoadTexture("splash.png");
         * renderer->PresentCustomTexture(splashScreen);
         * 
         * // 截图功能：输出保存的截图
         * auto screenshot = CaptureScreenshot();
         * renderer->PresentCustomTexture(screenshot); // 显示截图预览
         * 
         * // 视频播放：输出视频帧
         * auto videoFrame = videoPlayer->GetCurrentFrame();
         * renderer->PresentCustomTexture(videoFrame);
         * 
         * // 计算着色器结果：输出UAV纹理
         * auto computeResult = RunComputeShader();
         * renderer->PresentCustomTexture(computeResult);
         * @endcode
         * 
         * **教学要点**：
         * - 理解DirectX 12的资源状态管理
         * - 学习灵活的纹理输出架构设计
         * - 掌握CopyResource()的正确使用
         * 
         * **注意事项**：
         * - texture必须与BackBuffer格式兼容（R8G8B8A8_UNORM）
         * - 分辨率必须一致（1920x1080等）
         * - texture初始状态假设为PIXEL_SHADER_RESOURCE
         * - 不执行任何后处理（直接拷贝）
         * 
         * **典型应用**：
         * - 游戏启动画面（Splash Screen）
         * - 截图预览系统
         * - 视频播放器
         * - 纹理编辑器预览
         */
        void PresentCustomTexture(std::shared_ptr<D12Texture> texture);

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
#pragma endregion

#pragma region RenderTarget Management

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

        // ==================== Milestone 4: 深度缓冲管理 ====================

        /**
         * @brief 切换活动深度缓冲（depthtex0 ↔ depthtex1 ↔ depthtex2）
         * @param newActiveIndex 新的活动深度缓冲索引 [0-2]
         *
         * **Milestone 4 新增功能**:
         * - 支持动态切换当前使用的深度纹理
         * - 更新DSV绑定到新的深度纹理
         * - 保持Iris语义不变（depthtex0/1/2）
         *
         * **使用场景**:
         * - 多阶段渲染时切换不同的深度缓冲
         * - 支持灵活的深度纹理管理策略
         *
         * 教学要点:
         * - 理解活动深度缓冲的概念
         * - 掌握DirectX 12的DSV切换机制
         * - 委托DepthTextureManager执行实际操作
         */
        void SwitchDepthBuffer(int newActiveIndex);

        /**
         * @brief 复制深度纹理（通用接口）
         * @param srcIndex 源深度纹理索引 [0-2]
         * @param dstIndex 目标深度纹理索引 [0-2]
         *
         * **Milestone 4 新增功能**:
         * - 提供公共的深度复制接口
         * - 支持任意深度纹理之间的复制
         * - 自动处理资源状态转换
         *
         * **使用场景**:
         * - TERRAIN_TRANSLUCENT前：CopyDepthBuffer(0, 1) // depthtex0 → depthtex1
         * - HAND_SOLID前：CopyDepthBuffer(0, 2)          // depthtex0 → depthtex2
         * - 自定义复制：CopyDepthBuffer(1, 2)            // depthtex1 → depthtex2
         *
         * **业务逻辑**:
         * 1. 参数验证（范围[0-2]，不能相同）
         * 2. 转换资源状态：DEPTH_WRITE → COPY_SOURCE/DEST
         * 3. 执行CopyResource()
         * 4. 恢复资源状态：COPY_SOURCE/DEST → DEPTH_WRITE
         *
         * 教学要点:
         * - 理解DirectX 12的资源状态转换
         * - 掌握ResourceBarrier的正确使用
         * - 委托DepthTextureManager执行实际操作
         */
        void CopyDepthBuffer(int srcIndex, int dstIndex);

        /**
         * @brief 查询当前激活的深度缓冲索引
         * @return 当前活动的深度缓冲索引 [0-2]
         *
         * **Milestone 4 新增功能**:
         * - 查询当前正在使用的深度纹理
         * - 支持状态查询和调试
         *
         * 教学要点:
         * - noexcept保证（简单getter）
         * - 状态查询API的设计
         */
        int GetActiveDepthBufferIndex() const noexcept;
#pragma endregion

#pragma region State Management

        /**
         * @brief 获取当前配置
         * @return 当前的配置参数
         *
         * Milestone 3.0: 返回类型从Configuration改为RendererSubsystemConfig
         */
        const RendererSubsystemConfig& GetConfiguration() const noexcept { return m_configuration; }

        /**
         * @brief 获取配置的shader入口点名称
         * @return const std::string& 入口点名称（如 "main" 或 "VSMain"）
         * 
         * 教学要点:
         * - 配置访问: 提供对配置项的便捷访问
         * - Iris兼容: 默认值为 "main"（Iris标准）
         * - 编译流程: 用于shader编译时指定入口点
         */
        const std::string& GetShaderEntryPoint() const noexcept { return m_configuration.shaderEntryPoint; }


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
#pragma endregion

    public:
        /**
         * @brief 重新加载当前Shader Pack
         * @return true表示重新加载成功
         * @details 用于Shader开发期间的热重载
         */
        bool ReloadShaderPack();

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

        // ============================================================
        // ImGui Integration Support (7 getter methods for IImGuiRenderContext)
        // ============================================================

        /**
         * @brief 获取当前命令列表
         * @return 当前活动的命令列表指针
         * @details 委托给D3D12RenderSystem::GetCurrentCommandList()
         */
        ID3D12GraphicsCommandList* GetCurrentCommandList() const noexcept;

        /**
         * @brief 获取SRV描述符堆
         * @return SRV描述符堆指针
         * @details 委托给GlobalDescriptorHeapManager::GetCbvSrvUavHeap()
         */
        ID3D12DescriptorHeap* GetSRVHeap() const noexcept;

        /**
         * @brief 获取RenderTarget格式
         * @return RTV格式
         * @details 返回SwapChain的后台缓冲区格式（固定为DXGI_FORMAT_R8G8B8A8_UNORM）
         */
        DXGI_FORMAT GetRTVFormat() const noexcept;

        /**
         * @brief 获取帧缓冲数量
         * @return 帧缓冲数量（双缓冲或三缓冲）
         * @details 返回SwapChain的缓冲区数量
         */
        uint32_t GetFramesInFlight() const noexcept;

        /**
         * @brief 检查是否已初始化
         * @return true表示已初始化
         * @details 检查m_isInitialized标志
         */
        bool IsInitialized() const noexcept;

        // ==================== 自定义材质纹理上传 API (Phase 3) ====================

        /**
         * @brief 上传自定义纹理到指定槽位（槽位范围：0-15）
         * @param slotIndex 材质槽位索引（必须 < 16）
         * @param texture 要上传的纹理（必须有有效的 Bindless 索引）
         *
         * **Phase 3 新增功能**:
         * - 支持应用层上传自定义材质纹理
         * - 更新 CustomImageIndexBuffer 的指定槽位
         * - 纹理在 HLSL 中通过 customImage0-15 访问
         *
         * **使用场景**:
         * - 自定义材质系统
         * - 动态纹理加载
         * - 用户自定义着色器效果
         *
         * **HLSL 访问示例**:
         * @code
         * Texture2D customImage0 : register(t0, space2);
         * float4 color = customImage0.Sample(sampler0, uv);
         * @endcode
         *
         * 教学要点:
         * - 理解 Bindless 纹理系统
         * - 掌握应用层与着色器的数据传递
         * - 委托 UniformManager 执行实际更新
         */
        void UploadCustomTexture(uint8_t slotIndex, const std::shared_ptr<class D12Texture>& texture);

        /**
         * @brief 批量上传多个自定义纹理
         * @param textures 槽位-纹理对的列表
         *
         * **Phase 3 新增功能**:
         * - 一次性上传多个自定义纹理
         * - 内部调用单个上传方法
         * - 减少重复的参数验证开销
         *
         * **使用示例**:
         * @code
         * std::vector<std::pair<uint8_t, std::shared_ptr<D12Texture>>> textures = {
         *     {0, myAlbedoTexture},
         *     {1, myNormalTexture},
         *     {2, myRoughnessTexture}
         * };
         * renderer->UploadCustomTextures(textures);
         * @endcode
         *
         * 教学要点:
         * - 批量操作的设计模式
         * - std::pair 的使用
         * - 便捷 API 的设计思路
         */
        void UploadCustomTextures(const std::vector<std::pair<uint8_t, std::shared_ptr<class D12Texture>>>& textures);

        /**
         * @brief 清空指定槽位的纹理
         * @param slotIndex 要清空的槽位索引
         *
         * **Phase 3 新增功能**:
         * - 将指定槽位重置为无效索引
         * - 用于释放不再使用的纹理槽位
         * - 内部使用 INVALID_INDEX (UINT32_MAX)
         *
         * **使用场景**:
         * - 材质卸载
         * - 槽位复用
         * - 内存优化
         *
         * 教学要点:
         * - 资源生命周期管理
         * - 无效索引的使用
         * - 防御性编程（参数验证）
         */
        void ClearCustomTextureSlot(uint8_t slotIndex);

        /**
         * @brief 重置所有自定义纹理槽位
         *
         * **Phase 3 新增功能**:
         * - 一次性清空所有16个槽位
         * - 用于场景切换或资源重载
         * - 委托 UniformManager 执行批量重置
         *
         * **使用场景**:
         * - 场景切换
         * - 材质系统重置
         * - 内存清理
         *
         * 教学要点:
         * - 批量操作的重要性
         * - 场景管理最佳实践
         * - RAII 思想的应用
         */
        void ResetCustomTextures();

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

        // ==================== ShaderPack Lifecycle Management ====================

        // 阶段2.3: SelectShaderPackPath, LoadShaderPackInternal已移至ShaderPackHelper
        // 阶段2.4: EnsureImmediateVBO和EnsureImmediateIBO已移至RendererHelper

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

        /// ShaderCache管理器 - 统一管理ShaderSource和ShaderProgram缓存 (Shrimp Task)
        /// 职责: 缓存ShaderSource（持久存储）和ShaderProgram（可清理），支持ProgramId和字符串双重索引
        std::unique_ptr<ShaderCache> m_shaderCache;

        // ==================== Immediate模式渲染组件 ====================

        /// 渲染指令队列 - immediate模式核心
        std::unique_ptr<RenderCommandQueue> m_renderCommandQueue;

        /// 即时顶点缓冲区 - 用于DrawVertexArray即时数据模式
        std::shared_ptr<D12VertexBuffer> m_immediateVBO;

        /// 即时索引缓冲区 - 用于DrawVertexArray索引绘制
        std::shared_ptr<D12IndexBuffer> m_immediateIBO;

        // ==================== GBuffer管理组件 (Milestone 3.0任务4) ====================

        /// RenderTarget管理器 - 管理16个colortex RenderTarget (Iris兼容)
        std::unique_ptr<RenderTargetManager> m_renderTargetManager;

        /// 深度纹理管理器 - 管理3个深度纹理 (Iris depthtex0/1/2)
        std::unique_ptr<DepthTextureManager> m_depthTextureManager;

        /// Shadow Color管理器 - 管理8个shadowcolor RenderTarget (Iris兼容)
        std::unique_ptr<class ShadowColorManager> m_shadowColorManager;

        /// Shadow Target管理器 - 管理2个shadowtex纹理 (Iris兼容)
        std::unique_ptr<class ShadowTargetManager> m_shadowTargetManager;

        /// RenderTarget绑定器 - 统一RT绑定接口 (组合4个Manager)
        std::unique_ptr<class RenderTargetBinder> m_renderTargetBinder;

        // ==================== 全屏绘制资源 (M6.3) ====================

        /// 全屏三角形VertexBuffer - 用于后处理Pass和Present操作
        std::unique_ptr<D12VertexBuffer> m_fullscreenTriangleVB;

        // ==================== Uniform管理组件 (Phase 3) ====================

        /// Uniform管理器 - 管理所有Uniform Buffer（包括CustomImageIndexBuffer）
        std::unique_ptr<class UniformManager> m_uniformManager;

        // ==================== PSO管理组件 ====================

        /// PSO管理器 - 动态创建和缓存PSO
        std::unique_ptr<PSOManager> m_psoManager;

        /// 当前混合模式
        BlendMode m_currentBlendMode = BlendMode::Opaque;

        /// 当前深度模式
        DepthMode m_currentDepthMode = DepthMode::Enabled;

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
