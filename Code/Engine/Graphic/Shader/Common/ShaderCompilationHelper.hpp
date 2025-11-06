/**
 * @file ShaderCompilationHelper.hpp
 * @brief Shader编译辅助工具类 - 静态辅助函数集合
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * ShaderCompilationHelper 提供一组静态辅助函数，用于着色器编译流程中的
 * 文件IO、路径处理、配置生成等通用操作。
 *
 * 职责边界:
 * - + 文件IO操作 (读取HLSL源码)
 * - + 路径处理 (提取程序名、构建Include路径)
 * - + 配置生成 (默认Directives、编译选项转换)
 * - ❌ 不负责实际编译 (由 DXCCompiler 负责)
 * - ❌ 不负责着色器管理 (由 ShaderProgramBuilder 负责)
 *
 * 设计理念:
 * - 纯静态工具类：所有函数都是static，无需实例化
 * - 单一职责：每个函数只做一件事
 * - 跨平台兼容：使用std::filesystem处理路径
 * - 错误处理：使用std::optional表示可能失败的操作
 *
 * 教学要点:
 * - 工具类模式 (Utility Class Pattern)
 * - 静态方法设计 (Static Method Design)
 * - 跨平台路径处理 (Cross-platform Path Handling)
 * - 可选值语义 (Optional Value Semantics)
 *
 * 典型用法:
 * @code
 * // ========== 基本用法 ==========
 * 
 * // 1. 读取着色器源码
 * auto sourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile("gbuffers_basic.vs.hlsl");
 * if (!sourceOpt) {
 *     // 处理文件读取失败
 * }
 *
 * // 2. 提取程序名
 * std::string programName = ShaderCompilationHelper::ExtractProgramNameFromPath("gbuffers_basic.vs.hlsl");
 * // programName = "gbuffers_basic"
 *
 * // 3. 构建Include路径
 * std::vector<std::filesystem::path> userPaths = {"shaders/lib", "shaders/include"};
 * auto includePaths = ShaderCompilationHelper::BuildIncludePaths(userPaths);
 *
 * // 4. 创建默认Directives
 * auto directives = ShaderCompilationHelper::CreateDefaultDirectives();
 *
 * // 5. 转换编译选项
 * ShaderCompileOptions opts;
 * opts.enableDebugInfo = true;
 * auto dxcOpts = ShaderCompilationHelper::ConvertToCompilerOptions(opts, ShaderStage::Vertex);
 * 
 * // ========== 完整编译流程示例 ==========
 * 
 * // 示例 1：从文件编译（最简单）
 * auto vsSourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile("test.vert.hlsl");
 * auto psSourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile("test.frag.hlsl");
 * 
 * if (vsSourceOpt && psSourceOpt) {
 *     // 创建 ShaderSource（自动解析 Directives）
 *     auto vsSource = ShaderSource::FromString(*vsSourceOpt, "test.vert.hlsl");
 *     auto psSource = ShaderSource::FromString(*psSourceOpt, "test.frag.hlsl");
 *     
 *     // 使用 ShaderProgramBuilder 编译
 *     ShaderProgramBuilder builder;
 *     auto program = builder.Build(vsSource, psSource, "TestProgram");
 * }
 * 
 * // 示例 2：自定义编译选项
 * ShaderCompileOptions opts;
 * opts.includePaths = {"shaders/lib"};
 * opts.defines = {"USE_BINDLESS=1"};
 * opts.enableDebugInfo = true;
 * 
 * auto program = g_theRendererSubsystem->CreateShaderProgramFromFiles(
 *     "test.vert.hlsl",
 *     "test.frag.hlsl",
 *     "TestProgram"
 * );
 * 
 * // 示例 3：使用静态工厂方法
 * auto opts1 = ShaderCompileOptions::Default();           // 默认配置
 * auto opts2 = ShaderCompileOptions::WithCommonInclude(); // 包含 Common.hlsl
 * auto opts3 = ShaderCompileOptions::Debug();             // 调试配置
 * @endcode
 */

#pragma once

