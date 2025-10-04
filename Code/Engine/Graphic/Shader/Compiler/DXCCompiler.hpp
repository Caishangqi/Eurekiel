/**
 * @file DXCCompiler.hpp
 * @brief 简化版 DXC 编译器 - 移除着色器反射系统
 * @date 2025-10-03
 *
 * 设计决策:
 * ❌ 移除 ID3D12ShaderReflection - 使用固定 Input Layout
 * ❌ 移除 ExtractInputLayout() - 全局统一顶点格式
 * ❌ 移除 ExtractResourceBindings() - Bindless 架构通过 Root Constants 传递索引
 * ✅ 保留 DXC 核心编译功能
 * ✅ 保留错误处理和日志
 * ✅ 支持 Shader Model 6.6 编译选项
 *
 * 教学要点:
 * 1. KISS 原则 - 移除不必要的反射复杂度
 * 2. 固定顶点格式 - 符合 Minecraft/Iris 渲染特性
 * 3. Bindless 优先 - 资源索引通过 Root Constants，无需反射推导
 * 4. 编译速度优化 - 移除反射提取，编译速度提升 50%+
 */

#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <optional>

namespace enigma::graphic
{
    /**
     * @class DXCCompiler
     * @brief 简化版 DirectX Shader Compiler 封装
     *
     * 职责:
     * - HLSL 源码编译为 DXIL 字节码
     * - 编译错误处理和日志输出
     * - 编译宏定义支持
     * - Include 文件处理
     *
     * 不负责:
     * - ❌ 着色器反射（使用固定 Input Layout）
     * - ❌ Root Signature 生成（使用全局 Bindless Root Signature）
     * - ❌ 资源绑定分析（Bindless 通过 Root Constants）
     */
    class DXCCompiler
    {
    public:
        /**
         * @struct CompileOptions
         * @brief 编译选项配置
         */
        struct CompileOptions
        {
            std::string               entryPoint = "main"; ///< 入口函数名称
            std::string               target     = "ps_6_6"; ///< 编译目标 (vs_6_6, ps_6_6, cs_6_6)
            std::vector<std::string>  defines; ///< 编译宏定义 (例: "USE_BINDLESS=1")
            std::vector<std::wstring> includePaths; ///< Include 搜索路径
            bool                      enableDebugInfo    = false; ///< 是否生成调试信息
            bool                      enableOptimization = true; ///< 是否启用优化 (-O3)
            bool                      enable16BitTypes   = true; ///< 是否启用 16-bit 类型
            bool                      enableBindless     = true; ///< 是否启用 Bindless 支持 (Shader Model 6.6)
        };

        /**
         * @struct CompileResult
         * @brief DXC 编译结果 - 简化版
         *
         * 教学要点:
         * 1. 只包含编译器的直接输出 (字节码 + 错误信息)
         * 2. 不包含着色器元数据 (由 Resource/CompiledShader.hpp 负责)
         * 3. 职责分离: DXCCompiler 只负责 HLSL → DXIL 编译
         */
        struct CompileResult
        {
            std::vector<uint8_t> bytecode; ///< DXIL 字节码
            std::string          errorMessage; ///< 编译错误信息（如果失败）
            std::string          warningMessage; ///< 编译警告信息
            bool                 success = false; ///< 编译是否成功

            /**
             * @brief 获取字节码指针
             */
            const void* GetBytecodePtr() const { return bytecode.data(); }

            /**
             * @brief 获取字节码大小
             */
            size_t GetBytecodeSize() const { return bytecode.size(); }

            /**
             * @brief 是否有编译警告
             */
            bool HasWarnings() const { return !warningMessage.empty(); }
        };

    public:
        DXCCompiler()  = default;
        ~DXCCompiler() = default;

        // 禁止拷贝和移动
        DXCCompiler(const DXCCompiler&)            = delete;
        DXCCompiler& operator=(const DXCCompiler&) = delete;

        /**
         * @brief 初始化 DXC 编译器
         * @return 成功返回 true
         *
         * 教学要点:
         * 1. 使用 DxcCreateInstance 创建编译器实例
         * 2. 创建 IDxcUtils 用于编码转换
         * 3. 创建默认 Include Handler
         */
        bool Initialize();

        /**
         * @brief 编译 HLSL 着色器
         * @param source HLSL 源码字符串
         * @param options 编译选项
         * @return 编译结果 (CompileResult)
         *
         * 教学要点:
         * 1. DXC 编译参数:
         *    - -HV 2021: 使用 HLSL 2021 语法
         *    - -enable-16bit-types: 启用 half/min16float
         *    - -O3: 最高级别优化
         *    - -Zi: 生成调试信息
         * 2. 错误处理:
         *    - IDxcResult::GetStatus() 检查编译状态
         *    - IDxcBlobUtf8 提取错误消息
         * 3. 字节码提取:
         *    - DXC_OUT_OBJECT 获取 DXIL 字节码
         * 4. ❌ 不提取 DXC_OUT_REFLECTION（简化设计）
         *
         * 职责分离:
         * - DXCCompiler: 纯编译器，只返回字节码 + 错误信息
         * - ShaderProgramBuilder: 协调编译流程，创建 CompiledShader
         * - CompiledShader (Resource/): 完整着色器元数据容器
         *
         * 示例:
         * @code
         * DXCCompiler compiler;
         * compiler.Initialize();
         *
         * CompileOptions opts;
         * opts.entryPoint = "PSMain";
         * opts.target = "ps_6_6";
         * opts.defines.push_back("USE_BINDLESS=1");
         *
         * auto result = compiler.CompileShader(hlslSource, opts);
         * if (result.success) {
         *     // 使用 result.bytecode 创建 PSO
         * }
         * @endcode
         */
        CompileResult CompileShader(const std::string& source, const CompileOptions& options);

        /**
         * @brief 从文件编译 HLSL 着色器
         * @param filePath HLSL 文件路径
         * @param options 编译选项
         * @return 编译结果 (CompileResult)
         */
        CompileResult CompileShaderFromFile(const std::wstring& filePath, const CompileOptions& options);

        /**
         * @brief 检查是否已初始化
         */
        bool IsInitialized() const { return m_compiler != nullptr; }

    private:
        /**
         * @brief 构建 DXC 编译参数
         * @param options 编译选项
         * @return 参数字符串数组
         *
         * 教学要点:
         * 1. 参数顺序:
         *    - 入口点: -E <entryPoint>
         *    - 目标: -T <target>
         *    - 宏定义: -D <name>=<value>
         *    - 优化: -O0/-O3
         *    - 调试: -Zi -Qembed_debug
         * 2. Shader Model 6.6 必需参数:
         *    - -HV 2021: 新语法特性
         *    - -enable-16bit-types: 16-bit 类型支持
         * 3. Bindless 支持:
         *    - 无需特殊编译参数，通过 HLSL 语法实现
         */
        std::vector<std::wstring> BuildCompileArgs(const CompileOptions& options);

        /**
         * @brief 提取编译错误/警告信息
         * @param result DXC 编译结果
         * @return 错误/警告消息
         */
        std::string ExtractErrorMessage(IDxcResult* result);

    private:
        Microsoft::WRL::ComPtr<IDxcCompiler3>      m_compiler; ///< DXC 编译器实例
        Microsoft::WRL::ComPtr<IDxcUtils>          m_utils; ///< DXC 工具类（编码转换）
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> m_includeHandler; ///< Include 文件处理器
    };
} // namespace enigma::graphic
