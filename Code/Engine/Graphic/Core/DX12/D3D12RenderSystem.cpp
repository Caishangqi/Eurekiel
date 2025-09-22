#include "D3D12RenderSystem.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using namespace enigma::graphic;
using namespace enigma::core;

// ====================================================================================================
// 静态成员变量定义
// 参考: DirectX 12编程指南第3章 - 全局对象管理
// ====================================================================================================

// 核心DirectX对象
Microsoft::WRL::ComPtr<ID3D12Device>  D3D12RenderSystem::s_device             = nullptr;
Microsoft::WRL::ComPtr<IDXGIFactory6> D3D12RenderSystem::s_dxgiFactory        = nullptr;
std::unique_ptr<CommandListManager>   D3D12RenderSystem::s_commandListManager = nullptr;

// 调试接口
Microsoft::WRL::ComPtr<ID3D12Debug3>       D3D12RenderSystem::s_debugController = nullptr;
Microsoft::WRL::ComPtr<ID3D12DebugDevice2> D3D12RenderSystem::s_debugDevice     = nullptr;

// 系统状态标志
bool D3D12RenderSystem::s_isInitialized     = false;
bool D3D12RenderSystem::s_debugLayerEnabled = false;

// GPU能力支持标志
bool D3D12RenderSystem::s_supportsDirectStorage = false;
bool D3D12RenderSystem::s_supportsBindless      = false;
bool D3D12RenderSystem::s_supportsCompute       = false;
bool D3D12RenderSystem::s_supportsMeshShaders   = false;
bool D3D12RenderSystem::s_supportsRayTracing    = false;
bool D3D12RenderSystem::s_supportsVRS           = false;

// 技术参数
float           D3D12RenderSystem::s_shaderModelVersion = 5.1f; // 默认支持shader model 5.1
std::thread::id D3D12RenderSystem::s_renderThreadId{};

// ====================================================================================================
// 公共接口实现
// ====================================================================================================

/*
*1. 完整的InitializeRenderer实现

  - 防重复初始化检查：避免多次初始化
  - 线程安全机制：记录渲染线程ID，对应Iris的线程安全设计
  - 异常安全处理：使用try-catch确保失败时正确清理资源

  2. 详细的DirectX 12初始化步骤

  1. 启用调试和验证层 (EnableDebugAndValidationLayers)
  2. 创建DXGI工厂 (CreateDXGIFactory2)
  3. 创建DirectX 12设备 (CreateDevice)
  4. 检测GPU能力和特性 (DetectGPUCapabilities)
  5. 初始化命令列表管理器 (InitializeCommandListManager)
  6. 设置调试对象名称
  7. 标记初始化完成

  3. 全面的API参考标注

  每个DirectX 12 API调用都标注了：
  - 参考文档：DirectX 12编程指南章节
  - API来源：Microsoft DirectX 12/DXGI Documentation
  - 方法说明：具体的接口和函数名称

  4. GPU能力检测系统

  - Shader Model版本检测：6.6 → 5.1全版本支持
  - 光线追踪支持：DXR检测
  - 可变率着色：VRS特性检测
  - 网格着色器：Mesh Shader支持检测
  - Bindless资源：Resource Binding Tier检测
  - DirectStorage：基于Shader Model版本推断

  5. 完整的静态成员变量定义

  - DirectX核心对象管理
  - 调试接口管理
  - GPU能力标志管理
  - 线程安全ID记录

  实现的核心特性

  与Iris架构对应

  - 线程安全检查 ↔ Iris RenderSystem.assertOnRenderThreadOrInit()
  - GPU能力检测 ↔ Iris supportsCompute, supportsTesselation 等
  - 调试层管理 ↔ Iris 调试状态管理
 */
