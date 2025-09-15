#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <d3d12.h>
#include <d3dcompiler.h>

// 前向声明
namespace enigma::resource
{
    class IResourceProvider;
    struct ResourceLocation;
}

namespace enigma::graphic
{
    // 前向声明
    class GBuffer;
    class BindlessResourceManager;

    /**
     * @brief ShaderPackManager类 - 支持Iris注释的HLSL着色器管理系统
     * 
     * 教学要点:
     * 1. 实现Iris兼容的注释解析：支持RENDERTARGETS、格式配置、渲染状态等
     * 2. 纯HLSL着色器支持：避免GLSL到HLSL的复杂转换
     * 3. 与资源系统集成：通过IResourceProvider加载ShaderPack资源
     * 4. 热重载支持：开发时实时重新编译和加载着色器
     * 
     * Iris注释支持:
     * - RENDERTARGETS/DRAWBUFFERS: 指定MRT输出目标
     * - GAUX*FORMAT: 配置RT像素格式
     * - GAUX*SIZE: 配置RT分辨率缩放
     * - BLEND/DEPTH/CULL: 渲染状态配置
     * - COMPUTE_THREADS: 计算着色器配置
     * 
     * HLSL程序类型:
     * - gbuffers_*: 18种几何体渲染程序 
     * - deferred1-99: 延迟光照计算程序
     * - composite1-99: 后处理效果程序
     * - setup1-99: 计算着色器初始化程序
     * - begin1-99/final: 帧开始和结束程序
     * 
     * @note 专为Enigma延迟渲染器设计，完全兼容Iris Shader Pack规范
     */
    class ShaderPackManager final
    {
    public:
        /**
         * @brief 着色器程序类型枚举
         * 
         * 教学要点: 对应Iris规范的各种着色器程序类型
         */
        enum class ShaderType
        {
            // Setup阶段 - Compute Shader only
            Setup, // setup1.csh - setup99.csh

            // Begin阶段 - Composite-style
            Begin, // begin1.vsh/.fsh - begin99.vsh/.fsh

            // Shadow阶段 - Gbuffers-style
            Shadow, // shadow.vsh/.fsh

            // ShadowComp阶段 - Composite-style
            ShadowComp, // shadowcomp1.vsh/.fsh - shadowcomp99.vsh/.fsh

            // Prepare阶段 - Composite-style
            Prepare, // prepare1.vsh/.fsh - prepare99.vsh/.fsh

            // GBuffers阶段 - Gbuffers-style (18种不同程序)
            GBuffers_Terrain, // gbuffers_terrain.vsh/.fsh
            GBuffers_Entities, // gbuffers_entities.vsh/.fsh
            GBuffers_EntitiesTranslucent, // gbuffers_entities_translucent.vsh/.fsh
            GBuffers_Hand, // gbuffers_hand.vsh/.fsh
            GBuffers_Weather, // gbuffers_weather.vsh/.fsh
            GBuffers_Block, // gbuffers_block.vsh/.fsh
            GBuffers_BeaconBeam, // gbuffers_beaconbeam.vsh/.fsh
            GBuffers_Item, // gbuffers_item.vsh/.fsh
            GBuffers_Entities_glowing, // gbuffers_entities_glowing.vsh/.fsh
            GBuffers_Glint, // gbuffers_glint.vsh/.fsh
            GBuffers_Eyes, // gbuffers_eyes.vsh/.fsh
            GBuffers_Armor_glint, // gbuffers_armor_glint.vsh/.fsh
            GBuffers_SpiderEyes, // gbuffers_spidereyes.vsh/.fsh
            GBuffers_Hand_water, // gbuffers_hand_water.vsh/.fsh
            GBuffers_Textured, // gbuffers_textured.vsh/.fsh
            GBuffers_Textured_lit, // gbuffers_textured_lit.vsh/.fsh
            GBuffers_Skybasic, // gbuffers_skybasic.vsh/.fsh
            GBuffers_Skytextured, // gbuffers_skytextured.vsh/.fsh
            GBuffers_Clouds, // gbuffers_clouds.vsh/.fsh
            GBuffers_Water, // gbuffers_water.vsh/.fsh

