/**
 * @file DXCCompiler.cpp
 * @brief 简化版 DXC 编译器实现
 * @date 2025-10-03
 */

#include "DXCCompiler.hpp"
#include <combaseapi.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace enigma::graphic
{
    // ============================================================================
    // 初始化
    // ============================================================================

    bool DXCCompiler::Initialize()
    {
        HRESULT hr;

        // 1. 创建 DXC 编译器实例
        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler));
        if (FAILED(hr))
        {
            std::cerr << "[DXCCompiler] Failed to create DXC compiler: HRESULT = 0x" << std::hex << hr << std::dec
                << std::endl;
            return false;
        }

        // 2. 创建 DXC 工具类（用于编码转换）
        hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils));
        if (FAILED(hr))
        {
            std::cerr << "[DXCCompiler] Failed to create DXC Utils: HRESULT = 0x" << std::hex << hr << std::dec
                << std::endl;
            return false;
        }

        // 3. 创建默认 Include Handler
        hr = m_utils->CreateDefaultIncludeHandler(&m_includeHandler);
        if (FAILED(hr))
        {
            std::cerr << "[DXCCompiler] Failed to create Include Handler: HRESULT = 0x" << std::hex << hr << std::dec
                << std::endl;
            return false;
        }

        std::cout << "[DXCCompiler] Initialization is successful" << std::endl;
        return true;
    }

    // ============================================================================
    // 编译核心
    // ============================================================================

    DXCCompiler::CompileResult DXCCompiler::CompileShader(const std::string&    source,
                                                          const CompileOptions& options)
    {
        CompileResult result;

        if (!IsInitialized())
        {
            result.errorMessage = "DXCCompiler not initialized";
            return result;
        }

        // 1. 创建源码 Blob
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
        HRESULT                                  hr = m_utils->CreateBlob(source.c_str(),
                                         static_cast<UINT32>(source.size()),
                                         CP_UTF8,
                                         &sourceBlob);

        if (FAILED(hr))
        {
            result.errorMessage = "创建源码 Blob 失败";
            return result;
        }

        // 2. 构建编译参数
        std::vector<std::wstring> argsStorage = BuildCompileArgs(options);
        std::vector<LPCWSTR>      args;
        args.reserve(argsStorage.size());
        for (const auto& arg : argsStorage)
        {
            args.push_back(arg.c_str());
        }

        // 3. 设置编译参数结构
        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr      = sourceBlob->GetBufferPointer();
        sourceBuffer.Size     = sourceBlob->GetBufferSize();
        sourceBuffer.Encoding = CP_UTF8;

        // 4. 调用 DXC 编译
        Microsoft::WRL::ComPtr<IDxcResult> compileResult;
        hr = m_compiler->Compile(&sourceBuffer, // 源码
                                 args.data(), // 编译参数
                                 static_cast<UINT32>(args.size()),
                                 m_includeHandler.Get(), // Include Handler
                                 IID_PPV_ARGS(&compileResult));

        if (FAILED(hr))
        {
            result.errorMessage = "DXC compile call failed";
            return result;
        }

        // 5. 检查编译状态
        HRESULT compileStatus;
        hr = compileResult->GetStatus(&compileStatus);
        if (FAILED(hr))
        {
            result.errorMessage = "Failed to get compile status";
            return result;
        }

        // 6. 提取错误/警告信息
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        hr = compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (SUCCEEDED(hr) && errors && errors->GetStringLength() > 0)
        {
            std::string message(errors->GetStringPointer(), errors->GetStringLength());

            if (FAILED(compileStatus))
            {
                result.errorMessage = message;
            }
            else
            {
                result.warningMessage = message;
            }
        }

        // 7. 如果编译失败，返回
        if (FAILED(compileStatus))
        {
            result.success = false;
            return result;
        }

        // 8. 提取编译字节码
        Microsoft::WRL::ComPtr<IDxcBlob> bytecodeBlob;
        hr = compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecodeBlob), nullptr);
        if (FAILED(hr) || !bytecodeBlob)
        {
            result.errorMessage = "Failed to get compiled bytecode";
            return result;
        }

        // 9. 拷贝字节码到结果
        const uint8_t* bytecodeData = static_cast<const uint8_t*>(bytecodeBlob->GetBufferPointer());
        size_t         bytecodeSize = bytecodeBlob->GetBufferSize();
        result.bytecode.assign(bytecodeData, bytecodeData + bytecodeSize);

        // ❌ 移除: 不提取 DXC_OUT_REFLECTION（简化设计）
        // 原因: 使用固定 Input Layout，Bindless 通过 Root Constants 传递资源索引

        result.success = true;
        return result;
    }

    DXCCompiler::CompileResult DXCCompiler::CompileShaderFromFile(const std::wstring&   filePath,
                                                                  const CompileOptions& options)
    {
        CompileResult result;

        // 1. 读取文件内容
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            // 使用 WideCharToMultiByte 转换宽字符串为窄字符串
            int         sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0, NULL, NULL);
            std::string narrowPath(sizeNeeded, 0);
            WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, &narrowPath[0], sizeNeeded, NULL, NULL);
            narrowPath.resize(sizeNeeded - 1); // 移除 null terminator

            result.errorMessage = "Unable to open the file: " + narrowPath;
            return result;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string source;
        source.resize(static_cast<size_t>(size));
        if (!file.read(source.data(), size))
        {
            // 使用 WideCharToMultiByte 转换宽字符串为窄字符串
            int         sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0, NULL, NULL);
            std::string narrowPath(sizeNeeded, 0);
            WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, &narrowPath[0], sizeNeeded, NULL, NULL);
            narrowPath.resize(sizeNeeded - 1); // 移除 null terminator

            result.errorMessage = "Failed to read the file: " + narrowPath;
            return result;
        }

        // 2. 调用编译
        return CompileShader(source, options);
    }

    DXCCompiler::CompileResult DXCCompiler::CompileFromMemory(
        const std::vector<uint8_t>& hlslData,
        const std::string&          shaderName,
        const CompileOptions&       options)
    {
        CompileResult result;

        if (!IsInitialized())
        {
            result.errorMessage = "DXCCompiler not initialized";
            return result;
        }

        // 验证输入数据
        if (hlslData.empty())
        {
            result.errorMessage = "Empty HLSL data for shader: " + shaderName;
            return result;
        }

        // 1. 从二进制数据创建 Blob
        // 教学要点: IDxcUtils::CreateBlob() 可以直接从内存数据创建 Blob
        // 参数:
        //   - pData: 数据指针 (IResource::m_data.data())
        //   - size: 数据大小 (IResource::m_data.size())
        //   - codePage: CP_UTF8 (HLSL 源码编码)
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
        HRESULT                                  hr = m_utils->CreateBlob(
            hlslData.data(), // 从资源系统加载的二进制数据
            static_cast<UINT32>(hlslData.size()),
            CP_UTF8, // HLSL 源码使用 UTF-8 编码
            &sourceBlob
        );

        if (FAILED(hr))
        {
            result.errorMessage = "Failed to create Blob from memory for shader: " + shaderName;
            return result;
        }

        // 2. 构建编译参数
        std::vector<std::wstring> argsStorage = BuildCompileArgs(options);
        std::vector<LPCWSTR>      args;
        args.reserve(argsStorage.size());
        for (const auto& arg : argsStorage)
        {
            args.push_back(arg.c_str());
        }

        // 3. 设置编译参数结构
        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr      = sourceBlob->GetBufferPointer();
        sourceBuffer.Size     = sourceBlob->GetBufferSize();
        sourceBuffer.Encoding = CP_UTF8;

        // 4. 调用 DXC 编译
        Microsoft::WRL::ComPtr<IDxcResult> compileResult;
        hr = m_compiler->Compile(
            &sourceBuffer, // 源码 Blob
            args.data(), // 编译参数
            static_cast<UINT32>(args.size()),
            m_includeHandler.Get(), // Include Handler
            IID_PPV_ARGS(&compileResult)
        );

        if (FAILED(hr))
        {
            result.errorMessage = "DXC compile call failed for shader: " + shaderName;
            return result;
        }

        // 5. 检查编译状态
        HRESULT compileStatus;
        hr = compileResult->GetStatus(&compileStatus);
        if (FAILED(hr))
        {
            result.errorMessage = "Failed to get compile status for shader: " + shaderName;
            return result;
        }

        // 6. 提取错误/警告信息
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        hr = compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (SUCCEEDED(hr) && errors && errors->GetStringLength() > 0)
        {
            std::string message(errors->GetStringPointer(), errors->GetStringLength());

            if (FAILED(compileStatus))
            {
                result.errorMessage = "[" + shaderName + "] " + message;
            }
            else
            {
                result.warningMessage = "[" + shaderName + "] " + message;
            }
        }

        // 7. 如果编译失败，返回
        if (FAILED(compileStatus))
        {
            result.success = false;
            return result;
        }

        // 8. 提取编译字节码
        Microsoft::WRL::ComPtr<IDxcBlob> bytecodeBlob;
        hr = compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecodeBlob), nullptr);
        if (FAILED(hr) || !bytecodeBlob)
        {
            result.errorMessage = "Failed to get compiled bytecode for shader: " + shaderName;
            return result;
        }

        // 9. 拷贝字节码到结果
        const uint8_t* bytecodeData = static_cast<const uint8_t*>(bytecodeBlob->GetBufferPointer());
        size_t         bytecodeSize = bytecodeBlob->GetBufferSize();
        result.bytecode.assign(bytecodeData, bytecodeData + bytecodeSize);

        result.success = true;

        // 日志输出 (可选)
        std::cout << "[DXCCompiler] Successfully compiled shader from memory: " << shaderName
            << " (" << result.bytecode.size() << " bytes)" << std::endl;

        return result;
    }

    // ============================================================================
    // 私有辅助方法
    // ============================================================================

    std::vector<std::wstring> DXCCompiler::BuildCompileArgs(const CompileOptions& options)
    {
        std::vector<std::wstring> args;

        // 1. 入口点
        args.push_back(L"-E");
        args.push_back(std::wstring(options.entryPoint.begin(), options.entryPoint.end()));

        // 2. 编译目标
        args.push_back(L"-T");
        args.push_back(std::wstring(options.target.begin(), options.target.end()));

        // 3. HLSL 版本（使用 2021 语法）
        args.push_back(L"-HV");
        args.push_back(L"2021");

        // 4. 16-bit 类型支持
        if (options.enable16BitTypes)
        {
            args.push_back(L"-enable-16bit-types");
        }

        // 5. 优化级别
        if (options.enableOptimization)
        {
            args.push_back(L"-O3"); // 最高级别优化
        }
        else
        {
            args.push_back(L"-O0"); // 无优化（调试）
        }

        // 6. 调试信息
        if (options.enableDebugInfo)
        {
            args.push_back(L"-Zi"); // 生成调试信息
            args.push_back(L"-Qembed_debug"); // 嵌入调试信息
        }

        // 7. 宏定义
        for (const auto& define : options.defines)
        {
            std::wstring wDefine = L"-D";
            wDefine += std::wstring(define.begin(), define.end());
            args.push_back(wDefine);
        }

        // 8. Include 路径
        for (const auto& includePath : options.includePaths)
        {
            args.push_back(L"-I");
            args.push_back(includePath);
        }

        // 9. Shader Model 6.6 特性（Bindless 支持）
        // 注意: Bindless 通过 HLSL 语法实现，无需特殊编译参数
        // 示例: ResourceDescriptorHeap[index] 直接访问

        return args;
    }

    std::string DXCCompiler::ExtractErrorMessage(IDxcResult* result)
    {
        if (!result)
            return "";

        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        HRESULT                              hr = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

        if (SUCCEEDED(hr) && errors && errors->GetStringLength() > 0)
        {
            return std::string(errors->GetStringPointer(), errors->GetStringLength());
        }

        return "";
    }
} // namespace enigma::graphic