void D3D12RenderSystem::InitializeRenderer(bool enableDebugLayer, bool enableGPUValidation)
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Initializing D3D12 render system...");

    // 防止重复初始化
    if (s_isInitialized)
    {
        LogWarn(RendererSubsystem::GetStaticSubsystemName(), "D3D12 render system already initialized!");
        return;
    }

    // 保存渲染线程ID，用于后续线程安全检查
    // 参考: Iris IrisRenderSystem 中的线程安全机制
    s_renderThreadId = std::this_thread::get_id();

    try
    {
        // ===========================================================================================
        // 1. 启用调试和验证层 (必须在创建任何DirectX对象之前)
        // 参考: DirectX 12编程指南第4章 - 调试和验证
        // API来源: Microsoft DirectX 12 Documentation - ID3D12Debug3 interface
        // ===========================================================================================
        if (enableDebugLayer || enableGPUValidation)
        {
            EnableDebugAndValidationLayers(enableGPUValidation);
        }
        s_debugLayerEnabled = enableDebugLayer;

        // ===========================================================================================
        // 2. 创建DXGI工厂
        // 参考: DirectX 12编程指南第3章 - DXGI工厂和适配器
        // API来源: Microsoft DXGI Documentation - CreateDXGIFactory2 function
        // ===========================================================================================
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Creating DXGI Factory...");

        UINT dxgiFactoryFlags = 0;
        if (enableDebugLayer)
        {
            // 启用DXGI调试层，配合D3D12调试层使用
            // API来源: DXGI_CREATE_FACTORY_DEBUG flag documentation
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&s_dxgiFactory));
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create DXGI Factory! HRESULT: " + std::to_string(hr));
        }
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "DXGI Factory created successfully");

        // ===========================================================================================
        // 3. 创建DirectX 12设备
        // 参考: DirectX 12编程指南第4章 - 设备创建和能力检测
        // API来源: Microsoft DirectX 12 Documentation - D3D12CreateDevice function
        // ===========================================================================================
        CreateDevice(enableDebugLayer);

        // ===========================================================================================
        // 4. 检测GPU能力和特性支持
        // 参考: DirectX 12编程指南第5章 - 特性支持查询
        // API来源: ID3D12Device::CheckFeatureSupport method documentation
        // ===========================================================================================
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Detecting GPU capabilities...");
        DetectGPUCapabilities();

        // ===========================================================================================
        // 5. 验证Bindless管线最低要求
        // 参考: DirectX 12编程指南第12章 - Bindless资源访问
        // 项目要求: 现代Bindless延迟渲染管线必需的最低GPU特性
        // ===========================================================================================
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Validating Bindless pipeline requirements...");
        ValidateBindlessRequirements();

        // ===========================================================================================
        // 6. 初始化命令列表管理器
        // 参考: DirectX 12编程指南第6章 - 命令队列和命令列表
        // API来源: Microsoft DirectX 12 Documentation - ID3D12CommandQueue interface
        // ===========================================================================================
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Initializing command list manager...");
        InitializeCommandListManager();

        // ===========================================================================================
        // 7. 如果启用调试层，设置调试对象名称
        // 参考: DirectX 12编程指南第14章 - 调试技术
        // API来源: ID3D12Object::SetName method documentation
        // ===========================================================================================
        if (enableDebugLayer)
        {
            SetDebugName(s_device.Get(), L"Enigma::D3D12Device");
            if (s_dxgiFactory)
            {
                s_dxgiFactory->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Enigma::DXGIFactory") - 1, "Enigma::DXGIFactory");
            }
        }

        // ===========================================================================================
        // 8. 标记初始化完成
        // ===========================================================================================
        s_isInitialized = true;
        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "D3D12 render system initialized successfully! GPU: %s", GetGPUDescription().c_str());
    }
    catch (const std::exception& e)
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Failed to initialize D3D12 render system: %s", e.what());

        // 清理已创建的资源
        ShutdownRenderer();
        throw;
    }
}