            // Deferred阶段 - Composite-style
            Deferred, // deferred1.vsh/.fsh - deferred99.vsh/.fsh

            // Composite阶段 - Composite-style  
            Composite, // composite1.vsh/.fsh - composite99.vsh/.fsh

            // Final阶段 - Composite-style
            Final // final.vsh/.fsh
        };

        /**
         * @brief 着色器阶段枚举 (GPU流水线阶段)
         */
        enum class ShaderStage
        {
            Vertex, // 顶点着色器 (.vsh -> .hlsl)
            Pixel, // 像素着色器 (.fsh -> .hlsl)
            Compute, // 计算着色器 (.csh -> .hlsl)
            Geometry, // 几何着色器 (.gsh -> .hlsl) - 可选
            Hull, // 外壳着色器 (.tcs -> .hlsl) - 可选
            Domain // 域着色器 (.tes -> .hlsl) - 可选
        };

    private:
        /**
         * @brief Iris注释配置结构
         * 
         * 教学要点: 封装从着色器注释解析出的配置信息
         */
        struct IrisAnnotations
        {
            // 渲染目标配置
            std::vector<uint32_t> renderTargets; // RENDERTARGETS: 0,1,2,3
            std::string           drawBuffers; // DRAWBUFFERS: 0123 (兼容格式)

            // RT格式配置
            std::unordered_map<std::string, std::string>             rtFormats; // GAUX1FORMAT: RGBA32F
            std::unordered_map<std::string, std::pair<float, float>> rtSizes; // GAUX1SIZE: 0.5 0.5

            // 渲染状态配置
            std::optional<std::string> blendMode; // BLEND: SrcAlpha OneMinusSrcAlpha
            std::optional<std::string> depthTest; // DEPTHTEST: LessEqual
            std::optional<bool>        depthWrite; // DEPTHWRITE: false
            std::optional<std::string> cullFace; // CULLFACE: Back

            // 计算着色器配置
            std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeThreads; // COMPUTE_THREADS: 16,16,1
            std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeSize; // COMPUTE_SIZE: 1920,1080,1

            // 其他配置
            std::unordered_map<std::string, std::string> customDefines; // 自定义宏定义

            IrisAnnotations();
            void Reset();
        };

        /**
         * @brief 编译后的着色器程序
         */
        struct CompiledShader
        {
            ShaderType  type; // 着色器类型
            ShaderStage stage; // GPU流水线阶段
            std::string name; // 着色器名称
            std::string entryPoint; // 入口函数名
            std::string profile; // HLSL编译配置 (vs_5_0, ps_5_0等)

            ID3DBlob*            bytecode; // 编译后的字节码
            ID3D12PipelineState* pipelineState; // 管线状态对象 (如果已创建)

            IrisAnnotations annotations; // 解析的注释配置
            std::string     sourceCode; // 原始HLSL代码 (用于热重载)

            CompiledShader();
            ~CompiledShader();

            // 禁用拷贝
            CompiledShader(const CompiledShader&)            = delete;
            CompiledShader& operator=(const CompiledShader&) = delete;

            // 支持移动
            CompiledShader(CompiledShader&& other) noexcept;
            CompiledShader& operator=(CompiledShader&& other) noexcept;
        };

        /**
         * @brief ShaderPack数据结构 (空壳实现，为将来扩展预留)
         * 
         * 教学要点: 代表一个完整的Shader Pack资源包
         */
        struct ShaderPack
        {
            std::string name; // Shader Pack名称
            std::string version; // 版本号
            std::string author; // 作者
            std::string description; // 描述

            // 着色器文件路径映射 (将来可扩展为完整的资源管理)
            std::unordered_map<std::string, std::string> shaderFiles;

            ShaderPack();
        };

        // 核心资源
        ID3D12Device*                        m_device; // DX12设备 (外部管理)
        enigma::resource::IResourceProvider* m_resourceProvider; // 资源提供者 (外部管理)

