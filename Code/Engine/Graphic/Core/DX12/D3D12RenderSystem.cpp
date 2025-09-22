#include "D3D12RenderSystem.hpp"
#include <cassert>
#include <algorithm>

namespace enigma::graphic
{
    // ===== 静态成员变量初始化（包含设备和命令系统）=====
    Microsoft::WRL::ComPtr<ID3D12Device>  D3D12RenderSystem::s_device      = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> D3D12RenderSystem::s_dxgiFactory = nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> D3D12RenderSystem::s_adapter     = nullptr;

    // 命令系统管理
    std::unique_ptr<CommandListManager> D3D12RenderSystem::s_commandListManager = nullptr;

    bool D3D12RenderSystem::s_isInitialized = false;

    // ===== 公共API实现 =====

    /**
     * 初始化DirectX 12渲染系统（设备和命令系统）
     * 对应Iris的IrisRenderSystem.initialize()方法
     * 注意：现在包含CommandListManager的完整初始化
     */
    bool D3D12RenderSystem::Initialize(bool enableDebugLayer, bool enableGPUValidation)
    {
        if (s_isInitialized)
        {
            return true; // 已经初始化
        }

        // 1. 启用调试层（如果请求）
        if (enableDebugLayer)
        {
            EnableDebugLayer();
        }

        // 2. 创建DXGI工厂和设备
        if (!CreateDevice(enableGPUValidation))
        {
            return false;
        }

        // 3. 创建和初始化CommandListManager（对应IrisRenderSystem的命令管理）
        s_commandListManager = std::make_unique<CommandListManager>();
        if (!s_commandListManager->Initialize())
        {
            s_commandListManager.reset();
            return false;
        }

        s_isInitialized = true;
        return true;
    }

    /**
     * 关闭渲染系统（释放所有资源）
     * 包括CommandListManager、设备和DXGI对象
     * 对应IrisRenderSystem的完整清理逻辑
     */
    void D3D12RenderSystem::Shutdown()
    {
        if (!s_isInitialized)
        {
            return;
        }

        // 1. 首先关闭CommandListManager（等待GPU完成所有命令）
        if (s_commandListManager)
        {
            s_commandListManager->Shutdown();
            s_commandListManager.reset();
        }

        // 2. 释放DirectX 12对象（ComPtr会自动释放）
        s_adapter.Reset();
        s_dxgiFactory.Reset();
        s_device.Reset();

        s_isInitialized = false;
    }

    // ===== 缓冲区创建API实现 =====

    /**
     * 主要的缓冲区创建方法
     * 对应Iris的IrisRenderSystem.createBuffers()方法
     *
     * 教学重点：
     * 1. 参数验证和错误处理
     * 2. 使用RAII进行资源管理
     * 3. DirectX 12资源创建的完整流程
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        // 验证系统是否已初始化
        if (!s_isInitialized || !s_device)
        {
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // 验证输入参数
        if (createInfo.size == 0)
        {
            assert(false && "Buffer size must be greater than 0");
            return nullptr;
        }

        // 对常量缓冲区进行256字节对齐
        // DirectX 12要求：常量缓冲区必须256字节对齐
        BufferCreateInfo alignedCreateInfo = createInfo;
        if (HasFlag(createInfo.usage, BufferUsage::ConstantBuffer))
        {
            alignedCreateInfo.size = AlignConstantBufferSize(createInfo.size);
        }

        // 创建D12Buffer对象
        // 使用std::make_unique确保异常安全
        try
        {
            auto buffer = std::make_unique<D12Buffer>(alignedCreateInfo);

            // 验证缓冲区是否创建成功
            if (!buffer->IsValid())
            {
                return nullptr;
            }

            return buffer;
        }
        catch (const std::exception&)
        {
            // 记录错误（实际项目中应该使用日志系统）
            assert(false && "Failed to create D12Buffer");
            return nullptr;
        }
    }

    /**
     * 简化的顶点缓冲区创建
     * 对应常见的顶点数据用途
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateVertexBuffer(
        size_t      size,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size;
        createInfo.usage        = BufferUsage::VertexBuffer;
        createInfo.memoryAccess = initialData ? MemoryAccess::CPUToGPU : MemoryAccess::GPUOnly;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    /**
     * 简化的索引缓冲区创建
     * 对应常见的索引数据用途
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateIndexBuffer(
        size_t      size,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size;
        createInfo.usage        = BufferUsage::IndexBuffer;
        createInfo.memoryAccess = initialData ? MemoryAccess::CPUToGPU : MemoryAccess::GPUOnly;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    /**
     * 简化的常量缓冲区创建
     * DirectX 12要求：常量缓冲区必须256字节对齐
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateConstantBuffer(
        size_t      size,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size; // AlignConstantBufferSize会在CreateBuffer中调用
        createInfo.usage        = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUWritable; // 常量缓冲区通常需要频繁更新
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    /**
     * 创建结构化缓冲区（对应Iris的SSBO）
     * 用于存储大量结构化数据，如粒子系统、实例数据等
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateStructuredBuffer(
        size_t      elementCount,
        size_t      elementSize,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = elementCount * elementSize;
        createInfo.usage        = BufferUsage::StructuredBuffer;
        createInfo.memoryAccess = initialData ? MemoryAccess::CPUToGPU : MemoryAccess::GPUOnly;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    // ===== 调试API实现 =====

    /**
     * 设置DirectX对象调试名称
     * 对应Iris的GLDebug.nameObject功能
     */
    void D3D12RenderSystem::SetDebugName(ID3D12Object* object, const char* name)
    {
        if (!object || !name)
        {
            return;
        }

        // 转换为宽字符串
        size_t   len      = strlen(name);
        wchar_t* wideName = new wchar_t[len + 1];

        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, wideName, len + 1, name, len);

