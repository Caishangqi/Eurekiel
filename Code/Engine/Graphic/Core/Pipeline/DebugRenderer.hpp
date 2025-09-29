#pragma once

#include "CompositePass.hpp"
#include "ProgramArrayId.hpp"
#include "TextureStage.hpp"
#include "../DX12/D3D12RenderSystem.hpp"
#include "Engine/Code/Engine/Core/Vertex_PCU.hpp"
#include <memory>
#include <vector>
#include <string>

#include "Engine/Graphic/Immediate/RenderCommand.hpp"
#include "Engine/Graphic/Immediate/RenderCommandQueue.hpp"

struct Vertex_PCU;


namespace enigma::graphic
{
    class D12RenderTargets;
    class IShaderRenderingPipeline;
    class ProgramSet;
    class BindlessResourceManager;
    class UniformManager;
    class D12Texture;
    class D12Buffer;
}

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的调试渲染器
     *
     * 严格按照Iris源码的CompositeRenderer构造模式设计：
     * new CompositeRenderer(this, CompositePass.DEBUG, programSet, renderTargets, TextureStage.DEBUG)
     *
     * 这个渲染器专门用于开发和测试DirectX 12渲染管线的核心功能：
     * - Bindless纹理注册和使用
     * - 基础几何体渲染 (三角形、四边形)
     * - Immediate模式指令执行
     * - Uniform系统验证
     * - 管线状态调试
     *
     * 设计原则：
     * - 最小可行原型(MVP)验证
     * - 专注核心功能，忽略复杂特性
     * - 便于迭代开发和调试
     * - 为完整管线打基础
     *
     * 教学要点：
     * - 理解Iris渲染器的构造模式
     * - 学习调试渲染器的设计思路
     * - 掌握渲染管线的验证方法
     * - 体会自顶向下的开发策略
     *
     * Iris架构对应：
     * Java: CompositeRenderer with DEBUG pass
     * C++:  DebugRenderer (specialized CompositeRenderer)
     */
    class DebugRenderer final
    {
    public:
        /**
         * @brief 调试渲染配置
         */
        struct DebugConfig
        {
            bool  enableWireframe      = false; // 启用线框模式
            bool  enableBoundingBoxes  = false; // 启用包围盒渲染
            bool  enableNormals        = false; // 启用法线可视化
            bool  enableBindlessStats  = true; // 启用Bindless统计
            bool  enablePerformanceHUD = true; // 启用性能显示
            float debugGeometryScale   = 1.0f; // 调试几何体缩放
        };

    private:
        // ===========================================
        // Iris兼容的5参数成员变量
        // ===========================================

        /// 父渲染管线引用 - 对应Iris的this参数
        IShaderRenderingPipeline* m_parentPipeline = nullptr;

        /// 合成Pass类型 - 对应Iris的CompositePass.DEBUG
        CompositePass m_compositePass;

        /// 程序集 - 对应Iris的programSet.getComposite(ProgramArrayId.Debug)
        /// 暂时为nullptr，等待着色器系统实现后填充
        ProgramSet* m_programSet = nullptr;

        /// 渲染目标管理器 - 对应Iris的renderTargets
        std::shared_ptr<D12RenderTargets> m_renderTargets;

        /// 纹理阶段 - 对应Iris的TextureStage.DEBUG
        TextureStage m_textureStage;

        // ===========================================
        // 调试渲染核心组件
        // ===========================================

        /// 命令列表管理器
        std::shared_ptr<CommandListManager> m_commandManager;

        /// Immediate模式指令队列 - 专门处理DEBUG阶段的指令
        std::unique_ptr<RenderCommandQueue> m_debugCommandQueue;

        /// Uniform管理器
        std::shared_ptr<UniformManager> m_uniformManager;

        // 注意：移除了 m_bindlessManager 成员变量
        // 原因：DebugRenderer不应该直接持有BindlessResourceManager
        // 正确做法：通过D3D12RenderSystem::RegisterResourceToBindless()调用

        // ===========================================
        // 调试渲染状态
        // ===========================================

        /// 调试配置
        DebugConfig m_config;

        /// 是否已初始化
        bool m_isInitialized = false;

        /// 当前帧索引
        uint64_t m_currentFrame = 0;

        /// 性能统计
        struct DebugStats
        {
            uint32_t texturesRegistered = 0;
            uint32_t geometriesRendered = 0;
            uint32_t commandsExecuted   = 0;
            float    lastFrameTime      = 0.0f;
        } m_stats;

        // ===========================================
        // 预定义调试几何体
        // ===========================================

        /// 调试三角形顶点数据
        std::vector<Vertex_PCU> m_debugTriangleVertices;

        /// 调试四边形顶点数据
        std::vector<Vertex_PCU> m_debugQuadVertices;

        /// 顶点缓冲区
        std::shared_ptr<D12Buffer> m_vertexBuffer;

        /// 索引缓冲区
        std::shared_ptr<D12Buffer> m_indexBuffer;

    public:
        /**
         * @brief 构造函数 - 严格遵循Iris的5参数模式
         *
         * 对应Iris调用：
         * new CompositeRenderer(this, CompositePass.DEBUG, programSet.getComposite(ProgramArrayId.Debug), renderTargets, TextureStage.DEBUG)
         *
         * @param parentPipeline 父渲染管线 (对应Iris的this)
         * @param compositePass 合成Pass类型 (CompositePass::DEBUG)
         * @param programSet 程序集 (暂时nullptr，等待着色器系统实现)
         * @param renderTargets 渲染目标管理器
         * @param textureStage 纹理阶段 (TextureStage::DEBUG)
         */
        explicit DebugRenderer(
            IShaderRenderingPipeline*         parentPipeline,
            CompositePass                     compositePass,
            ProgramSet*                       programSet,
            std::shared_ptr<D12RenderTargets> renderTargets,
            TextureStage                      textureStage
        );

        /**
         * @brief 析构函数
         */
        ~DebugRenderer();

        // 禁用拷贝和移动
        DebugRenderer(const DebugRenderer&)            = delete;
        DebugRenderer& operator=(const DebugRenderer&) = delete;
        DebugRenderer(DebugRenderer&&)                 = delete;
        DebugRenderer& operator=(DebugRenderer&&)      = delete;

        // ===========================================
        // 初始化和清理
        // ===========================================

        /**
         * @brief 初始化调试渲染器
         *
         * 创建所有必要的DirectX 12资源：
         * - 顶点缓冲区和索引缓冲区
         * - 调试几何体数据
         * - Immediate指令队列
         * - 基础着色器资源
         *
         * @return 是否初始化成功
         */
        bool Initialize();

        /**
         * @brief 清理资源
         */
        void Destroy();

        // ===========================================
        // 渲染接口 - 对应Iris的renderAll()
        // ===========================================

        /**
         * @brief 执行所有调试渲染 - 对应Iris CompositeRenderer.renderAll()
         *
         * 核心渲染循环，执行DEBUG阶段的所有渲染指令：
         * 1. 从RenderCommandQueue获取DEBUG阶段的指令
         * 2. 按顺序执行所有指令
         * 3. 更新Bindless资源状态
         * 4. 渲染调试信息
         */
        void RenderAll();

        /**
         * @brief 开始调试渲染帧
         *
         * @param frameIndex 帧索引
         */
        void BeginFrame(uint64_t frameIndex);

        /**
         * @brief 结束调试渲染帧
         */
        void EndFrame();

        // ===========================================
        // 调试几何体渲染接口
        // ===========================================

        /**
         * @brief 渲染调试三角形
         *
         * 使用指定的纹理索引渲染一个三角形。
         *
         * @param bindlessTextureIndex Bindless纹理索引
         * @param transform 变换矩阵 (暂时使用单位矩阵)
         */
        void RenderDebugTriangle(uint32_t bindlessTextureIndex, const void* transform = nullptr);

        /**
         * @brief 渲染调试四边形
         *
         * 使用指定的纹理索引渲染一个四边形。
         *
         * @param bindlessTextureIndex Bindless纹理索引
         * @param transform 变换矩阵 (暂时使用单位矩阵)
         */
        void RenderDebugQuad(uint32_t bindlessTextureIndex, const void* transform = nullptr);

        /**
         * @brief 渲染自定义顶点数据
         *
         * 渲染用户提供的顶点数据。
         *
         * @param vertices 顶点数据
         * @param bindlessTextureIndex Bindless纹理索引
         */
        void RenderCustomVertices(const std::vector<Vertex_PCU>& vertices, uint32_t bindlessTextureIndex);


        // ===========================================
        // Immediate模式集成接口
        // ===========================================

        /**
         * @brief 从RenderCommandQueue获取DEBUG阶段指令
         *
         * 这是与Immediate模块集成的关键接口，获取TestGame提交的DEBUG阶段指令。
         *
         * @param globalQueue 全局渲染指令队列
         */
        void ProcessImmediateCommands(RenderCommandQueue* globalQueue);

        /**
         * @brief 提交调试指令到队列
         *
         * 便捷接口，用于内部生成调试指令。
         *
         * @param command 渲染指令
         */
        void SubmitDebugCommand(RenderCommandPtr command);

        // ===========================================
        // 配置和状态查询
        // ===========================================

        /**
         * @brief 更新调试配置
         *
         * @param config 新的调试配置
         */
        void UpdateConfig(const DebugConfig& config) { m_config = config; }

        /**
         * @brief 获取调试配置
         */
        const DebugConfig& GetConfig() const { return m_config; }

        /**
         * @brief 获取调试统计
         */
        const DebugStats& GetStats() const { return m_stats; }

        /**
         * @brief 检查是否已初始化
         */
        bool IsInitialized() const { return m_isInitialized; }

        /**
         * @brief 获取纹理阶段
         */
        TextureStage GetTextureStage() const { return m_textureStage; }

        /**
         * @brief 获取合成Pass类型
         */
        CompositePass GetCompositePass() const { return m_compositePass; }

    private:
        // ===========================================
        // 内部实现方法
        // ===========================================

        /**
         * @brief 创建调试几何体数据
         */
        bool CreateDebugGeometry();

        /**
         * @brief 创建DirectX 12缓冲区
         */
        bool CreateD3D12Buffers();

        /**
         * @brief 执行单个渲染指令
         */
        void ExecuteRenderCommand(const IRenderCommand* command);

        /**
         * @brief 更新统计信息
         */
        void UpdateStats();

        /**
         * @brief 记录调试日志
         */
        void LogDebugInfo(const std::string& message) const;

        /**
         * @brief 验证渲染状态
         */
        bool ValidateRenderState() const;
    };
} // namespace enigma::graphic
