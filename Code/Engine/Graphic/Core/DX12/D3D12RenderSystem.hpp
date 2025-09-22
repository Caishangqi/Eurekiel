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
        // Instantiation prohibited - Static tool class
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
         * @brief Create framebuffer (corresponding to Iris' createFramebuffer)
         * @return Newly created framebuffer object
         *
         * TODO: Implement DirectX 12 equivalent operations
         * In DirectX 12, RTV and DSV descriptors may be created correspondingly
         */
        // static FramebufferHandle CreateFramebuffer();

        // ====================================================================================================
        // Synchronization and memory management (delegated to CommandListManager)
        // ====================================================================================================

        /**
         * @brief Wait for the GPU to complete all work (delegated to CommandListManager)
         *
         * TODO: Delegated to CommandListManager::WaitForGPU()
         * Avoid repeated fence management logic in D3D12RenderSystem
         */
        static void WaitForGPU();

        /**
          * @brief Get GPU video memory usage (corresponding to Iris' getVRAM)
          * @return Current available video memory size (bytes)
          *
          * TODO: Implement VRAM query, through:
          * - DXGI adapter query
          * - DirectX 12 Memory Budget API
          */
        static uint64_t GetAvailableVRAM();

        /**
          * @brief Get GPU memory budget information
          * @return GPU memory budget (bytes)
          */
        static uint64_t GetVRAMBudget();

        // ====================================================================================================
        // Debugging and verification (corresponding to Iris' debugging function)
        // ====================================================================================================

        /**
         * @brief Enable/disable debug layer
         * @param enable true means enable debug information
         * @details corresponds to debug state management in Iris
         */
        static void EnableDebugLayer(bool enable);

        /**
         * @brief Check whether the debug layer is enabled
         * @return true means that the debug layer is enabled
         */
        static bool IsDebugLayerEnabled() noexcept { return s_debugLayerEnabled; }

        /**
         * @brief Set the debug object name
         * @param object DirectX object pointer
         * @param name Debug name
         * @details for object recognition in debugging tools
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
          * @brief Verify device status
          * @return true means the device is in normal state
          * @details Check device removal and other error status
          */
        static bool ValidateDeviceState();

        // ====================================================================================================
        // Thread safety check (corresponding to Iris' RenderSystem.assertOnRenderThreadOrInit)
        // ====================================================================================================

        /**
         * @brief asserts that the rendering thread is currently in
         * @details
         * Corresponding to RenderSystem.assertOnRenderThreadOrInit() in Iris
         * Make sure DirectX calls are executed on the correct thread
         */
        static void AssertOnRenderThread();

        /**
         * @brief Check whether the rendering thread is
         * @return true means rendering thread
         */
        static bool IsOnRenderThread() noexcept;

    private:
        // ====================================================================================================
        // Internal initialization method
        // ====================================================================================================

        /**
         * @brief Create DirectX 12 devices
         * @param enableDebugLayer Whether to enable debug layer
         */
        static void CreateDevice(bool enableDebugLayer);

        /**
         * @brief Initialize CommandListManager
         * @details Initialize the global CommandListManager with the created device
         *
         * 教学要点：
         * CommandListManager负责：
         * 1. 创建Graphics/Compute/Copy三种命令队列
         * 2. 创建围栏对象和事件句柄
         * 3. 初始化命令列表池
         * 4. 管理命令列表的生命周期
         *
         * D3D12RenderSystem只需要：
         * 1. 提供Device给CommandListManager
         * 2. 调用Initialize方法
         * 3. 避免职责重叠
         */
        static void InitializeCommandListManager();

        /**
         * @brief Detection of GPU capabilities
         * @details Query various features supported by the device
         */
        static void DetectGPUCapabilities();

        /**
         * @brief 验证Bindless管线最低要求
         * @details 检查GPU是否支持现代Bindless资源访问和Shader Model 6.0+
         * @throws std::runtime_error 如果GPU不满足最低要求
         *
         * 强制要求:
         * - Shader Model 6.0+ (Bindless资源访问)
         * - Resource Binding Tier 2+ (无限制描述符数组)
         * - Compute Shaders (延迟渲染光照计算)
         *
         * 推荐特性:
         * - Mesh Shaders (现代几何管线)
         * - Variable Rate Shading (性能优化)
         * - DirectX Raytracing (高级光照效果)
         */
        static void ValidateBindlessRequirements();

        /**
          * @brief Enable debugging and verification layers
          */
        static void EnableDebugAndValidationLayers(bool enableGPUValidation);

        /**
         * @brief 获取GPU描述信息
         * @return GPU设备描述字符串
         * @details 用于日志记录和调试信息显示
         */
        static std::string GetGPUDescription();

    private:
        // ========================================================================
        // Static member variables - manage only core device objects to avoid duplication with CommandListManager
        // ========================================================================

        /// DirectX 12 device - the core of all resource creation
        static Microsoft::WRL::ComPtr<ID3D12Device> s_device;

        /// DXGI Factory - for enumerating adapters and creating swap chains
        static Microsoft::WRL::ComPtr<IDXGIFactory6> s_dxgiFactory;

        /// CommandListManager - Responsible for all command queues, fences and command list management
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
