#pragma once

// 基于 Iris 源码: net.irisshaders.iris.gl.IrisRenderSystem (引用)
// 文档验证: 2025-09-15 通过本地源码分析确认存在
// 
// 教学要点:
// 1. 在Iris中IrisRenderSystem是全局静态API封装层
// 2. 负责抽象OpenGL调用并确保线程安全
// 3. 提供DSA支持和GPU能力检测
// 4. 对应我们DirectX 12的底层API抽象

#include <memory>
#include <string>
#include <array>
#include <thread>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <d3d12sdklayers.h>

// 包含现有的Enigma资源封装类
#include "../../Resource/CommandListManager.hpp"
#include "../../Resource/D12Resources.hpp"

namespace enigma::graphic
{
    // 前向声明现有封装类
    class CommandListManager;
    class D12Resource;

    /**
     * @brief DirectX 12渲染系统API封装
     * 
     * 对应 Iris 中的 IrisRenderSystem 静态类，作为底层图形API的抽象层
     * 负责所有DirectX 12 API调用的封装和管理，包括：
     * - 设备和命令队列的全局管理
     * - API调用的线程安全检查
     * - GPU能力检测和特性支持
     * - 资源创建和状态管理的统一接口
     * 
     * TODO: 基于 Iris IrisRenderSystem 实现分析完整的API封装逻辑
     * - 实现 InitializeRenderer() 方法 (对应 Iris 的 initRenderer())
     * - 支持GPU能力检测 (对应 supportsCompute, supportsTesselation 等)
     * - 提供统一的资源创建接口
     * - 集成调试和验证层管理
     */
    class D3D12RenderSystem final
    {
    private:
        // 禁止实例化 - 静态工具类，对应Iris的设计
        D3D12RenderSystem()                                    = delete;
        ~D3D12RenderSystem()                                   = delete;
        D3D12RenderSystem(const D3D12RenderSystem&)            = delete;
        D3D12RenderSystem& operator=(const D3D12RenderSystem&) = delete;

    public:
        // ========================================================================
        // 系统初始化和管理 (对应Iris的initRenderer等)
        // ========================================================================

        /**
         * @brief 初始化DirectX 12渲染器 (对应Iris的initRenderer)
         * @param enableDebugLayer 是否启用调试层
         * @param enableGPUValidation 是否启用GPU验证
         * 
         * TODO: 实现渲染器初始化，包括：
         * - 创建DXGI工厂和选择适配器
         * - 初始化DirectX 12设备
         * - 创建命令队列
         * - 检测GPU特性支持
         * - 设置调试和验证层
         */
        static void InitializeRenderer(bool enableDebugLayer = false, bool enableGPUValidation = false);

        /**
         * @brief 关闭渲染器并释放所有资源
         * 
         * TODO: 实现安全的资源释放：
         * - 等待GPU完成所有工作
         * - 释放所有DirectX对象
         * - 关闭调试层
         */
        static void ShutdownRenderer();

        /**
         * @brief 检查渲染器是否已初始化
         * @return true表示渲染器可用
         */
        static bool IsInitialized() noexcept { return s_isInitialized; }

        // ========================================================================
        // 设备和命令队列访问 (全局静态访问)
        // ========================================================================

        /**
         * @brief 获取DirectX 12设备
         * @return D3D12设备指针
         * @details 
         * 对应Iris中通过IrisRenderSystem访问OpenGL上下文
         * 提供全局设备访问，其他组件可以直接获取而无需通过RendererSubsystem
         */
        static ID3D12Device* GetDevice() noexcept { return s_device.Get(); }

        /**
         * @brief 获取命令列表管理器
         * @return 全局命令列表管理器指针
         * @details 
         * 命令队列和围栏管理已移至CommandListManager
         * 这样避免了职责重叠，符合单一职责原则
         */
        static CommandListManager* GetCommandListManager() noexcept { return s_commandListManager.get(); }

        /**
         * @brief 获取DXGI工厂
         * @return DXGI工厂指针
         * @details 用于交换链创建和适配器枚举
         */
        static IDXGIFactory6* GetDXGIFactory() noexcept { return s_dxgiFactory.Get(); }

        // ========================================================================
        // GPU能力检测 (对应Iris的各种supports方法)
        // ========================================================================

