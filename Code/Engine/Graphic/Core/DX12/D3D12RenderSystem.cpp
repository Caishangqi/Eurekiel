#include "D3D12RenderSystem.hpp"
#include <cassert>
#include <algorithm>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Resource/DepthTexture/D12DepthTexture.hpp"
#include "Engine/Graphic/Resource/BindlessResourceManager.hpp"
#include "Engine/Graphic/Resource/ShaderResourceBinder.hpp"

namespace enigma::graphic
{
    // ===== Static member variable initialization (including device and command system) =====
    Microsoft::WRL::ComPtr<ID3D12Device>  D3D12RenderSystem::s_device      = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> D3D12RenderSystem::s_dxgiFactory = nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> D3D12RenderSystem::s_adapter     = nullptr;

    // Command system management
    std::unique_ptr<CommandListManager> D3D12RenderSystem::s_commandListManager = nullptr;
    // Bindless resource management system
    std::unique_ptr<BindlessResourceManager> D3D12RenderSystem::s_bindlessResourceManager = nullptr;
    std::unique_ptr<ShaderResourceBinder>    D3D12RenderSystem::s_shaderResourceBinder    = nullptr;

    bool D3D12RenderSystem::s_isInitialized = false;

    // ===== Public API implementation =====

    /**
     * Initialize DirectX 12 rendering system (device and command system)
     * The IrisRenderSystem.initialize() method corresponding to Iris
     * Note: The complete initialization of CommandListManager is now included
     */
    bool D3D12RenderSystem::Initialize(bool enableDebugLayer, bool enableGPUValidation)
    {
        if (s_isInitialized)
        {
            return true; // has been initialized
        }

        // 1. Enable debug layer (if requested)
        if (enableDebugLayer)
        {
            EnableDebugLayer();
        }

        // 2. Create DXGI factories and equipment
        if (!CreateDevice(enableGPUValidation))
        {
            return false;
        }

        // 3. Create and initialize CommandListManager (command command management corresponding to IrisRenderSystem)
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
     * Turn off the rendering system (release all resources)
     * Includes CommandListManager, Devices and DXGI Objects
     * Complete cleaning logic corresponding to IrisRenderSystem
     */
    void D3D12RenderSystem::Shutdown()
    {
        if (!s_isInitialized)
        {
            return;
        }

        // 1. First close CommandListManager (wait for the GPU to complete all commands)
        if (s_commandListManager)
        {
            s_commandListManager->Shutdown();
            s_commandListManager.reset();
        }

        // 2. Release DirectX 12 object (ComPtr will be automatically released)
        s_adapter.Reset();
        s_dxgiFactory.Reset();
        s_device.Reset();

        s_isInitialized = false;
    }

    // ===== Buffer creation API implementation =====

    /**
     * Main buffer creation method
     * The IrisRenderSystem.createBuffers() method corresponding to Iris
     *
     * Teaching focus:
     * 1. Parameter verification and error handling
     * 2. Use RAII for resource management
     * 3. The complete process of DirectX 12 resource creation
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateBuffer(const BufferCreateInfo& createInfo)
    {
        // Verify that the system has been initialized
        if (!s_isInitialized || !s_device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(), "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // Verify input parameters
        if (createInfo.size == 0)
        {
            assert(false && "Buffer size must be greater than 0");
            return nullptr;
        }

        // 256 byte alignment of the constant buffer
        // DirectX 12 requirements: Constant buffers must be aligned 256 bytes
        BufferCreateInfo alignedCreateInfo = createInfo;
        if (HasFlag(createInfo.usage, BufferUsage::ConstantBuffer))
        {
            alignedCreateInfo.size = AlignConstantBufferSize(createInfo.size);
        }

        // Create a D12Buffer object
        // Use std::make_unique to ensure exception security
        try
        {
            auto buffer = std::make_unique<D12Buffer>(alignedCreateInfo);

            // Verify that the buffer is created successfully
            if (!buffer->IsValid())
            {
                return nullptr;
            }

            return buffer;
        }
        catch (const std::exception&)
        {
            // Log Error
            core::LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create D12Buffer");
            assert(false && "Failed to create D12Buffer");
            return nullptr;
        }
    }

    /**
     * Simplified vertex buffer creation
     * Corresponding to common vertex data usage
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateVertexBuffer(size_t size, const void* initialData, const char* debugName)
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
     * Simplified index buffer creation
     * Corresponding to common index data uses
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateIndexBuffer(size_t size, const void* initialData, const char* debugName)
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
      * Simplified constant buffer creation
      * DirectX 12 requirements: Constant buffers must be aligned 256 bytes
      */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateConstantBuffer(
        size_t      size,
        const void* initialData,
        const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = size; // AlignConstantBufferSize会在CreateBuffer中调用
        createInfo.usage        = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUWritable; // Constant buffers usually need to be updated frequently
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    /**
     * Create a structured buffer (corresponding to Iris' SSBO)
     * Used to store a large amount of structured data, such as particle system, instance data, etc.
     */
    std::unique_ptr<D12Buffer> D3D12RenderSystem::CreateStructuredBuffer(size_t elementCount, size_t elementSize, const void* initialData, const char* debugName)
    {
        BufferCreateInfo createInfo;
        createInfo.size         = elementCount * elementSize;
        createInfo.usage        = BufferUsage::StructuredBuffer;
        createInfo.memoryAccess = initialData ? MemoryAccess::CPUToGPU : MemoryAccess::GPUOnly;
        createInfo.initialData  = initialData;
        createInfo.debugName    = debugName;

        return CreateBuffer(createInfo);
    }

    // ===== Texture creation API implementation =====

    /**
     * 主要纹理创建方法
     * 对应Iris的IrisRenderSystem.createTexture()，支持Bindless纹理架构
     *
     * 教学重点:
     * 1. 参数验证和错误处理
     * 2. 使用RAII进行资源管理
     * 3. DirectX 12纹理创建的完整流程
     *
     * DirectX 12 API调用链：
     * 1. ID3D12Device::CreateCommittedResource() - 创建纹理资源
     * 2. ID3D12Device::CreateShaderResourceView() - 创建SRV
     * 3. ID3D12Device::CreateUnorderedAccessView() - 创建UAV (如果需要)
     * 4. ID3D12Resource::SetName() - 设置调试名称
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture(const TextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(), "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // 验证输入参数
        if (createInfo.width == 0 || createInfo.height == 0)
        {
            assert(false && "Texture dimensions must be greater than 0");
            return nullptr;
        }

        // 验证纹理格式
        if (createInfo.format == DXGI_FORMAT_UNKNOWN)
        {
            assert(false && "Texture format cannot be UNKNOWN");
            return nullptr;
        }

        // 验证使用标志
        if (createInfo.usage == static_cast<TextureUsage>(0))
        {
            assert(false && "Texture usage must be specified");
            return nullptr;
        }

        // 创建D12Texture对象
        // 使用std::make_unique确保异常安全
        try
        {
            auto texture = std::make_unique<D12Texture>(createInfo);

            // 验证纹理创建成功
            if (!texture->IsValid())
            {
                core::LogError(RendererSubsystem::GetStaticSubsystemName(), "Failed to create D12Texture resource");
                return nullptr;
            }

            return texture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Exception during D12Texture creation: %s", e.what());
            assert(false && "Failed to create D12Texture");
            return nullptr;
        }
    }

    /**
     * 简化的创建2D纹理方法
     * 对应常见的2D纹理使用场景
     *
     * 教学重点:
     * - 默认参数的合理选择
     * - 常见纹理用法的封装
     * - 简化API的设计思路
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(
        uint32_t     width,
        uint32_t     height,
        DXGI_FORMAT  format,
        TextureUsage usage,
        const void*  initialData,
        const char*  debugName)
    {
        TextureCreateInfo createInfo;
        createInfo.type        = TextureType::Texture2D;
        createInfo.width       = width;
        createInfo.height      = height;
        createInfo.depth       = 1;
        createInfo.mipLevels   = 1; // 默认单层Mip
        createInfo.arraySize   = 1; // 非数组纹理
        createInfo.format      = format;
        createInfo.usage       = usage;
        createInfo.initialData = initialData;
        createInfo.debugName   = debugName;

        // 计算数据大小和间距（如果提供了初始数据）
        if (initialData)
        {
            uint32_t bytesPerPixel = D12Texture::GetFormatBytesPerPixel(format);
            createInfo.rowPitch    = width * bytesPerPixel;
            createInfo.slicePitch  = createInfo.rowPitch * height;
            createInfo.dataSize    = createInfo.slicePitch;
        }

        return CreateTexture(createInfo);
    }

    /**
     * 重载版本：提供默认TextureUsage参数
     */
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(
        uint32_t    width,
        uint32_t    height,
        DXGI_FORMAT format,
        const void* initialData,
        const char* debugName)
    {
        return CreateTexture2D(width, height, format, TextureUsage::ShaderResource, initialData, debugName);
    }

    /**
     * 创建深度纹理（主要方法）
     * 对应Iris的深度纹理管理功能，支持depthtex1/depthtex2
     *
     * 教学重点:
     * 1. 深度纹理格式选择和验证
     * 2. DSV/SRV双视图管理
     * 3. 深度采样和阴影贴图支持
     * 4. RAII资源管理和异常安全
     *
     * DirectX 12 API调用链：
     * 1. 参数验证和格式检查
     * 2. D12DepthTexture构造函数内部调用CreateCommittedResource
     * 3. 自动创建DSV和SRV描述符（如果支持采样）
     * 4. 设置调试名称
     */
    std::unique_ptr<D12DepthTexture> D3D12RenderSystem::CreateDepthTexture(const DepthTextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(), "D3D12RenderSystem not initialized");
            assert(false && "D3D12RenderSystem not initialized");
            return nullptr;
        }

        // 验证输入参数
        if (createInfo.width == 0 || createInfo.height == 0)
        {
            assert(false && "Depth texture dimensions must be greater than 0");
            return nullptr;
        }

        // 验证深度纹理名称
        if (createInfo.name.empty())
        {
            assert(false && "Depth texture name cannot be empty");
            return nullptr;
        }

        // 验证清除值范围
        if (createInfo.clearDepth < 0.0f || createInfo.clearDepth > 1.0f)
        {
            assert(false && "Clear depth value must be between 0.0 and 1.0");
            return nullptr;
        }

        // 创建D12DepthTexture对象
        // 使用std::make_unique确保异常安全
        try
        {
            auto depthTexture = std::make_unique<D12DepthTexture>(createInfo);

            // 验证深度纹理创建成功
            if (!depthTexture->IsValid())
            {
                core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "Failed to create D12DepthTexture resource: %s", createInfo.name.c_str());
                return nullptr;
            }

            // 记录成功创建的日志（调试用）
            core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "Created D12DepthTexture: %s (%dx%d, Type: %d)",
                          createInfo.name.c_str(),
                          createInfo.width,
                          createInfo.height,
                          static_cast<int>(createInfo.depthType));