        // Shader管理
        std::unordered_map<std::string, std::unique_ptr<CompiledShader>> m_shaders; // 所有编译的着色器
        std::unordered_map<ShaderType, std::vector<std::string>>         m_shadersByType; // 按类型分组的着色器
        std::unique_ptr<ShaderPack>                                      m_currentPack; // 当前加载的Shader Pack

        // 编译器配置
        uint32_t m_compileFlags; // HLSL编译标志
        bool     m_debugMode; // 调试模式 (包含调试信息)
        bool     m_optimizeShaders; // 是否优化着色器

        // 热重载支持
        std::unordered_map<std::string, std::filesystem::file_time_type> m_fileWatchList; // 文件监视列表
        bool                                                             m_hotReloadEnabled; // 是否启用热重载
        std::chrono::steady_clock::time_point                            m_lastCheckTime; // 上次检查时间

        // 缓存系统
        std::string m_cacheDirectory; // 着色器缓存目录
        bool        m_enableCache; // 是否启用缓存

        // 统计信息
        uint32_t m_totalShaders; // 总着色器数量
        uint32_t m_compiledShaders; // 已编译着色器数量
        uint32_t m_failedShaders; // 编译失败着色器数量

        // 状态管理
        bool m_initialized; // 初始化状态

    public:
        /**
         * @brief 构造函数
         * 
         * 教学要点: 基本初始化，实际资源创建在Initialize中
         */
        ShaderPackManager();

        /**
         * @brief 析构函数
         * 
         * 教学要点: 释放所有编译的着色器和资源
         */
        ~ShaderPackManager();

        // ========================================================================
        // 生命周期管理
        // ========================================================================

        /**
         * @brief 初始化着色器管理系统
         * @param device DX12设备指针
         * @param resourceProvider 资源提供者指针
         * @param cacheDirectory 着色器缓存目录 (可选)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点:
         * 1. 设置HLSL编译器配置
         * 2. 创建着色器缓存目录
         * 3. 初始化文件监视系统 (用于热重载)
         */
        bool Initialize(ID3D12Device*                        device,
                        enigma::resource::IResourceProvider* resourceProvider,
                        const std::string&                   cacheDirectory = "");

        /**
         * @brief 释放所有着色器资源
         * 
         * 教学要点: 按正确顺序释放D3D12资源，避免泄漏
         */
        void Shutdown();

        // ========================================================================
        // ShaderPack加载和管理
        // ========================================================================

        /**
         * @brief 加载Shader Pack
         * @param packName Shader Pack名称 (对应资源命名空间)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点:
         * 1. 通过资源提供者加载Shader Pack文件
         * 2. 扫描所有HLSL着色器文件
         * 3. 解析Iris注释配置
         * 4. 批量编译所有着色器
         */
        bool LoadShaderPack(const std::string& packName);

        /**
         * @brief 卸载当前Shader Pack
         * 
         * 教学要点: 释放所有相关资源，重置到默认状态
         */
        void UnloadShaderPack();

        /**
         * @brief 重新加载当前Shader Pack (热重载)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 支持开发时的实时着色器更新
         */
        bool ReloadShaderPack();

        /**
         * @brief 获取当前Shader Pack信息
         * @return Shader Pack指针，未加载返回nullptr
         */
        const ShaderPack* GetCurrentShaderPack() const { return m_currentPack.get(); }

        // ========================================================================
        // 着色器编译和管理
        // ========================================================================

        /**
         * @brief 编译单个HLSL着色器
         * @param shaderName 着色器名称 (如"gbuffers_terrain")
         * @param shaderCode HLSL源代码
         * @param type 着色器类型
         * @param stage GPU流水线阶段
         * @return 成功返回true，失败返回false
         * 
         * 教学要点:
         * 1. 解析HLSL代码中的Iris注释
         * 2. 使用D3DCompiler编译HLSL到字节码
         * 3. 缓存编译结果以提升性能
         */
        bool CompileShader(const std::string& shaderName, const std::string& shaderCode,
                           ShaderType         type, ShaderStage              stage);