        // DirectX 12 API: ID3D12Object::SetName()
        object->SetName(wideName);

        delete[] wideName;
    }

    /**
     * 检查设备特性支持
     */
    bool D3D12RenderSystem::CheckFeatureSupport(D3D12_FEATURE feature)
    {
        if (!s_device)
        {
            return false;
        }

        // 这里需要根据具体的feature类型来查询
        // 示例：检查基本特性支持
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
        HRESULT                          hr      = s_device->CheckFeatureSupport(feature, &options, sizeof(options));
        return SUCCEEDED(hr);
    }

    /**
     * 获取GPU内存信息
     */
    DXGI_QUERY_VIDEO_MEMORY_INFO D3D12RenderSystem::GetVideoMemoryInfo()
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo = {};

        if (s_adapter)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
            if (SUCCEEDED(s_adapter.As(&adapter3)))
            {
                adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo);
            }
        }

        return memoryInfo;
    }

    // ===== 命令管理API实现 =====

    /**
     * 获取命令队列管理器
     * 对应IrisRenderSystem的命令管理功能
     * D3D12RenderSystem作为DirectX 12底层API封装，直接管理CommandListManager实例
     */
    CommandListManager* D3D12RenderSystem::GetCommandListManager()
    {
        if (!s_isInitialized || !s_commandListManager)
        {
            return nullptr;
        }
        return s_commandListManager.get();
    }

    // ===== 内部实现方法 =====

    /**
     * 创建DirectX 12设备
     */
    bool D3D12RenderSystem::CreateDevice([[maybe_unused]] bool enableGPUValidation)
    {
        // 1. 创建DXGI工厂
        // DXGI API: CreateDXGIFactory2()
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&s_dxgiFactory));
        if (FAILED(hr))
        {
            assert(false && "Failed to create DXGI factory");
            return false;
        }

        // 2. 枚举适配器，选择最好的
        for (UINT adapterIndex = 0; ; ++adapterIndex)
        {
            hr = s_dxgiFactory->EnumAdapters1(adapterIndex, &s_adapter);
            if (FAILED(hr))
            {
                break; // 没有更多适配器
            }

            // 尝试创建DirectX 12设备
            // DirectX 12 API: D3D12CreateDevice()
            hr = D3D12CreateDevice(
                s_adapter.Get(), // 适配器
                D3D_FEATURE_LEVEL_11_0, // 最低特性级别（使用标准值）
                IID_PPV_ARGS(&s_device) // 输出设备接口
            );

            if (SUCCEEDED(hr))
            {
                // 成功创建设备
                break;
            }

            // 释放当前适配器，尝试下一个
            s_adapter.Reset();
        }

        if (!s_device)
        {
            assert(false && "Failed to create D3D12 device");
            return false;
        }

        return true;
    }

    /**
     * 启用DirectX 12调试层
     */
    void D3D12RenderSystem::EnableDebugLayer()
    {
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;

        // DirectX 12 API: D3D12GetDebugInterface()
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            // 启用调试层
            debugController->EnableDebugLayer();

            // 可选：启用GPU验证（性能影响较大）
            // 注意：GPU验证已移到Initialize参数中控制
            // 这里不再使用s_config，简化了架构
        }
#endif
    }

    /**
     * 常量缓冲区大小对齐到256字节
     * DirectX 12要求：常量缓冲区必须256字节对齐
     */
    size_t D3D12RenderSystem::AlignConstantBufferSize(size_t size)
    {
        // 256字节对齐
        const size_t alignment = 256;
        return (size + alignment - 1) & ~(alignment - 1);
    }

    /**
     * 创建提交资源的统一接口
     */
    HRESULT D3D12RenderSystem::CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES& heapProps,
        const D3D12_RESOURCE_DESC&   desc,
        D3D12_RESOURCE_STATES        initialState,
        ID3D12Resource**             resource)
    {
        if (!s_device)
        {
            return E_FAIL;
        }

        return s_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initialState,
            nullptr,
            IID_PPV_ARGS(resource));
    }
} // namespace enigma::graphic
