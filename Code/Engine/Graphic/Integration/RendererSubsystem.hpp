/**
 * @file RendererSubsystem.hpp
 * @brief Enigma引擎渲染子系统 - DirectX 12延迟渲染管线管理器
 * 
 * 教学重点:
 * 1. 理解新的灵活渲染架构（BeginFrame/Render/EndFrame + 60+ API）
 * 2. 学习DirectX 12的现代化资源管理
 * 3. 掌握引擎子系统的生命周期管理
 * 4. Understand shader bundle architecture and fallback mechanism
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <filesystem>

#include "FullQuadsRenderer.hpp"
#include "Engine/Core/SubsystemManager.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Resource/ResourceCommon.hpp"
#include "Engine/Window/Window.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "../Core/RenderState.hpp"
#include "RendererSubsystemConfig.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Camera/ICamera.hpp" // [NEW] ICamera interface for new camera system
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp"
#include "Engine/Graphic/Target/ColorTextureProvider.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"
#include "Engine/Graphic/Target/ShadowColorProvider.hpp"
#include "Engine/Graphic/Target/ShadowTextureProvider.hpp"
#include "Engine/Graphic/Sampler/SamplerConfig.hpp" // [NEW] Dynamic Sampler System
#include "Engine/Graphic/Sampler/SamplerProvider.hpp" // [NEW] Dynamic Sampler System

using namespace enigma::core;
using namespace enigma::graphic;

namespace enigma::graphic
{
    class IRenderTargetProvider;
    class ShaderProgram;
    class ShaderSource;
    class ProgramSet;
    class D12VertexBuffer;
    class D12IndexBuffer;
    class DepthTextureManager;
    class PSOManager;
    class CustomImageManager;
    class VertexLayout; // [NEW] Forward declaration for VertexLayout state API
    class VertexRingBuffer; // [NEW] Forward declaration for RingBuffer wrapper (Option D architecture)
    class IndexRingBuffer; // [NEW] Forward declaration for RingBuffer wrapper

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

            /**
             * @brief 重置统计信息
             */
            void Reset();
        };

    public:
#pragma region Lifecycle Management

        explicit RendererSubsystem(RendererSubsystemConfig& config);
        ~RendererSubsystem() override;

        DECLARE_SUBSYSTEM(RendererSubsystem, "RendererSubsystem", 80)

        /**
          * @brief early initialization phase
          * @details
          * Called before Startup of all subsystems, used for:
          * - Delegate D3D12RenderSystem to initialize DirectX 12 devices and command queues
          * - Set up debugging and verification layers
          *
          */
        void Initialize() override;

        /**
         * @brief Main startup phase
         * @details
         * Called after the Initialize phase, used for:
         * - Initialize rendering resources and status
         */
        void Startup() override;

        /**
         * @brief Close subsystem
         * @details
         * Release all managed resources to ensure no memory leaks:
         * - Release all advanced rendering components
         * - Entrust D3D12RenderSystem to clean up underlying resources
         *
         * @note The destructor will call this method, but calling it explicitly is safer
         */
        void Shutdown() override;

        bool RequiresInitialize() const override { return true; }

        /**
         * @brief Initiates the start of a new rendering frame.
         * @details
         * Handles all necessary preparations for rendering at the beginning of a frame, including:
         * - Resetting per-frame resource offsets and state caches.
         * - Ensuring correctness of GPU and CPU-side pipeline state synchronization.
         * - Preparing the DirectX 12 frame by acquiring the next back buffer and resetting bindings.
         * - Clearing all render targets and depth-stencil buffers as per configuration to ensure a clean rendering state.
         *
         * Teaching Focus:
         * - Illustrates explicit state management and modern graphics API design principles.
         * - Emphasizes responsible resource handling, including frame boundary constraints.
         * - Balances optimization techniques with architectural purity (e.g., frame-local hash caching).
         * - Showcases the importance of separating frame-internal optimizations from inter-frame processes.
         *
         * Key Features:
         * - Resets GPU state and all associated pipeline caches to avoid contamination from the previous frame.
         * - Implements a centralized render target clearing strategy for enhanced state reliability.
         * - Integrates multi-pass rendering considerations by preserving values when necessary.
         *
         * @note This method does not bind any render target. Render target binding is deferred to the UseProgram function.
         *
         * @critical Adjusts internal mechanisms (such as PSO binding and hash caches) to support performance optimization while adhering to frame-specific state requirements.
         */
        void BeginFrame() override;

        void EndFrame() override;
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
         * @brief Use ShaderProgram and bind RenderTargets (pair-based API)
         * @param shaderProgram Shader program pointer
         * @param targets RT binding list as {RTType, index} pairs. Empty = bind SwapChain
         *
         * @details
         * Unified RT binding with pair-based targets:
         * - Non-empty targets: call RenderTargetBinder->BindRenderTargets(targets)
         * - Empty targets: bind SwapChain as default RT
         *
         * @code
         * // GBuffer pass: 4 ColorTex + 1 DepthTex
         * renderer->UseProgram(gbuffersProgram, {
         *     {RTType::ColorTex, 0}, {RTType::ColorTex, 1},
         *     {RTType::ColorTex, 2}, {RTType::ColorTex, 3},
         *     {RTType::DepthTex, 0}
         * });
         *
         * // Shadow pass: ShadowTex only
         * renderer->UseProgram(shadowProgram, {{RTType::ShadowTex, 0}});
         *
         * // Final pass: output to SwapChain (empty targets)
         * renderer->UseProgram(finalProgram);
         * @endcode
         */
        void UseProgram(
            std::shared_ptr<ShaderProgram>             shaderProgram,
            const std::vector<std::pair<RTType, int>>& targets = {}
        );

        /**
         * @brief [NEW] Get RenderTarget provider by RTType
         * @param rtType RT type (ColorTex, DepthTex, ShadowColor, ShadowTex)
         * @return Provider pointer for dynamic RT configuration
         *
         * @details
         * Allows App-side to access providers for dynamic resolution changes.
         * Delegates to m_renderTargetBinder->GetProvider(rtType).
         *
         * @code
         * auto colorProvider = renderer->GetProvider(RTType::ColorTex);
         * colorProvider->SetRtConfig(newConfig);
         * @endcode
         */
        IRenderTargetProvider* GetProvider(RTType rtType);

        /**
         * @brief [NEW] Begin camera rendering - ICamera interface version
         * @param camera ICamera interface reference (supports all camera types)
         *
         * Teaching Points:
         * - Calls camera.UpdateMatrixUniforms() to fill MatricesUniforms
         * - Data flow: ICamera → UpdateMatrixUniforms → UniformManager → GPU
         * - Supports PerspectiveCamera, OrthographicCamera, ShadowCamera, etc.
         */
        void BeginCamera(const ICamera& camera);

        /**
         * @brief 结束Camera渲染
         *
         * 教学要点：
         * - 清理Camera状态
         * - 对应Iris的endCamera()
         */
        void EndCamera(const ICamera& camera);

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
         * @brief 绘制实例化几何体（非索引）
         * @param vertexCount 每个实例的顶点数量
         * @param instanceCount 实例数量
         * @param startVertex 起始顶点（默认0）
         * @param startInstance 起始实例（默认0）
         *
         * 教学要点：
         * - 执行DrawInstanced指令
         * - 对应OpenGL的glDrawArraysInstanced
         * - 对应DirectX 11的DrawInstanced
         * - 使用当前绑定的VertexBuffer
         */
        void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t startInstance = 0);

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
         *
         * 教学要点：
         * - 缓冲区句柄模式：使用用户管理的缓冲区
         * - 生命周期：用户负责缓冲区创建和销毁
         * - 性能优势：避免每帧上传数据
         * - 适用场景：静态或低频更新的几何体
         */
        void DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo);

        /**
         * @brief [NEW] Directly bind VBO/IBO drawing (skip Ring Buffer, used for static geometry)
         * @param vbo vertex buffer smart pointer
         * @param ibo index buffer smart pointer
         *
         *Teaching points:
         * - Performance optimization: avoid memcpy copying static data in each frame
         * - Applicable scenarios: static or low-frequency updated geometry such as Chunk Mesh
         * - Direct binding: use VBO/IBO's own BufferView, no data copy
         * - Difference from DrawVertexBuffer:
         * - DrawVertexBuffer: Copy to Ring Buffer (suitable for dynamic data)
         * - DrawVertexBuffer(vbo, ibo): direct binding (suitable for static data)
         *
         * Reference: DX12Renderer::DrawVertexIndexedInternal implementation
         */
        void DrawVertexBuffer(const std::shared_ptr<D12VertexBuffer>& vbo, const std::shared_ptr<D12IndexBuffer>& ibo);

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

        // ========================================================================
        // Clear Operations - Flexible RT Management
        // ========================================================================

        /**
         * @brief Clear a specific render target with the given color
         * @param rtType RT type (ColorTex, ShadowColor)
         * @param rtIndex Render target index
         * @param clearColor Clear color (default: black)
         *
         * @details
         * [REFACTORED] Provider-based Architecture
         * - 使用 IRenderTargetProvider 统一接口
         * - 支持 ColorTex 和 ShadowColor 类型
         * - 符合 SOLID 原则（依赖倒置）
         *
         * @code
         * // Clear colortex0 to red
         * renderer->ClearRenderTarget(RTType::ColorTex, 0, Rgba8::RED);
         *
         * // Clear shadowcolor0 to black
         * renderer->ClearRenderTarget(RTType::ShadowColor, 0, Rgba8::BLACK);
         * @endcode
         *
         * Teaching Points:
         * - Understand DirectX 12's ClearRenderTargetView API
         * - Learn Provider-based RT architecture
         * - Master flexible render target management
         */
        void ClearRenderTarget(RTType rtType, int rtIndex, const Rgba8& clearColor = Rgba8::BLACK);

        /**
         * @brief Clear a specific depth-stencil texture
         * @param depthIndex Depth texture index [0, 2] (depthtex0/1/2)
         * @param clearDepth Clear depth value (default: 1.0f)
         * @param clearStencil Clear stencil value (default: 0)
         *
         * @details
         * Provides flexible depth-stencil clearing for multi-pass rendering.
         * Supports clearing depth and stencil independently.
         *
         * @code
         * // Clear depthtex0 to default (depth=1.0, stencil=0)
         * renderer->ClearDepthStencil(0);
         *
         * // Clear depthtex1 with custom values
         * renderer->ClearDepthStencil(1, 0.5f, 128);
         * @endcode
         *
         * Teaching Points:
         * - Understand DirectX 12's ClearDepthStencilView API
         * - Learn depth-stencil clearing strategies
         * - Master multi-pass depth management
         */
        void ClearDepthStencil(uint32_t depthIndex = 0, float clearDepth = 1.0f, uint8_t clearStencil = 0);

        /**
         * @brief Clear all active render targets and depth textures
         * @param clearColor Clear color for all RTs (default: black)
         *
         * @details
         * Convenience method to clear all active colortex (0 to activeColorTexCount-1)
         * and all depthtex (0 to 2) in one call.
         *
         * @code
         * // Clear all RTs to black and all depth to 1.0
         * renderer->ClearAllRenderTargets();
         *
         * // Clear all RTs to blue
         * renderer->ClearAllRenderTargets(Rgba8::BLUE);
         * @endcode
         *
         * Teaching Points:
         * - Understand batch clearing operations
         * - Learn efficient multi-RT clearing
         * - Master frame initialization patterns
         */
        void ClearAllRenderTargets(const Rgba8& clearColor = Rgba8::BLACK);

        /**
         * @brief [DEPRECATED] Set blend mode (use SetBlendConfig instead)
         * @param mode Blend mode enum
         *
         * Teaching Points:
         * - Kept for backward compatibility
         * - Internally delegates to SetBlendConfig
         */
        void SetBlendMode(BlendMode mode);

        /**
         * @brief [DEPRECATED] Get current blend mode
         * @return Current BlendMode
         *
         * Teaching Points:
         * - Kept for backward compatibility
         * - Returns mode derived from m_currentBlendConfig
         */
        [[nodiscard]] BlendMode GetBlendMode() const noexcept { return m_currentBlendMode; }

        /**
         * @brief [NEW] Set blend configuration (replaces SetBlendMode)
         * @param config Blend configuration (blend enable, factors, operations)
         *
         * Teaching Points:
         * - More flexible than BlendMode enum
         * - Follows RasterizationConfig design pattern
         * - Use static presets: BlendConfig::Opaque(), Alpha(), Additive(), etc.
         *
         * Usage Examples:
         * @code
         * // Standard opaque rendering
         * renderer->SetBlendConfig(BlendConfig::Opaque());
         *
         * // Alpha blending for translucent objects
         * renderer->SetBlendConfig(BlendConfig::Alpha());
         *
         * // Additive blending for particles
         * renderer->SetBlendConfig(BlendConfig::Additive());
         * @endcode
         */
        void SetBlendConfig(const BlendConfig& config);

        /**
         * @brief [NEW] Get current blend configuration
         * @return Current BlendConfig
         *
         * Teaching Points:
         * - Used for saving/restoring blend state in RenderPass
         * - Returns copy of current config (not reference)
         */
        [[nodiscard]] BlendConfig GetBlendConfig() const noexcept { return m_currentBlendConfig; }

        /**
         * @brief [NEW] Set sampler configuration at index
         * @param index Sampler slot (0-15)
         * @param config Sampler configuration
         *
         * Teaching Points:
         * - Part of Dynamic Sampler System
         * - Delegates to SamplerProvider
         * - Default samplers: Linear(0), Point(1), Shadow(2), PointWrap(3)
         *
         * Usage Examples:
         * @code
         * // Change sampler0 to anisotropic filtering
         * renderer->SetSamplerConfig(0, SamplerConfig::Anisotropic(16));
         *
         * // Reset to default linear
         * renderer->SetSamplerConfig(0, SamplerConfig::Linear());
         * @endcode
         */
        void SetSamplerConfig(uint32_t index, const SamplerConfig& config);

        /**
         * @brief [REFACTORED] Set depth configuration (replaces SetDepthMode)
         * @param config Depth configuration (test enable, write enable, comparison function)
         *
         * Teaching Points:
         * - More flexible than DepthMode enum
         * - Follows RasterizationConfig design pattern
         * - Use static presets: DepthConfig::Enabled(), ReadOnly(), Disabled(), etc.
         *
         * Usage Examples:
         * @code
         * // Standard depth test (read + write)
         * renderer->SetDepthConfig(DepthConfig::Enabled());
         *
         * // Read-only depth (for translucent objects)
         * renderer->SetDepthConfig(DepthConfig::ReadOnly());
         *
         * // Custom configuration
         * renderer->SetDepthConfig(DepthConfig::Custom(true, false, DepthComparison::Greater));
         * @endcode
         */
        void SetDepthConfig(const DepthConfig& config);

        /**
         * @brief Get current depth configuration
         * @return Current DepthConfig (test enable, write enable, comparison function)
         *
         * Teaching Points:
         * - Used for saving/restoring depth state in RenderPass
         * - Returns copy of current config (not reference)
         */
        [[nodiscard]] DepthConfig GetDepthConfig() const noexcept { return m_currentDepthConfig; }

        /**
         * @brief Set stencil test configuration (for PSO)
         * @param detail Stencil test detail (enable, compare function, operations, etc.)
         *
         * Teaching Points:
         * - Stencil configuration is part of PSO (immutable state)
         * - Changing it requires PSO rebuild via PSOManager
         * - Reference value is dynamic and set separately via SetStencilRefValue
         */
        void SetStencilTest(const StencilTestDetail& detail);

        /**
         * @brief Set stencil reference value (dynamic CommandList state)
         * @param refValue Stencil reference value (0-255)
         *
         * Teaching Points:
         * - Reference value is dynamic state (does not affect PSO)
         * - Can be changed per draw call without PSO rebuild
         * - Applied via OMSetStencilRef on active CommandList
         */
        void SetStencilRefValue(uint8_t refValue);

        /**
         * @brief Set rasterization configuration (for PSO)
         * @param config Rasterization configuration (cull mode, fill mode, depth bias, etc.)
         *
         * Teaching Points:
         * - Rasterization configuration is part of PSO (immutable state)
         * - Changing it requires PSO rebuild via PSOManager
         * - Configuration takes effect at next UseProgram call (deferred binding)
         * - Stores config in pending state until PSO is actually created/bound
         *
         * Usage Examples:
         * @code
         * // Disable face culling for clouds/particles
         * renderer->SetRasterizationConfig(RasterizationConfig::NoCull());
         * renderer->UseProgram(cloudProgram);
         * renderer->DrawClouds();
         * 
         * // Restore default culling
         * renderer->SetRasterizationConfig(RasterizationConfig::Default());
         * @endcode
         */
        void SetRasterizationConfig(const RasterizationConfig& config);

        /**
         * @brief Set vertex layout for next draw call
         * @param layout Pointer to vertex layout (nullptr uses default)
         *
         * Teaching Points:
         * - Decouples vertex buffer from layout definition
         * - Enables dynamic layout switching per draw call
         * - Layout affects PSO creation (InputLayout)
         * - nullptr falls back to default layout (Vertex_PCUTBN)
         *
         * Usage Examples:
         * @code
         * // Set terrain-specific layout
         * renderer->SetVertexLayout(VertexLayoutRegistry::GetLayout("Terrain"));
         * renderer->DrawTerrain();
         *
         * // Reset to default layout
         * renderer->SetVertexLayout(nullptr);
         * @endcode
         */
        void SetVertexLayout(const VertexLayout* layout);

        /**
         * @brief Get current vertex layout
         * @return Pointer to current vertex layout
         *
         * Teaching Points:
         * - Returns currently set layout for debugging/validation
         * - Returns default layout if none explicitly set
         */
        [[nodiscard]] const VertexLayout* GetCurrentVertexLayout() const noexcept;