        /**
         * @brief 获取编译后的着色器
         * @param shaderName 着色器名称
         * @return 着色器指针，未找到返回nullptr
         */
        const CompiledShader* GetShader(const std::string& shaderName) const;

        /**
         * @brief 获取指定类型的所有着色器
         * @param type 着色器类型
         * @return 着色器名称列表
         * 
         * 教学要点: 用于实现Iris的fallback机制
         */
        std::vector<std::string> GetShadersByType(ShaderType type) const;

        /**
         * @brief 查找可用的着色器 (实现fallback机制)
         * @param preferredNames 首选着色器名称列表 (按优先级排序)
         * @param type 着色器类型
         * @return 第一个找到的着色器名称，都未找到返回空字符串
         * 
         * 教学要点: Iris的fallback系统，如果gbuffers_terrain不存在，尝试gbuffers_basic等
         */
        std::string FindAvailableShader(const std::vector<std::string>& preferredNames, ShaderType type) const;

        // ========================================================================
        // Iris注释解析和配置应用
        // ========================================================================

        /**
         * @brief 解析Iris注释
         * @param shaderCode HLSL源代码
         * @param annotations 输出的注释配置
         * @return 成功返回true，失败返回false
         * 
         * 教学要点:
         * 1. 使用正则表达式匹配注释模式
         * 2. 解析各种注释类型 (RENDERTARGETS, FORMAT等)
         * 3. 验证配置的有效性
         */
        bool ParseIrisAnnotations(const std::string& shaderCode, IrisAnnotations& annotations) const;

        /**
         * @brief 应用着色器注释配置到G-Buffer
         * @param shaderName 着色器名称
         * @param gBuffer G-Buffer管理器
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 根据着色器注释动态配置RT格式和大小
         */
        bool ApplyAnnotationsToGBuffer(const std::string& shaderName, GBuffer& gBuffer) const;

        /**
         * @brief 获取着色器的渲染目标配置
         * @param shaderName 着色器名称
         * @return 渲染目标索引列表
         */
        std::vector<uint32_t> GetShaderRenderTargets(const std::string& shaderName) const;

        /**
         * @brief 获取着色器的混合模式配置
         * @param shaderName 着色器名称
         * @return 混合模式字符串，未配置返回空
         */
        std::string GetShaderBlendMode(const std::string& shaderName) const;

        // ========================================================================
        // 热重载和开发支持
        // ========================================================================

        /**
         * @brief 启用/禁用热重载
         * @param enabled 是否启用
         * 
         * 教学要点: 开发模式下启用，发布模式下禁用以节省性能
         */
        void SetHotReloadEnabled(bool enabled) { m_hotReloadEnabled = enabled; }

        /**
         * @brief 检查文件更新并执行热重载
         * 
         * 教学要点: 定期调用此方法检查着色器文件是否被修改
         */
        void CheckAndReloadModifiedShaders();

        /**
         * @brief 添加文件到热重载监视列表
         * @param filePath 文件路径
         */
        void AddToWatchList(const std::string& filePath);

        /**
         * @brief 从监视列表移除文件
         * @param filePath 文件路径
         */
        void RemoveFromWatchList(const std::string& filePath);

        // ========================================================================
        // 编译器配置
        // ========================================================================

        /**
         * @brief 设置HLSL编译标志
         * @param flags 编译标志 (D3DCOMPILE_*)
         * 
         * 教学要点: 调试模式使用D3DCOMPILE_DEBUG，发布模式使用D3DCOMPILE_OPTIMIZATION_LEVEL3
         */
        void SetCompileFlags(uint32_t flags) { m_compileFlags = flags; }

        /**
         * @brief 启用/禁用调试模式
         * @param enabled 是否启用
         */
        void SetDebugMode(bool enabled) { m_debugMode = enabled; }

        /**
         * @brief 启用/禁用着色器优化
         * @param enabled 是否启用
         */
        void SetOptimizationEnabled(bool enabled) { m_optimizeShaders = enabled; }