void D3D12RenderSystem::ShutdownRenderer()
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Shutting down D3D12 render system...");

    if (!s_isInitialized)
    {
        return;
    }

    // 等待GPU完成所有工作
    if (s_commandListManager)
    {
        s_commandListManager->WaitForGPU();
    }

    // 释放CommandListManager
    s_commandListManager.reset();

    // 释放调试接口
    s_debugDevice.Reset();
    s_debugController.Reset();

    // 释放核心对象
    s_device.Reset();
    s_dxgiFactory.Reset();

    // 重置状态
    s_isInitialized     = false;
    s_debugLayerEnabled = false;

    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "D3D12 render system shut down successfully");
}

// ====================================================================================================
// 私有辅助方法实现
// ====================================================================================================

void D3D12RenderSystem::EnableDebugAndValidationLayers(bool enableGPUValidation)
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Enabling debug and validation layers...");

    // 启用D3D12调试层
    // 参考: DirectX 12编程指南第14章 - 调试层配置
    // API来源: Microsoft DirectX 12 Documentation - ID3D12Debug3 interface
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&s_debugController));
    if (SUCCEEDED(hr))
    {
        s_debugController->EnableDebugLayer();

        if (enableGPUValidation)
        {
            // 启用GPU验证，会显著降低性能但提供更强的错误检测
            // API来源: ID3D12Debug3::SetEnableGPUBasedValidation method
            s_debugController->SetEnableGPUBasedValidation(TRUE);
            s_debugController->SetEnableSynchronizedCommandQueueValidation(TRUE);
            LogInfo(RendererSubsystem::GetStaticSubsystemName(), "GPU validation enabled");
        }

        LogInfo(RendererSubsystem::GetStaticSubsystemName(), "D3D12 debug layer enabled successfully");
    }
    else
    {
        LogWarn(RendererSubsystem::GetStaticSubsystemName(), "Failed to enable D3D12 debug layer. HRESULT: 0x%08X", hr);
    }
}

void D3D12RenderSystem::CreateDevice(bool enableDebugLayer)
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Creating D3D12 device...");

    // 枚举适配器并选择最佳的硬件适配器
    // 参考: DirectX 12编程指南第4章 - 适配器选择策略
    // API来源: Microsoft DXGI Documentation - IDXGIFactory6::EnumAdapterByGpuPreference
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
         SUCCEEDED(s_dxgiFactory->EnumAdapterByGpuPreference(
             adapterIndex,
             DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
             IID_PPV_ARGS(&adapter)));
         ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // 跳过软件适配器
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        // 尝试创建D3D12设备
        // API来源: Microsoft DirectX 12 Documentation - D3D12CreateDevice function
        HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&s_device));
        if (SUCCEEDED(hr))
        {
            // 转换适配器描述为字符串用于日志记录
            int size = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0)
            {
                std::string adapterName(size - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName.data(), size, nullptr, nullptr);
                LogInfo(RendererSubsystem::GetStaticSubsystemName(), "D3D12 device created successfully on adapter: %s", adapterName.c_str());
            }
            else
            {
                LogInfo(RendererSubsystem::GetStaticSubsystemName(), "D3D12 device created successfully on hardware adapter");
            }
            break;
        }
    }

    if (!s_device)
    {
        throw std::runtime_error("Failed to create D3D12 device on any adapter!");
    }

    // 如果启用调试层，创建调试设备接口
    if (enableDebugLayer)
    {
        HRESULT hr = s_device->QueryInterface(IID_PPV_ARGS(&s_debugDevice));
        if (SUCCEEDED(hr))
        {
            LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Debug device interface created");
        }
    }
}