            return depthTexture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "Exception during D12DepthTexture creation (%s): %s",
                           createInfo.name.c_str(), e.what());
            assert(false && "Failed to create D12DepthTexture");
            return nullptr;
        }
    }

    // ===== Debug API implementation =====

    /**
     * Set DirectX object debug name
     * Corresponding to Iris' GLDebug.nameObject function
     */
    void D3D12RenderSystem::SetDebugName(ID3D12Object* object, const char* name)
    {
        if (!object || !name)
        {
            return;
        }

        // Convert to wide string
        size_t   len      = strlen(name);
        wchar_t* wideName = new wchar_t[len + 1];

        size_t convertedChars = 0;
        auto   error          = mbstowcs_s(&convertedChars, wideName, len + 1, name, len);
        assert(error == 0 && "Failed to convert string to wide string");
        UNUSED(error)

        // DirectX 12 API: ID3D12Object::SetName()
        object->SetName(wideName);
        delete[] wideName;
    }

    /**
     * Check the device characteristics support
     */
    bool D3D12RenderSystem::CheckFeatureSupport(D3D12_FEATURE feature)
    {
        if (!s_device)
        {
            return false;
        }

        // Here you need to query according to the specific feature type
        // Example: Check basic feature support
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
        HRESULT                          hr      = s_device->CheckFeatureSupport(feature, &options, sizeof(options));
        return SUCCEEDED(hr);
    }

    /**
     * Get GPU memory information
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

    // ===== Command Management API implementation =====

    /**
     * Get command queue manager
     * Command management function corresponding to IrisRenderSystem
     * D3D12RenderSystem is a DirectX 12 underlying API encapsulation to directly manage CommandListManager instances
     */
    CommandListManager* D3D12RenderSystem::GetCommandListManager()
    {
        if (!s_isInitialized || !s_commandListManager)
        {
            return nullptr;
        }
        return s_commandListManager.get();
    }

    // ===== Internal implementation method =====

    /**
     * Create DirectX 12 devices
     */
    bool D3D12RenderSystem::CreateDevice([[maybe_unused]] bool enableGPUValidation)
    {
        // 1. Create a DXGI factory
        // DXGI API: CreateDXGIFactory2()
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&s_dxgiFactory));
        if (FAILED(hr))
        {
            assert(false && "Failed to create DXGI factory");
            return false;
        }

        // 2. Enumerate the adapter and select the best one
        for (UINT adapterIndex = 0; ; ++adapterIndex)
        {
            hr = s_dxgiFactory->EnumAdapters1(adapterIndex, &s_adapter);
            if (FAILED(hr))
            {
                break; // No more adapters
            }

            // Try to create a DirectX 12 device
            // DirectX 12 API: D3D12CreateDevice()
            hr = D3D12CreateDevice(
                s_adapter.Get(), // Adapter
                D3D_FEATURE_LEVEL_11_0, // Minimum feature level (using standard value)
                IID_PPV_ARGS(&s_device) // Output device interface
            );

            if (SUCCEEDED(hr))
            {
                // Successfully created the device
                break;
            }

            // Release the current adapter and try the next one
            s_adapter.Reset();
        }

        if (!s_device)
        {
            LogError(RendererSubsystem::GetStaticSubsystemName(), "Failed to create D3D12 device");
            assert(false && "Failed to create D3D12 device");
            ERROR_AND_DIE("Failed to create D3D12 device")
        }

        return true;
    }

    /**
     * Enable DirectX 12 debug layer
     */
    void D3D12RenderSystem::EnableDebugLayer()
    {
#ifdef _DEBUG
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;

        // DirectX 12 API: D3D12GetDebugInterface()
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            // Enable debug layer
            debugController->EnableDebugLayer();

            // Optional: Enable GPU verification (performance impact is greater)
            // Note: GPU verification has been moved to the Initialize parameter to control
            // s_config is no longer used here, simplifying the architecture
        }
#endif
    }

    /**
     * Constant buffer size aligned to 256 bytes
     * DirectX 12 requirements: Constant buffers must be aligned 256 bytes
     */
    size_t D3D12RenderSystem::AlignConstantBufferSize(size_t size)
    {
        // 256 bytes aligned
        const size_t alignment = 256;
        return (size + alignment - 1) & ~(alignment - 1);
    }

    /**
     * Create a unified interface for submitting resources
     */
    HRESULT D3D12RenderSystem::CreateCommittedResource(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState, ID3D12Resource** resource)
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

    std::unique_ptr<BindlessResourceManager>& D3D12RenderSystem::GetBindlessResourceManager()
    {
        return s_bindlessResourceManager;
    }

    std::unique_ptr<ShaderResourceBinder>& D3D12RenderSystem::GetShaderResourceBinder()
    {
        return s_shaderResourceBinder;
    }
} // namespace enigma::graphic