        /**
         * @brief 设置着色器缓存目录
         * @param directory 缓存目录路径
         */
        void SetCacheDirectory(const std::string& directory);

        /**
         * @brief 启用/禁用着色器缓存
         * @param enabled 是否启用
         */
        void SetCacheEnabled(bool enabled) { m_enableCache = enabled; }

        // ========================================================================
        // 查询和统计接口
        // ========================================================================

        /**
         * @brief 获取着色器统计信息
         * @param total 总着色器数量
         * @param compiled 已编译数量
         * @param failed 失败数量
         */
        void GetShaderStats(uint32_t& total, uint32_t& compiled, uint32_t& failed) const
        {
            total    = m_totalShaders;
            compiled = m_compiledShaders;
            failed   = m_failedShaders;
        }

        /**
         * @brief 检查指定着色器是否存在
         * @param shaderName 着色器名称
         * @return 存在返回true，否则返回false
         */
        bool HasShader(const std::string& shaderName) const;

        /**
         * @brief 获取所有已加载的着色器名称
         * @return 着色器名称列表
         */
        std::vector<std::string> GetAllShaderNames() const;

        /**
         * @brief 检查管理器是否已初始化
         * @return 已初始化返回true，否则返回false
         */
        bool IsInitialized() const { return m_initialized; }

        /**
         * @brief 检查是否有Shader Pack已加载
         * @return 已加载返回true，否则返回false
         */
        bool HasShaderPackLoaded() const { return m_currentPack != nullptr; }

    private:
        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 扫描Shader Pack中的所有着色器文件
         * @param packName Shader Pack名称
         * @return 着色器文件路径列表
         */
        std::vector<std::string> ScanShaderFiles(const std::string& packName) const;

        /**
         * @brief 根据文件名推断着色器类型
         * @param fileName 着色器文件名
         * @param stage 输出的GPU流水线阶段
         * @return 着色器类型
         */
        ShaderType InferShaderType(const std::string& fileName, ShaderStage& stage) const;

        /**
         * @brief 生成HLSL编译配置 (profile)
         * @param stage GPU流水线阶段
         * @return HLSL profile字符串 (如"vs_5_0")
         */
        std::string GetHLSLProfile(ShaderStage stage) const;

        /**
         * @brief 从缓存加载着色器
         * @param shaderName 着色器名称
         * @param sourceHash 源代码哈希值
         * @return 加载成功返回true，否则返回false
         */
        bool LoadShaderFromCache(const std::string& shaderName, const std::string& sourceHash);

        /**
         * @brief 保存着色器到缓存
         * @param shaderName 着色器名称
         * @param sourceHash 源代码哈希值
         * @param bytecode 编译字节码
         */
        void SaveShaderToCache(const std::string& shaderName, const std::string& sourceHash, ID3DBlob* bytecode);

        /**
         * @brief 计算源代码哈希值
         * @param sourceCode 源代码
         * @return 哈希值字符串
         */
        std::string CalculateSourceHash(const std::string& sourceCode) const;

        /**
         * @brief 解析单个Iris注释行
         * @param line 注释行
         * @param annotations 输出的注释配置
         */
        void ParseAnnotationLine(const std::string& line, IrisAnnotations& annotations) const;

        /**
         * @brief 获取着色器类型的字符串名称 (调试用)
         * @param type 着色器类型
         * @return 类型名称
         */
        static const char* GetShaderTypeName(ShaderType type);

        /**
         * @brief 获取着色器阶段的字符串名称 (调试用)
         * @param stage 着色器阶段
         * @return 阶段名称
         */
        static const char* GetShaderStageName(ShaderStage stage);

        // 禁用拷贝构造和赋值操作
        ShaderPackManager(const ShaderPackManager&)            = delete;
        ShaderPackManager& operator=(const ShaderPackManager&) = delete;

        // 支持移动语义
        ShaderPackManager(ShaderPackManager&&)            = default;
        ShaderPackManager& operator=(ShaderPackManager&&) = default;
    };
} // namespace enigma::graphic
