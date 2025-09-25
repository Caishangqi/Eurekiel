#pragma once

#include "../../Resource/Buffer/D12Buffer.hpp"
#include "../../Resource/BindlessResourceTypes.hpp"
#include "../../Resource/CommandListManager.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>

namespace enigma::graphic
{
    class D12DepthTexture;
    class RendererSubsystem;
    class D12Texture;
    class BindlessResourceManager;
    class ShaderResourceBinder;
    struct TextureCreateInfo;
    struct DepthTextureCreateInfo;
    enum class TextureUsage : uint32_t;

    /**
     * 教学目标：了解现代图形API封装层的设计模式
     *
     * D3D12RenderSystem类设计说明：
     * - 对应Iris的IrisRenderSystem.java的DirectX 12实现
     * - 提供静态API封装，管理整个DirectX 12底层系统
     * - 职责范围：设备管理、命令队列管理、资源创建
     * - 包含CommandListManager管理，遵循IrisRenderSystem的完整职责
     * - 为RendererSubsystem提供底层DirectX 12 API封装
     *
     * DirectX 12 API参考：
     * - ID3D12Device: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12device
     * - D3D12CreateDevice: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
     */

    /**
     * D3D12RenderSystem：DirectX 12渲染系统的静态封装
     * 设计理念：对应Iris的IrisRenderSystem，管理完整的DirectX 12底层API
     * 职责范围：设备创建、命令队列管理、缓冲区管理、调试支持
     * 架构层次：底层API封装，为RendererSubsystem提供DirectX 12服务
     * 与Iris对应：IrisRenderSystem.java的DirectX 12版本实现
     */
    class D3D12RenderSystem
    {
    public:
        /**
         * 初始化DirectX 12渲染系统（设备和命令系统）
         * DirectX 12 API: D3D12CreateDevice, CreateDXGIFactory2
         * 对应IrisRenderSystem.initialize()方法的完整DirectX 12实现
         * @param enableDebugLayer 是否启用调试层
         * @param enableGPUValidation 是否启用GPU验证
         * @return 是否初始化成功
         */
        static bool Initialize(bool enableDebugLayer = true, bool enableGPUValidation = false);

        /**
         * 关闭渲染系统，释放所有资源
         * 包括设备、命令队列和CommandListManager
         */
        static void Shutdown();

        /**
         * 检查渲染系统是否已初始化
         * @return true表示系统已准备就绪
         */
        static bool IsInitialized() { return s_device != nullptr; }

        // ===== 缓冲区管理API（对应Iris的createBuffers等方法）=====

        /**
         * 创建缓冲区（主要方法）
         * 对应Iris的IrisRenderSystem.createBuffers()，但参数更丰富
         *
         * DirectX 12 API调用链：
         * 1. ID3D12Device::CreateCommittedResource() - 创建缓冲区资源
         * 2. ID3D12Resource::SetName() - 设置调试名称
         * 3. ID3D12Resource::Map() - 映射初始数据（如果提供）
         *
         * @param createInfo 缓冲区创建信息
         * @return 创建的D12Buffer智能指针，失败返回nullptr
         */
        static std::unique_ptr<D12Buffer> CreateBuffer(const BufferCreateInfo& createInfo);

        /**
         * 简化的创建顶点缓冲区方法
         * @param size 缓冲区大小
         * @param initialData 初始顶点数据（可为nullptr）
         * @param debugName 调试名称
         * @return 顶点缓冲区指针
         */
        static std::unique_ptr<D12Buffer> CreateVertexBuffer(
            size_t      size,
            const void* initialData = nullptr,
            const char* debugName   = "VertexBuffer");

        /**
         * 简化的创建索引缓冲区方法
         * @param size 缓冲区大小
         * @param initialData 初始索引数据（可为nullptr）
         * @param debugName 调试名称
         * @return 索引缓冲区指针
         */
        static std::unique_ptr<D12Buffer> CreateIndexBuffer(
            size_t      size,
            const void* initialData = nullptr,
            const char* debugName   = "IndexBuffer");