#include "Engine/Graphic/Shader/ShaderPack/Properties/ProgramDirectives.hpp"
#include "Engine/Graphic/Shader/Compiler/DXCCompiler.hpp"
#include "Engine/Graphic/Resource/CompiledShader.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace enigma::graphic
{
    /**
     * @brief 着色器编译选项配置（高层抽象）
     * 
     * 设计变更（Task 3）：
     * - 移除 autoIncludeCommon 布尔值
     * - 使用 includePaths 列表，用户完全控制 Include 路径
     * - 添加静态工厂方法提供常用配置
     * 
     * 使用示例：
     * @code
     * // ========== 静态工厂方法（推荐） ==========
     * 
     * // 1. 默认配置（不包含 Common.hlsl）
     * auto opts1 = ShaderCompileOptions::Default();
     * 
     * // 2. 包含 Common.hlsl（最常用）
     * auto opts2 = ShaderCompileOptions::WithCommonInclude();
     * 
     * // 3. 调试配置（启用调试信息，禁用优化）
     * auto opts3 = ShaderCompileOptions::Debug();
     * 
     * // ========== 自定义配置 ==========
     * 
     * // 4. 完全自定义
     * ShaderCompileOptions opts4;
     * opts4.includePaths = {"shaders/lib", "shaders/include"};
     * opts4.defines = {"USE_BINDLESS=1", "DEBUG_MODE=1"};
     * opts4.enableDebugInfo = true;
     * opts4.enableOptimization = false;
     * 
     * // 5. 基于默认配置修改
     * auto opts5 = ShaderCompileOptions::WithCommonInclude();
     * opts5.defines.push_back("CUSTOM_DEFINE=1");
     * opts5.enableDebugInfo = true;
     * 
     * // ========== 实际使用 ==========
     * 
     * // 使用自定义选项编译
     * auto program = g_theRendererSubsystem->CreateShaderProgramFromSource(
     *     vsSource, psSource, "CustomShader", opts4
     * );
     * @endcode
     * 
     * 教学要点：
     * - 理解静态工厂方法的优势（语义清晰，易于使用）
     * - 学习如何基于默认配置进行定制
     * - 掌握编译选项对着色器性能的影响
     */
    struct ShaderCompileOptions
    {
        bool                               enableDebugInfo    = false; ///< 是否生成调试信息
        bool                               enableOptimization = true; ///< 是否启用优化 (-O3)
        bool                               enable16BitTypes   = true; ///< 是否启用 16-bit 类型
        bool                               enableBindless     = true; ///< 是否启用 Bindless 支持 (SM 6.6)
        std::vector<std::string>           defines; ///< 编译宏定义 (例: "USE_BINDLESS=1")
        std::vector<std::filesystem::path> includePaths; ///< Include 搜索路径（用户完全控制）

        /**
         * @brief 创建默认配置
         * @return 默认编译选项（不包含 Common.hlsl）
         * 
         * 默认配置：
         * - enableDebugInfo = false
         * - enableOptimization = true
         * - enable16BitTypes = true
         * - enableBindless = true
         * - defines = {}
         * - includePaths = {}（空，不自动包含 Common.hlsl）
         */
        static ShaderCompileOptions Default();

        /**
         * @brief 创建调试配置
         * @return 调试编译选项（启用调试信息，禁用优化）
         * 
         * 调试配置：
         * - enableDebugInfo = true
         * - enableOptimization = false
         * - 其他选项与 Default() 相同
         */
        static ShaderCompileOptions Debug();

        /**
         * @brief 创建包含 Common.hlsl 的配置
         * @return 包含引擎核心路径的编译选项
         * 
         * 配置：
         * - 所有选项与 Default() 相同
         * - includePaths = {"Run/.enigma/assets/engine/shaders/core/"}
         */
        static ShaderCompileOptions WithCommonInclude();
    };

    /**
     * @class ShaderCompilationHelper
     * @brief 着色器编译辅助工具类 - 纯静态工具类
     *
     * 设计理念:
     * - 纯静态类：所有方法都是static，禁止实例化
     * - 无状态：不包含任何成员变量
     * - 单一职责：每个函数只做一件事
     * - 跨平台：使用std::filesystem处理路径
     *
     * 教学要点:
     * - 工具类模式 (Utility Class Pattern)
     * - 静态方法设计 (Static Method Design)
     * - 依赖倒置原则 (DIP): 依赖抽象接口而非具体实现
     */
    class ShaderCompilationHelper
    {
    public:
        // 禁止实例化 (纯静态工具类)
        ShaderCompilationHelper()                                          = delete;
        ~ShaderCompilationHelper()                                         = delete;
        ShaderCompilationHelper(const ShaderCompilationHelper&)            = delete;
        ShaderCompilationHelper& operator=(const ShaderCompilationHelper&) = delete;

        // ========== 文件IO操作 ==========

        /**
         * @brief 从文件读取着色器源码
         * @param filePath HLSL文件路径 (绝对路径或相对路径)
         * @return std::optional<std::string> 成功返回源码字符串，失败返回nullopt
         *
         * 教学要点:
         * - 使用std::optional表示可能失败的操作
         * - 使用std::ifstream进行文件读取
         * - 跨平台路径处理 (std::filesystem::path)
         * - RAII原则：文件自动关闭
         *
         * 错误处理:
         * - 文件不存在 → 返回 nullopt
         * - 文件无法打开 → 返回 nullopt
         * - 读取失败 → 返回 nullopt
         *
         * 示例:
         * @code
         * auto sourceOpt = ShaderCompilationHelper::ReadShaderSourceFromFile("gbuffers_basic.vs.hlsl");
         * if (sourceOpt) {
         *     std::string source = *sourceOpt;
         *     // 使用源码进行编译
         * } else {
         *     // 处理文件读取失败
         *     std::cerr << "Failed to read shader file" << std::endl;
         * }
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 仅支持文本文件 (HLSL源码)
         * - ⚠️ 不支持二进制文件读取
         * - ⚠️ 大文件可能导致内存占用过高
         */
        static std::optional<std::string> ReadShaderSourceFromFile(const std::filesystem::path& filePath);

        // ========== 路径处理 ==========

        /**
         * @brief 从文件路径提取程序名称
         * @param filePath 着色器文件路径 (例: "gbuffers_basic.vs.hlsl")
         * @return std::string 程序名称 (例: "gbuffers_basic")
         *
         * 教学要点:
         * - 字符串处理：提取文件名主干
         * - 命名约定：Iris着色器命名规范
         * - 跨平台路径处理
         *
         * 提取规则:
         * - "gbuffers_basic.vs.hlsl" → "gbuffers_basic"
         * - "composite1.fsh.hlsl" → "composite1"
         * - "shadow.vsh.hlsl" → "shadow"
         * - "/path/to/gbuffers_terrain.vs.hlsl" → "gbuffers_terrain"
         *
         * 实现逻辑:
         * 1. 提取文件名 (去除目录路径)
         * 2. 查找第一个 '.' 的位置
         * 3. 返回 '.' 之前的部分
         *
         * 示例:
         * @code
         * std::string name1 = ShaderCompilationHelper::ExtractProgramNameFromPath("gbuffers_basic.vs.hlsl");
         * // name1 = "gbuffers_basic"
         *
         * std::string name2 = ShaderCompilationHelper::ExtractProgramNameFromPath("/shaders/composite1.fsh.hlsl");
         * // name2 = "composite1"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 假设文件名格式为 "{name}.{stage}.hlsl"
         * - ⚠️ 如果文件名不包含 '.'，返回完整文件名
         */
        static std::string ExtractProgramNameFromPath(const std::filesystem::path& filePath);

        /**
         * @brief 获取引擎着色器核心路径
         * @return std::filesystem::path 引擎着色器核心目录路径
         *
         * 教学要点:
         * - 路径约定：引擎资源目录结构
         * - 跨平台路径处理
         * - 常量路径管理
         *
         * 返回路径:
         * - "Run/.enigma/assets/engine/shaders/core/"
         *
         * 用途:
         * - 定位引擎默认着色器 (Fallback着色器)
         * - 构建Include搜索路径
         * - 资源系统集成
         *
         * 示例:
         * @code
         * std::filesystem::path corePath = ShaderCompilationHelper::GetEngineShaderCorePath();
         * // corePath = "Run/.enigma/assets/engine/shaders/core/"
         *
         * std::filesystem::path shaderPath = corePath / "gbuffers_basic.vs.hlsl";
         * // shaderPath = "Run/.enigma/assets/engine/shaders/core/gbuffers_basic.vs.hlsl"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 路径是相对于项目根目录的
         * - ⚠️ 调用者需要确保路径存在
         */
        static std::filesystem::path GetEngineShaderCorePath();

        /**
         * @brief 构建Include搜索路径列表
         * @param userIncludePaths 用户提供的Include路径列表
         * @return std::vector<std::wstring> 转换为宽字符串的路径列表
         *
         * 教学要点:
         * - 路径转换：std::filesystem::path → std::wstring
         * - DXC编译器要求：Include路径必须是宽字符串
         * - 跨平台兼容性
         *
         * 转换规则:
         * - 将所有用户路径转换为绝对路径
         * - 转换为宽字符串 (DXC要求)
         * - 不强制添加Common.hlsl路径 (由调用者决定)
         *
         * 示例:
         * @code
         * std::vector<std::filesystem::path> userPaths = {
         *     "shaders/lib",
         *     "shaders/include"
         * };
         *
         * auto includePaths = ShaderCompilationHelper::BuildIncludePaths(userPaths);
         * // includePaths = {L"F:/project/shaders/lib", L"F:/project/shaders/include"}
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 相对路径会被转换为绝对路径
         * - ⚠️ 不验证路径是否存在 (由DXC编译器处理)
         * - ⚠️ 不添加引擎默认路径 (由调用者决定)
         */
        static std::vector<std::wstring> BuildIncludePaths(
            const std::vector<std::filesystem::path>& userIncludePaths
        );

        // ========== 配置生成 ==========

        /**
         * @brief 创建默认的ProgramDirectives
         * @return shader::ProgramDirectives 默认配置
         *
         * 教学要点:
         * - 默认值语义：提供合理的默认配置
         * - Iris兼容性：遵循Iris的默认行为
         * - 配置对象模式
         *
         * 默认配置:
         * - drawBuffers: {0} (只输出到 RT0)
         * - 其他指令: std::nullopt (未指定)
         *
         * 用途:
         * - 当着色器源码中没有指令注释时使用
         * - 作为Fallback配置
         * - 测试和调试
         *
         * 示例:
         * @code
         * auto directives = ShaderCompilationHelper::CreateDefaultDirectives();
         * // directives.GetDrawBuffers() = {0}
         * // directives.GetBlendMode() = std::nullopt
         * @endcode
         */
        static shader::ProgramDirectives CreateDefaultDirectives();

        /**
         * @brief 将ShaderCompileOptions转换为DXCCompiler::CompileOptions
         * @param opts 高层编译选项
         * @param stage 着色器阶段 (用于确定target)
         * @return DXCCompiler::CompileOptions DXC编译器选项
         *
         * 教学要点:
         * - 适配器模式 (Adapter Pattern)
         * - 抽象层设计：隐藏DXC编译器的复杂性
         * - 配置转换：高层API → 底层API
         *
         * 转换规则:
         * - stage → target (Vertex → "vs_6_6", Pixel → "ps_6_6", etc.)
         * - enableDebugInfo → enableDebugInfo
         * - enableOptimization → enableOptimization
         * - enable16BitTypes → enable16BitTypes
         * - enableBindless → enableBindless
         * - defines → defines
         * - includePaths → includePaths (转换为wstring)
         *
         * 示例:
         * @code
         * ShaderCompileOptions opts;
         * opts.enableDebugInfo = true;
         * opts.defines.push_back("USE_BINDLESS=1");
         * opts.includePaths.push_back("shaders/lib");
         *
         * auto dxcOpts = ShaderCompilationHelper::ConvertToCompilerOptions(opts, ShaderStage::Vertex);
         * // dxcOpts.target = "vs_6_6"
         * // dxcOpts.entryPoint = "VSMain"
         * // dxcOpts.enableDebugInfo = true
         * // dxcOpts.defines = {"USE_BINDLESS=1"}
         * // dxcOpts.includePaths = {L"F:/project/shaders/lib"}
         * @endcode
         *
         * 注意事项:
         * - ⚠️ entryPoint 根据 stage 自动设置 (VSMain, PSMain, etc.)
         * - ⚠️ target 根据 stage 自动设置 (vs_6_6, ps_6_6, etc.)
         */
        static DXCCompiler::CompileOptions ConvertToCompilerOptions(
            const ShaderCompileOptions& opts,
            ShaderStage                 stage
        );

    private:
        /**
         * @brief 根据ShaderStage获取HLSL Profile
         * @param stage 着色器阶段
         * @return std::string Profile字符串 (例: "vs_6_6", "ps_6_6")
         *
         * 教学要点:
         * - Shader Model 6.6 对应 DXC
         * - 支持 Vertex, Pixel, Geometry, Compute
         * - 未来可扩展 Hull/Domain 阶段
         *
         * 映射关系:
         * - ShaderStage::Vertex → "vs_6_6"
         * - ShaderStage::Pixel → "ps_6_6"
         * - ShaderStage::Compute → "cs_6_6"
         * - ShaderStage::Geometry → "gs_6_6"
         * - ShaderStage::Hull → "hs_6_6"
         * - ShaderStage::Domain → "ds_6_6"
         */
        static std::string GetShaderProfile(ShaderStage stage);

        /**
         * @brief 根据ShaderStage获取入口点名称
         * @param stage 着色器阶段
         * @return std::string 入口函数名 (例: "VSMain", "PSMain")
         *
         * 教学要点:
         * - 标准化入口点命名
         * - 所有着色器遵循统一约定
         *
         * 映射关系:
         * - ShaderStage::Vertex → "VSMain"
         * - ShaderStage::Pixel → "PSMain"
         * - ShaderStage::Compute → "CSMain"
         * - ShaderStage::Geometry → "GSMain"
         * - ShaderStage::Hull → "HSMain"
         * - ShaderStage::Domain → "DSMain"
         */
        static std::string GetEntryPoint(ShaderStage stage);
    };
} // namespace enigma::graphic