        /**
         * @brief 检查是否支持DirectStorage (对应Iris的supportsSSBO)
         * @return true表示支持DirectStorage
         * 
         * TODO: 实现DirectStorage支持检测
         */
        static bool SupportsDirectStorage() noexcept { return s_supportsDirectStorage; }

        /**
         * @brief 检查是否支持Bindless资源 (对应Iris的supportsImageLoadStore)
         * @return true表示支持Bindless描述符
         * 
         * TODO: 实现Bindless资源支持检测
         */
        static bool SupportsBindlessResources() noexcept { return s_supportsBindless; }

        /**
         * @brief 检查是否支持计算着色器 (对应Iris的supportsCompute)
         * @return true表示支持计算着色器
         */
        static bool SupportsComputeShaders() noexcept { return s_supportsCompute; }

        /**
         * @brief 检查是否支持网格着色器 (对应Iris的supportsTesselation)
         * @return true表示支持网格着色器
         * 
         * TODO: 实现网格着色器支持检测
         */
        static bool SupportsMeshShaders() noexcept { return s_supportsMeshShaders; }

        /**
         * @brief 检查是否支持光线追踪
         * @return true表示支持DXR
         */
        static bool SupportsRayTracing() noexcept { return s_supportsRayTracing; }

        /**
         * @brief 检查是否支持可变率着色 (Variable Rate Shading)
         * @return true表示支持VRS
         */
        static bool SupportsVariableRateShading() noexcept { return s_supportsVRS; }

        /**
         * @brief 获取支持的着色器模型版本
         * @return 着色器模型版本 (如 6.0, 6.1, 6.6 等)
         */
        static float GetShaderModelVersion() noexcept { return s_shaderModelVersion; }

        // ========================================================================
        // 高级资源创建接口 (返回Enigma封装类，不是原生DirectX对象)
        // ========================================================================