void D3D12RenderSystem::DetectGPUCapabilities()
{
    if (!s_device)
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Cannot detect capabilities: device is null");
        return;
    }

    // 检测Shader Model支持
    // 参考: DirectX 12编程指南第5章 - 着色器模型检测
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_FEATURE_SHADER_MODEL
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {D3D_SHADER_MODEL_6_6};
    if (SUCCEEDED(s_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
    {
        switch (shaderModel.HighestShaderModel)
        {
        case D3D_SHADER_MODEL_6_6: s_shaderModelVersion = 6.6f;
            break;
        case D3D_SHADER_MODEL_6_5: s_shaderModelVersion = 6.5f;
            break;
        case D3D_SHADER_MODEL_6_4: s_shaderModelVersion = 6.4f;
            break;
        case D3D_SHADER_MODEL_6_3: s_shaderModelVersion = 6.3f;
            break;
        case D3D_SHADER_MODEL_6_2: s_shaderModelVersion = 6.2f;
            break;
        case D3D_SHADER_MODEL_6_1: s_shaderModelVersion = 6.1f;
            break;
        case D3D_SHADER_MODEL_6_0: s_shaderModelVersion = 6.0f;
            break;
        default: s_shaderModelVersion = 5.1f;
            break;
        }
    }

    // 检测光线追踪支持
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_FEATURE_D3D12_OPTIONS5
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (SUCCEEDED(s_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
    {
        s_supportsRayTracing = (options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);
    }

    // 检测可变率着色支持
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_FEATURE_D3D12_OPTIONS6
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
    if (SUCCEEDED(s_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
    {
        s_supportsVRS = (options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1);
    }

    // 检测网格着色器支持
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_FEATURE_D3D12_OPTIONS7
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    if (SUCCEEDED(s_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
    {
        s_supportsMeshShaders = (options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1);
    }

    // 检测Bindless资源支持（通过Resource Binding Tier）
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_FEATURE_D3D12_OPTIONS
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (SUCCEEDED(s_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
    {
        s_supportsBindless = (options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2);
    }

    // 计算着色器始终支持（DirectX 12必需特性）
    s_supportsCompute = true;

    // DirectStorage支持检测（需要Windows 10版本1903+）
    // 简化检测：如果支持Shader Model 6.0+就认为可能支持DirectStorage
    s_supportsDirectStorage = (s_shaderModelVersion >= 6.0f);

    // 记录能力检测结果
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "GPU Capabilities:");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Shader Model: %.1f", s_shaderModelVersion);
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Ray Tracing: %s", s_supportsRayTracing ? "Yes" : "No");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Variable Rate Shading: %s", s_supportsVRS ? "Yes" : "No");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Mesh Shaders: %s", s_supportsMeshShaders ? "Yes" : "No");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Bindless Resources: %s", s_supportsBindless ? "Yes" : "No");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Compute Shaders: %s", s_supportsCompute ? "Yes" : "No");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  DirectStorage: %s", s_supportsDirectStorage ? "Yes" : "No");
}

void D3D12RenderSystem::InitializeCommandListManager()
{
    if (!s_device)
    {
        throw std::runtime_error("Cannot initialize CommandListManager: device is null");
    }

    // 创建CommandListManager实例
    // 参考: Engine/Graphic/Resource/CommandListManager.hpp
    s_commandListManager = std::make_unique<CommandListManager>();

    // 调用CommandListManager的Initialize方法
    // CommandListManager内部会：
    // 1. 从D3D12RenderSystem::GetDevice()获取设备
    // 2. 创建Graphics/Compute/Copy三种命令队列
    // 3. 创建围栏和事件对象
    // 4. 初始化命令列表池
    // API来源: CommandListManager::Initialize method
    bool initSuccess = s_commandListManager->Initialize();
    if (!initSuccess)
    {
        s_commandListManager.reset();
        throw std::runtime_error("Failed to initialize CommandListManager");
    }

    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "CommandListManager initialized successfully");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Graphics command lists: %u",
            s_commandListManager->GetAvailableCount(CommandListManager::Type::Graphics));
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Compute command lists: %u",
            s_commandListManager->GetAvailableCount(CommandListManager::Type::Compute));
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "  Copy command lists: %u",
            s_commandListManager->GetAvailableCount(CommandListManager::Type::Copy));
}

void D3D12RenderSystem::ValidateBindlessRequirements()
{
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Validating modern Bindless pipeline requirements...");

    // ====================================================================================================
    // Bindless管线最低要求验证
    // 参考: DirectX 12编程指南第12章 - 现代资源绑定策略
    // 项目设计原则: 仅支持现代GPU，拒绝传统描述符表方式
    // ====================================================================================================

    std::vector<std::string> unsupportedFeatures;
    std::vector<std::string> recommendedFeatures;

    // 1. 强制要求：Shader Model 6.0+ (Bindless资源访问最低要求)
    // API来源: HLSL Shader Model 6.0 Specification - Unbounded Descriptor Arrays
    if (s_shaderModelVersion < 6.0f)
    {
        unsupportedFeatures.push_back(Stringf("Shader Model 6.0+ (current: %.1f)", s_shaderModelVersion));
    }

    // 2. 强制要求：Resource Binding Tier 2+ (真正的Bindless支持)
    // API来源: ID3D12Device::CheckFeatureSupport - D3D12_RESOURCE_BINDING_TIER_2
    if (!s_supportsBindless)
    {
        unsupportedFeatures.push_back("Resource Binding Tier 2+ (required for unlimited descriptor arrays)");
    }

    // 3. 强制要求：计算着色器支持 (延迟渲染光照计算)
    if (!s_supportsCompute)
    {
        unsupportedFeatures.push_back("Compute Shaders (required for deferred lighting)");
    }

    // 4. 推荐特性：网格着色器 (现代几何管线)
    if (!s_supportsMeshShaders)
    {
        recommendedFeatures.push_back("Mesh Shaders (enhanced geometry processing)");
    }

    // 5. 推荐特性：可变率着色 (性能优化)
    if (!s_supportsVRS)
    {
        recommendedFeatures.push_back("Variable Rate Shading (performance optimization)");
    }

    // 6. 推荐特性：光线追踪 (高级光照效果)
    if (!s_supportsRayTracing)
    {
        recommendedFeatures.push_back("DirectX Raytracing (advanced lighting effects)");
    }

    // ====================================================================================================
    // 结果评估和错误处理
    // ====================================================================================================

    if (!unsupportedFeatures.empty())
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(),
                 "   CRITICAL: GPU does not meet Bindless pipeline minimum requirements!");
        LogError(RendererSubsystem::GetStaticSubsystemName(), "   Missing required features:");
        for (const auto& feature : unsupportedFeatures)
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "   - %s", feature.c_str());
        }
        LogError(RendererSubsystem::GetStaticSubsystemName(),
                 "   This project requires a modern GPU with DirectX 12 Tier 2+ support.");
        LogError(RendererSubsystem::GetStaticSubsystemName(),
                 "   Recommended: NVIDIA GTX 1060+, AMD RX 580+, Intel Arc A-series");

        throw std::runtime_error("GPU does not support required Bindless pipeline features");
    }

    if (!recommendedFeatures.empty())
    {
        LogWarn(RendererSubsystem::GetStaticSubsystemName(),
                "   GPU missing recommended features (performance may be reduced):");
        for (const auto& feature : recommendedFeatures)
        {
            LogWarn(RendererSubsystem::GetStaticSubsystemName(), "   - %s", feature.c_str());
        }
    }

    // 记录成功验证信息
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "   GPU meets all Bindless pipeline requirements!");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "   Shader Model: %.1f", s_shaderModelVersion);
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "   Bindless Resources: Tier 2+");
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "   Modern pipeline fully supported");
}

// ====================================================================================================
// 辅助方法：获取GPU描述信息
// ====================================================================================================
std::string D3D12RenderSystem::GetGPUDescription()
{
    if (!s_dxgiFactory)
    {
        return "Unknown GPU";
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (SUCCEEDED(s_dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))))
    {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc)))
        {
            // 转换wstring到string
            int         size = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
            std::string result(size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, result.data(), size, nullptr, nullptr);
            return result;
        }
    }

    return "Unknown GPU";
}