#pragma endregion

#pragma region RenderTarget Management

        /**
         * @brief 获取当前配置的GBuffer colortex数量
         * @return int 当前数量 [1-16]
         *
         * Milestone 3.0: 从m_configuration读取配置值（不再使用m_config）
         */
        int GetGBufferColorTexCount() const { return m_configuration.gbufferColorTexCount; }

        /**
         * @brief 设置CustomImage槽位的纹理
         * @param slotIndex 槽位索引 [0-15]
         * @param texture 纹理指针（可以为nullptr表示清除）
         *
         * @details
         * **使用场景**：
         * - 设置自定义材质纹理到指定槽位
         * - 支持运行时动态更新
         * - 可用于实现自定义后处理效果
         *
         * **实现方式**：
         * - 委托给CustomImageManager::SetCustomImage()
         * - 只修改CPU端数据，不触发GPU上传
         * - GPU上传在Draw时自动完成
         *
         * **执行流程**：
         * 1. 参数验证（slotIndex范围检查）
         * 2. 委托给m_customImageManager->SetCustomImage()
         * 3. 保存纹理指针和Bindless索引
         *
         * @code
         * // 基础用法：设置customImage0
         * auto myTexture = CreateTexture2D(1024, 1024, DXGI_FORMAT_R8G8B8A8_UNORM);
         * renderer->SetCustomImage(0, myTexture);
         *
         * // 清除槽位
         * renderer->SetCustomImage(0, nullptr);
         *
         * // 多槽位使用
         * renderer->SetCustomImage(0, diffuseMap);
         * renderer->SetCustomImage(1, normalMap);
         * renderer->SetCustomImage(2, roughnessMap);
         * @endcode
         *
         * **教学要点**：
         * - 理解委托模式（Delegation Pattern）
         * - 学习封装实现细节的重要性
         * - 掌握用户友好的API设计
         *
         * **注意事项**：
         * - slotIndex必须在 [0-15] 范围内
         * - texture可以为nullptr（清除槽位）
         * - 调用者负责管理texture的生命周期
         * - 不触发GPU上传（延迟到Draw时）
         *
         * @note 此方法提供简洁的用户接口，封装CustomImageManager的实现细节
         * @see GetCustomImage, ClearCustomImage, CustomImageManager::SetCustomImage
         */
        void SetCustomImage(int slotIndex, D12Texture* texture);

        /**
         * @brief 获取CustomImage槽位的纹理
         * @param slotIndex 槽位索引 [0-15]
         * @return D12Texture* 纹理指针（可能为nullptr）
         *
         * @details
         * **使用场景**：
         * - 查询当前槽位绑定的纹理
         * - 用于调试和状态检查
         * - 验证纹理是否正确设置
         *
         * **实现方式**：
         * - 委托给CustomImageManager::GetCustomImage()
         * - 返回原始指针（非所有权）
         * - const方法，不修改对象状态
         *
         * @code
         * // 基础用法：查询槽位
         * D12Texture* tex = renderer->GetCustomImage(0);
         * if (tex != nullptr) {
         *     // 槽位已设置...
         * }
         *
         * // 调试用法：验证纹理
         * auto expectedTex = myTexture;
         * auto actualTex = renderer->GetCustomImage(0);
         * ASSERT(actualTex == expectedTex);
         * @endcode
         *
         * **教学要点**：
         * - 理解const方法的语义
         * - 学习查询接口的设计
         * - 掌握空指针检查的重要性
         *
         * **注意事项**：
         * - 返回值可能为nullptr（槽位未设置）
         * - 不拥有返回指针的所有权
         * - 调用者不应删除返回的指针
         *
         * @see SetCustomImage, ClearCustomImage
         */
        D12Texture* GetCustomImage(int slotIndex) const;

        /**
         * @brief 清除CustomImage槽位的纹理
         * @param slotIndex 槽位索引 [0-15]
         *
         * @details
         * **使用场景**：
         * - 清空指定槽位的纹理绑定
         * - 释放不再使用的槽位
         * - 重置槽位状态
         *
         * **实现方式**：
         * - 委托给CustomImageManager::ClearCustomImage()
         * - 等价于SetCustomImage(slotIndex, nullptr)
         * - 提供更清晰的语义
         *
         * @code
         * // 基础用法：清空槽位
         * renderer->ClearCustomImage(0);
         *
         * // 批量清空
         * for (int i = 0; i < 16; ++i) {
         *     renderer->ClearCustomImage(i);
         * }
         * @endcode
         *
         * **教学要点**：
         * - 理解语义化API的价值
         * - 学习提供多种操作方式
         * - 掌握API设计的一致性
         *
         * @see SetCustomImage, GetCustomImage
         */
        void ClearCustomImage(int slotIndex);

        /**
         * @brief 创建2D纹理（便捷API）
         * @param width 纹理宽度
         * @param height 纹理高度
         * @param format 纹理格式
         * @param initialData 初始数据（可选，默认nullptr）
         * @return D12Texture* 纹理指针（调用者负责管理生命周期）
         *
         * @details
         * **使用场景**：
         * - 创建自定义纹理用于CustomImage
         * - 动态生成纹理数据
         * - 加载外部图片数据
         *
         * **实现方式**：
         * - 委托给D3D12RenderSystem::CreateTexture2D()
         * - 返回裸指针（调用者负责管理生命周期）
         * - 使用unique_ptr::release()转移所有权
         *
         * **执行流程**：
         * 1. 调用D3D12RenderSystem::CreateTexture2D()创建纹理
         * 2. 使用unique_ptr::release()转移所有权
         * 3. 返回裸指针给调用者
         *
         * @code
         * // 基础用法：创建空纹理
         * auto texture = renderer->CreateTexture2D(1024, 1024, DXGI_FORMAT_R8G8B8A8_UNORM);
         * renderer->SetCustomImage(0, texture);
         * // 注意：调用者负责删除texture
         *
         * // 带初始数据
         * std::vector<uint8_t> pixels(1024 * 1024 * 4, 255); // 白色纹理
         * auto texture2 = renderer->CreateTexture2D(1024, 1024, DXGI_FORMAT_R8G8B8A8_UNORM, pixels.data());
         * renderer->SetCustomImage(1, texture2);
         * @endcode
         *
         * **教学要点**：
         * - 理解所有权转移（unique_ptr::release()）
         * - 学习裸指针的生命周期管理
         * - 掌握资源创建的标准流程
         *
         * **注意事项**：
         * - 返回裸指针，调用者负责管理生命周期
         * - 建议使用智能指针管理返回的纹理
         * - 创建失败返回nullptr
         * - initialData可以为nullptr（创建空纹理）
         *
         * **生命周期管理建议**：
         * @code
         * // 推荐：使用智能指针管理
         * std::unique_ptr<D12Texture> texture(renderer->CreateTexture2D(...));
         * renderer->SetCustomImage(0, texture.get());
         * // texture会自动释放
         * @endcode
         *
         * @warning 调用者必须负责删除返回的纹理，否则会内存泄漏
         * @see SetCustomImage, D3D12RenderSystem::CreateTexture2D
         */
        std::shared_ptr<D12Texture> CreateTexture2D(int width, int height, DXGI_FORMAT format, const void* initialData = nullptr);
        std::shared_ptr<D12Texture> CreateTexture2D(const class ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName = "");
        std::shared_ptr<D12Texture> CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName = "");

        /**
         * @brief Get current frame vertex buffer write offset
         * @return Current offset in bytes
         * 
         * [NEW] Option D: Delegates to VertexRingBuffer::GetCurrentOffset()
         * Returns 0 if RingBuffer not yet initialized
         */
        size_t GetCurrentVertexOffset() const noexcept;

        /**
         * @brief Get current frame index buffer write offset
         * @return Current offset in bytes
         * 
         * [NEW] Option D: Delegates to IndexRingBuffer::GetCurrentOffset()
         * Returns 0 if RingBuffer not yet initialized
         */
        size_t GetCurrentIndexOffset() const noexcept;

        // ==================== Milestone 4: 深度缓冲管理 ====================

        /**
         * @brief Bind render targets using pair-based API
         * @param targets Vector of (RTType, index) pairs specifying targets to bind
         *
         * Teaching Points:
         * - Delegates to RenderTargetBinder::BindRenderTargets
         * - Unified API: color targets and depth target in single vector
         * - Depth target identified by RTType::DepthTex or RTType::ShadowTex
         *
         * Usage:
         * ```cpp
         * // Bind colortex0-3 + depthtex0
         * renderer->BindRenderTargets({
         *     {RTType::ColorTex, 0}, {RTType::ColorTex, 1},
         *     {RTType::ColorTex, 2}, {RTType::ColorTex, 3},
         *     {RTType::DepthTex, 0}
         * });
         *
         * // Shadow pass: shadowcolor0 + shadowtex0
         * renderer->BindRenderTargets({
         *     {RTType::ShadowColor, 0},
         *     {RTType::ShadowTex, 0}
         * });
         * ```
         */
        void BindRenderTargets(const std::vector<std::pair<RTType, int>>& targets);
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

        /**
         * @brief 获取Uniform管理器
         * @return UniformManager指针，用于上传Uniform数据
         * @details 
         * 提供对UniformManager的访问，允许Game层上传modelMatrix等Uniform数据
         * 
         * 教学要点:
         * - 引擎-Game层接口设计
         * - 返回原始指针供Game层使用（引擎内部使用unique_ptr管理）
         * - 用于上传变换矩阵、材质参数等Uniform数据
         */
        class UniformManager* GetUniformManager() const noexcept { return m_uniformManager.get(); }

    private:
        /**
         * @brief Prepare PSO and resource binding (common logic of Draw series functions)
         * @param cmdList graphics command list
         * @return true If the preparation is successful, you can initiate a Draw call; false if the preparation fails, you should return early
         *
         * [REFACTOR] Common logic extracted from Draw/DrawIndexed/DrawInstanced:
         * 1. PrepareCustomImages
         * 2. UpdateRingBufferOffsets
         * 3. Get layout with fallback
         * 4. Inline PSO state construction
         * 5. ValidateDrawState
         * 6. GetOrCreatePSO
         * 7. Bind PSO (if changed)
         * 8. Bind Root Signature
         * 9. Set Primitive Topology
         * 10. BindEngineBuffers
         * 11. BindCustomBufferTable
         *
         * @note IncrementDrawCount is not called in this function and is the responsibility of the caller
         */
        bool PreparePSOAndBindings(ID3D12GraphicsCommandList* cmdList);

        /// Swap chain - double buffered display (requires window handle, managed by subsystem)
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

        /// Immediate vertex RingBuffer - encapsulates VBO + offset management
        std::unique_ptr<VertexRingBuffer> m_immediateVertexRingBuffer;

        /// Immediate index RingBuffer - encapsulates IBO + offset management
        std::unique_ptr<IndexRingBuffer> m_immediateIndexRingBuffer;

        /// RenderTarget管理器 - 管理16个colortex RenderTarget (Iris兼容)
        std::unique_ptr<ColorTextureProvider> m_colorTextureProvider;

        /// 深度纹理管理器 - 管理3个深度纹理 (Iris depthtex0/1/2)
        std::unique_ptr<DepthTextureProvider> m_depthTextureProvider;

        /// Shadow Color管理器 - 管理8个shadowcolor RenderTarget (Iris兼容)
        std::unique_ptr<ShadowColorProvider> m_shadowColorProvider;

        /// Shadow Target管理器 - 管理2个shadowtex纹理 (Iris兼容)
        std::unique_ptr<ShadowTextureProvider> m_shadowTextureProvider;

        /// RenderTarget绑定器 - 统一RT绑定接口 (组合4个Manager)
        std::unique_ptr<class RenderTargetBinder> m_renderTargetBinder;

        /// Full Screen Draw
        std::unique_ptr<class FullQuadsRenderer> m_fullQuadsRenderer;

        // ==================== Uniform管理组件 (Phase 3) ====================

        /// Uniform管理器 - 管理所有Uniform Buffer（包括CustomImageIndexBuffer）
        std::unique_ptr<class UniformManager> m_uniformManager;

        /// CustomImage管理器 - 管理16个CustomImage槽位的纹理绑定
        std::unique_ptr<CustomImageManager> m_customImageManager;

        // ==================== PSO管理组件 ====================

        /// PSO管理器 - 动态创建和缓存PSO
        std::unique_ptr<PSOManager> m_psoManager;

        // [NEW] PSO state caching for deferred binding
        ShaderProgram*       m_currentShaderProgram       = nullptr;
        BlendMode            m_currentBlendMode           = BlendMode::Opaque; // [DEPRECATED] Kept for backward compatibility
        BlendConfig          m_currentBlendConfig         = BlendConfig::Opaque(); // [NEW] Dynamic Sampler System
        DepthConfig          m_currentDepthConfig         = DepthConfig::Enabled();
        StencilTestDetail    m_currentStencilTest         = StencilTestDetail::Disabled();
        RasterizationConfig  m_currentRasterizationConfig = RasterizationConfig::CullBack();
        const VertexLayout*  m_currentVertexLayout        = nullptr; // [NEW] Current vertex layout state
        ID3D12PipelineState* m_lastBoundPSO               = nullptr;

        // ==================== Dynamic Sampler System ====================

        /// [NEW] SamplerProvider - Manages bindless samplers (Linear, Point, Shadow, PointWrap)
        std::unique_ptr<SamplerProvider> m_samplerProvider;

        /// Current stencil reference value (CommandList dynamic state)
        uint8_t m_currentStencilRef = 0;

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