        /**
         * 简化的创建常量缓冲区方法
         * @param size 缓冲区大小（会自动对齐到256字节）
         * @param initialData 初始常量数据（可为nullptr）
         * @param debugName 调试名称
         * @return 常量缓冲区指针
         */
        static std::unique_ptr<D12Buffer> CreateConstantBuffer(
            size_t      size,
            const void* initialData = nullptr,
            const char* debugName   = "ConstantBuffer");

        /**
         * 创建结构化缓冲区（对应Iris的SSBO）
         * @param elementCount 元素数量
         * @param elementSize 单个元素大小
         * @param initialData 初始数据（可为nullptr）
         * @param debugName 调试名称
         * @return 结构化缓冲区指针
         */
        static std::unique_ptr<D12Buffer> CreateStructuredBuffer(
            size_t      elementCount,
            size_t      elementSize,
            const void* initialData = nullptr,
            const char* debugName   = "StructuredBuffer");

        // ===== 纹理创建API =====

        /**
         * 创建纹理（主要方法）
         * 对应Iris的IrisRenderSystem.createTexture()，支持Bindless纹理架构
         *
         * DirectX 12 API调用链：
         * 1. ID3D12Device::CreateCommittedResource() - 创建纹理资源
         * 2. ID3D12Device::CreateShaderResourceView() - 创建SRV
         * 3. ID3D12Device::CreateUnorderedAccessView() - 创建UAV (如果需要)
         * 4. ID3D12Resource::SetName() - 设置调试名称
         *
         * @param createInfo 纹理创建信息
         * @return 创建的D12Texture智能指针，失败返回nullptr
         */
        static std::unique_ptr<D12Texture>      CreateTexture(const TextureCreateInfo& createInfo);
        static std::unique_ptr<D12DepthTexture> CreateDepthTexture(const DepthTextureCreateInfo& createInfo);

        /**
         * 简化的创建2D纹理方法
         * @param width 纹理宽度
         * @param height 纹理高度
         * @param format 纹理格式
         * @param usage 使用标志
         * @param initialData 初始数据（可为nullptr）
         * @param debugName 调试名称
         * @return 2D纹理指针
         */
        static std::unique_ptr<D12Texture> CreateTexture2D(
            uint32_t     width,
            uint32_t     height,
            DXGI_FORMAT  format,
            TextureUsage usage,
            const void*  initialData = nullptr,
            const char*  debugName   = "Texture2D");

        /**
         * 使用默认TextureUsage::ShaderResource
         */
        static std::unique_ptr<D12Texture> CreateTexture2D(
            uint32_t    width,
            uint32_t    height,
            DXGI_FORMAT format,
            const void* initialData = nullptr,
            const char* debugName   = "Texture2D");


        // ===== 设备访问API =====

        /**
         * 获取DirectX 12设备接口
         * @return ID3D12Device指针，用于高级操作
         */
        static ID3D12Device* GetDevice() { return s_device.Get(); }

        /**
         * 获取DXGI工厂接口
         * @return IDXGIFactory4指针，用于交换链创建
         */
        static IDXGIFactory4* GetDXGIFactory() { return s_dxgiFactory.Get(); }

        /**
         * 获取选择的图形适配器
         * @return IDXGIAdapter1指针
         */
        static IDXGIAdapter1* GetAdapter() { return s_adapter.Get(); }

        // ===== 命令管理API =====

        /**
         * 获取命令队列管理器
         * @return CommandListManager指针，对应IrisRenderSystem的命令管理功能
         * @note D3D12RenderSystem作为DirectX 12底层API封装，负责管理CommandListManager实例
         */
        static CommandListManager* GetCommandListManager();

        // ===== 调试API =====

        /**
         * 设置对象调试名称
         * DirectX 12 API: ID3D12Object::SetName()
         * @param object 要命名的DirectX对象
         * @param name 调试名称
         */
        static void SetDebugName(ID3D12Object* object, const char* name);

        /**
         * 检查设备是否支持某个特性
         * DirectX 12 API: ID3D12Device::CheckFeatureSupport()
         * @param feature 要检查的特性
         * @return 是否支持
         */
        static bool CheckFeatureSupport(D3D12_FEATURE feature);