        /**
         * @brief 创建缓冲区资源 (返回Enigma封装)
         * @param size 缓冲区大小（字节）
         * @param usage 资源使用标志
         * @param initialState 初始资源状态
         * @return 创建的D12Buffer封装对象
         * 
         * TODO: 实现统一的缓冲区创建，支持：
         * - 顶点缓冲区、索引缓冲区
         * - 常量缓冲区
         * - 结构化缓冲区  
         * - UAV缓冲区
         * 
         * 注意：返回Enigma的D12Buffer而不是原生ID3D12Resource
         */
        static std::unique_ptr<class D12Buffer> CreateBuffer(
            uint64_t              size,
            D3D12_RESOURCE_FLAGS  usage        = D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

        /**
         * @brief 创建2D纹理资源 (返回Enigma封装)
         * @param width 纹理宽度
         * @param height 纹理高度
         * @param format 像素格式
         * @param mipLevels Mip级别数量
         * @param usage 资源使用标志
         * @param initialState 初始资源状态
         * @return 创建的D12Texture封装对象
         * 
         * TODO: 实现统一的纹理创建，支持：
         * - 渲染目标纹理
         * - 深度模板纹理
         * - 着色器资源纹理
         * - UAV纹理
         * 
         * 注意：返回Enigma的D12Texture而不是原生ID3D12Resource
         */
        static std::unique_ptr<class D12Texture> CreateTexture2D(
            uint32_t              width, uint32_t height,
            DXGI_FORMAT           format,
            uint16_t              mipLevels    = 1,
            D3D12_RESOURCE_FLAGS  usage        = D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

        /**
         * @brief 创建帧缓冲区 (对应Iris的createFramebuffer)
         * @return 新创建的帧缓冲区对象
         * 
         * TODO: 实现DirectX 12的等效操作
         * 在DirectX 12中可能对应创建RTV和DSV描述符
         */
        // static FramebufferHandle CreateFramebuffer();

        // ========================================================================
        // 同步和内存管理 (委托给CommandListManager)
        // ========================================================================

        /**
         * @brief 等待GPU完成所有工作 (委托给CommandListManager)
         * 
         * TODO: 委托给CommandListManager::WaitForGPU()
         * 避免在D3D12RenderSystem中重复围栏管理逻辑
         */
        static void WaitForGPU();

        /**
         * @brief 获取GPU视频内存使用情况 (对应Iris的getVRAM)
         * @return 当前可用视频内存大小（字节）
         * 
         * TODO: 实现VRAM查询，通过：
         * - DXGI适配器查询
         * - DirectX 12内存预算API
         */
        static uint64_t GetAvailableVRAM();

        /**
         * @brief 获取GPU内存预算信息
         * @return GPU内存预算（字节）
         */
        static uint64_t GetVRAMBudget();

        // ========================================================================
        // 调试和验证 (对应Iris的调试功能)
        // ========================================================================

        /**
         * @brief 启用/禁用调试层
         * @param enable true表示启用调试信息
         * @details 对应Iris中的调试状态管理
         */
        static void EnableDebugLayer(bool enable);

        /**
         * @brief 检查是否启用了调试层
         * @return true表示调试层已启用
         */
        static bool IsDebugLayerEnabled() noexcept { return s_debugLayerEnabled; }

        /**
         * @brief 设置调试对象名称
         * @param object DirectX对象指针
         * @param name 调试名称
         * @details 用于调试工具中的对象识别
         */
        template <typename T>
        static void SetDebugName(T* object, const std::wstring& name)
        {
            if (s_debugLayerEnabled && object)
            {
                object->SetName(name.c_str());
            }
        }

        /**
         * @brief 验证设备状态
         * @return true表示设备状态正常
         * @details 检查设备移除等错误状态
         */
        static bool ValidateDeviceState();

        // ========================================================================
        // 线程安全检查 (对应Iris的RenderSystem.assertOnRenderThreadOrInit)
        // ========================================================================

        /**
         * @brief 断言当前在渲染线程
         * @details 
         * 对应Iris中的RenderSystem.assertOnRenderThreadOrInit()
         * 确保DirectX调用在正确的线程上执行
         */
        static void AssertOnRenderThread();

        /**
         * @brief 检查是否在渲染线程
         * @return true表示在渲染线程
         */
        static bool IsOnRenderThread() noexcept;

    private:
        // ========================================================================
        // 内部初始化方法
        // ========================================================================

        /**
         * @brief 创建DirectX 12设备
         * @param enableDebugLayer 是否启用调试层
         */
        static void CreateDevice(bool enableDebugLayer);

        /**
         * @brief 创建命令队列 (初始化CommandListManager时调用)
         * @details 创建图形、计算、拷贝三个队列，并传递给CommandListManager
         */
        static void CreateCommandQueues();

        /**
         * @brief 初始化CommandListManager
         * @details 使用创建的设备和命令队列初始化全局CommandListManager
         */
        static void InitializeCommandListManager();

        /**
         * @brief 检测GPU能力
         * @details 查询设备支持的各种特性
         */
        static void DetectGPUCapabilities();

        /**
         * @brief 启用调试和验证层
         */
        static void EnableDebugAndValidationLayers(bool enableGPUValidation);

    private:
        // ========================================================================
        // 静态成员变量 - 只管理核心设备对象，避免与CommandListManager重复
        // ========================================================================

        /// DirectX 12设备 - 所有资源创建的核心
        static Microsoft::WRL::ComPtr<ID3D12Device> s_device;

        /// DXGI工厂 - 用于枚举适配器和创建交换链
        static Microsoft::WRL::ComPtr<IDXGIFactory6> s_dxgiFactory;

        /// CommandListManager - 负责所有命令队列、围栏和命令列表管理
        static std::unique_ptr<CommandListManager> s_commandListManager;

        /// 调试接口
        static Microsoft::WRL::ComPtr<ID3D12Debug3>       s_debugController;
        static Microsoft::WRL::ComPtr<ID3D12DebugDevice2> s_debugDevice;

        // ========================================================================
        // GPU能力标志 - 对应Iris的各种能力检测
        // ========================================================================

        /// 系统初始化状态
        static bool s_isInitialized;

        /// 调试层状态
        static bool s_debugLayerEnabled;

        /// GPU能力支持标志
        static bool s_supportsDirectStorage;
        static bool s_supportsBindless;
        static bool s_supportsCompute;
        static bool s_supportsMeshShaders;
        static bool s_supportsRayTracing;
        static bool s_supportsVRS;

        /// 着色器模型版本
        static float s_shaderModelVersion;

        /// 渲染线程ID
        static std::thread::id s_renderThreadId;

        // 注意：命令队列、围栏、命令列表管理已移至CommandListManager
        // 不在此处重复管理，避免职责重叠
    };
} // namespace enigma::graphic