        // ===== 内存管理API =====

        /**
         * 获取GPU内存使用情况
         * DXGI API: IDXGIAdapter3::QueryVideoMemoryInfo()
         * @return 内存信息结构
         */
        static DXGI_QUERY_VIDEO_MEMORY_INFO GetVideoMemoryInfo();

        // ===== 资源创建API =====

        /**
         * 创建提交资源的统一接口
         * DirectX 12 API: ID3D12Device::CreateCommittedResource()
         * 为所有资源类型提供统一的资源创建入口
         * @param heapProps 堆属性
         * @param desc 资源描述
         * @param initialState 初始资源状态
         * @param resource 输出的资源指针
         * @return 创建结果HRESULT
         */
        static HRESULT CreateCommittedResource(
            const D3D12_HEAP_PROPERTIES& heapProps,
            const D3D12_RESOURCE_DESC&   desc,
            D3D12_RESOURCE_STATES        initialState,
            ID3D12Resource**             resource);

        // ===== Bindless资源管理API (Milestone 2.3新增) =====

        /**
         * 获取Bindless资源管理器
         * @return BindlessResourceManager指针，用于底层资源注册
         * @note 遵循分层架构：D3D12RenderSystem管理BindlessResourceManager
         */
        static std::unique_ptr<BindlessResourceManager>& GetBindlessResourceManager();

        /**
         * 获取着色器资源绑定器
         * @return ShaderResourceBinder指针，用于现代化语义资源绑定
         * @note 遵循分层架构：ShaderResourceBinder包含BindlessResourceManager
         */
        static std::unique_ptr<ShaderResourceBinder>& GetShaderResourceBinder();

        /**
         * 统一资源注册接口 - 为D12Resource提供的便捷方法
         * @param resource 要注册的资源
         * @param resourceType 资源类型
         * @return 成功返回bindless索引，失败返回nullopt
         *
         * 教学要点：
         * - 这是D12Resource RegisterToBindlessManager的底层实现
         * - 遵循分层原则：D12Resource → D3D12RenderSystem → BindlessResourceManager
         */
        static std::optional<uint32_t> RegisterResourceToBindless(std::shared_ptr<class D12Resource> resource, BindlessResourceType resourceType);

        /**
         * 统一资源注销接口 - 为D12Resource提供的便捷方法
         * @param resource 要注销的资源
         * @return 成功返回true
         */
        static bool UnregisterResourceFromBindless(std::shared_ptr<class D12Resource> resource);

    private:
        // 禁用实例化（纯静态类）
        D3D12RenderSystem()                                    = delete;
        ~D3D12RenderSystem()                                   = delete;
        D3D12RenderSystem(const D3D12RenderSystem&)            = delete;
        D3D12RenderSystem& operator=(const D3D12RenderSystem&) = delete;

        // ===== 核心DirectX 12对象（包含设备和命令系统）=====
        static Microsoft::WRL::ComPtr<ID3D12Device>  s_device; // 主设备
        static Microsoft::WRL::ComPtr<IDXGIFactory4> s_dxgiFactory; // DXGI工厂
        static Microsoft::WRL::ComPtr<IDXGIAdapter1> s_adapter; // 图形适配器

        // 命令系统管理（对应IrisRenderSystem的命令管理职责）
        static std::unique_ptr<CommandListManager> s_commandListManager; // 命令列表管理器

        // Bindless资源管理系统（Milestone 2.3新增）
        static std::unique_ptr<BindlessResourceManager> s_bindlessResourceManager; // Bindless资源管理器
        static std::unique_ptr<ShaderResourceBinder>    s_shaderResourceBinder; // 现代化资源绑定器

        // 系统状态（移除重复的配置结构）
        static bool s_isInitialized; // 初始化状态

        // ===== 内部初始化方法 =====
        static bool CreateDevice(bool enableGPUValidation); // 简化的设备创建方法
        static void EnableDebugLayer();

        // ===== 内部辅助方法 =====
        static size_t AlignConstantBufferSize(size_t size); // 常量缓冲区大小对齐
    };
} // namespace enigma::graphic
