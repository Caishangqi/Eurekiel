#include "D3D12RenderSystem.hpp"
#include <cassert>
#include <algorithm>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/D12DepthTexture.hpp"
#include "Engine/Graphic/Resource/BindlessIndexAllocator.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Atlas/ImageResource.hpp"
#include "Engine/Core/Image.hpp"

// Milestone 2.X: 类型安全的VertexBuffer/IndexBuffer
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")

namespace enigma::graphic
{
    // ===== Static member variable initialization (including device and command system) =====
    Microsoft::WRL::ComPtr<ID3D12Device>  D3D12RenderSystem::s_device      = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory4> D3D12RenderSystem::s_dxgiFactory = nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> D3D12RenderSystem::s_adapter     = nullptr;

    // SwapChain management (based on A/A/A decision)
    Microsoft::WRL::ComPtr<IDXGISwapChain3> D3D12RenderSystem::s_swapChain              = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource>  D3D12RenderSystem::s_swapChainBuffers[3]    = {nullptr};
    D3D12_CPU_DESCRIPTOR_HANDLE             D3D12RenderSystem::s_swapChainRTVs[3]       = {};
    uint32_t                                D3D12RenderSystem::s_currentBackBufferIndex = 0;
    uint32_t                                D3D12RenderSystem::s_swapChainBufferCount   = 3;
    uint32_t                                D3D12RenderSystem::s_swapChainWidth         = 0;
    uint32_t                                D3D12RenderSystem::s_swapChainHeight        = 0;

    // Command system management
    std::unique_ptr<CommandListManager> D3D12RenderSystem::s_commandListManager         = nullptr;
    ID3D12GraphicsCommandList*          D3D12RenderSystem::s_currentGraphicsCommandList = nullptr;

    // SM6.6 Bindless resource management system (Milestone 2.7)
    std::unique_ptr<BindlessIndexAllocator>      D3D12RenderSystem::s_bindlessIndexAllocator      = nullptr;
    std::unique_ptr<GlobalDescriptorHeapManager> D3D12RenderSystem::s_globalDescriptorHeapManager = nullptr;
    std::unique_ptr<BindlessRootSignature>       D3D12RenderSystem::s_bindlessRootSignature       = nullptr;

    // Immediate mode rendering system (Milestone 2.6)
    std::unique_ptr<RenderCommandQueue> D3D12RenderSystem::s_renderCommandQueue = nullptr;

    // Texture cache system (Milestone Bindless)
    std::unordered_map<ResourceLocation, std::weak_ptr<D12Texture>> D3D12RenderSystem::s_textureCache;
    std::mutex                                                      D3D12RenderSystem::s_textureCacheMutex;

    bool D3D12RenderSystem::s_isInitialized = false;

    // ===== Public API implementation =====

    /**
     * Initialize DirectX 12 rendering system (device, command system, and SwapChain)
     * The IrisRenderSystem.initialize() method corresponding to Iris
     * Note: Now includes automatic SwapChain creation for complete engine-layer initialization
     */
    bool D3D12RenderSystem::Initialize(bool enableDebugLayer, bool enableGPUValidation,
                                       HWND hwnd, uint32_t         renderWidth, uint32_t renderHeight)
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

        // 4. Initialize SM6.6 Bindless components (Milestone 2.7)

        // 4.1 Create Bindless Index Allocator (纯索引分配器)
        s_bindlessIndexAllocator = std::make_unique<BindlessIndexAllocator>();
        // 构造函数自动初始化，无需调用Initialize()

        // 4.2 Create Global Descriptor Heap Manager (全局描述符堆)
        s_globalDescriptorHeapManager = std::make_unique<GlobalDescriptorHeapManager>();
        if (!s_globalDescriptorHeapManager->Initialize())
        {
            core::LogError(LogRenderer,
                           "Failed to initialize GlobalDescriptorHeapManager");
            s_globalDescriptorHeapManager.reset();
            s_bindlessIndexAllocator.reset();
            s_commandListManager.reset();
            return false;
        }

        // 4.3 Create SM6.6 Bindless Root Signature (极简Root Signature)
        s_bindlessRootSignature = std::make_unique<BindlessRootSignature>();
        if (!s_bindlessRootSignature->Initialize())
        {
            core::LogError(LogRenderer,
                           "Failed to initialize BindlessRootSignature");
            s_bindlessRootSignature.reset();
            s_globalDescriptorHeapManager->Shutdown();
            s_globalDescriptorHeapManager.reset();
            s_bindlessIndexAllocator.reset();
            s_commandListManager.reset();
            return false;
        }

        core::LogInfo(LogRenderer,
                      "SM6.6 Bindless architecture initialized successfully");

        // 5. Create SwapChain (if window handle is provided)
        if (hwnd)
        {
            if (!CreateSwapChain(hwnd, renderWidth, renderHeight))
            {
                core::LogError(LogRenderer,
                               "Failed to create SwapChain during D3D12RenderSystem initialization");
                // SwapChain creation failure is not fatal - continue initialization
                // This allows headless rendering or manual SwapChain creation later
            }
            else
            {
                core::LogInfo(LogRenderer,
                              "D3D12RenderSystem initialized successfully with SwapChain (%dx%d)",
                              renderWidth, renderHeight);
            }
        }
        else
        {
            core::LogInfo(LogRenderer,
                          "D3D12RenderSystem initialized successfully (no SwapChain - headless mode)");
        }

        s_isInitialized = true;
        return true;
    }

    /**
     * Turn off the rendering system (release all resources)
     * Includes CommandListManager, Bindless systems, SwapChain, Devices and DXGI Objects
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

        // 2. Clean up SM6.6 Bindless components (Milestone 2.7)
        if (s_bindlessRootSignature)
        {
            s_bindlessRootSignature->Shutdown();
            s_bindlessRootSignature.reset();
        }

        if (s_globalDescriptorHeapManager)
        {
            s_globalDescriptorHeapManager->Shutdown();
            s_globalDescriptorHeapManager.reset();
        }

        if (s_bindlessIndexAllocator)
        {
            // BindlessIndexAllocator无需显式Shutdown，析构函数自动清理
            s_bindlessIndexAllocator.reset();
        }

        // 3. Clean up SwapChain resources
        for (uint32_t i = 0; i < s_swapChainBufferCount; ++i)
        {
            if (s_swapChainBuffers[i])
            {
                s_swapChainBuffers[i].Reset();
            }
            // Note: RTV descriptors are automatically released when BindlessIndexAllocator is destroyed
            s_swapChainRTVs[i] = {};
        }
        s_swapChain.Reset();
        s_currentBackBufferIndex = 0;
        s_swapChainBufferCount   = 3;

        // 4. Release DirectX 12 object (ComPtr will be automatically released)
        s_adapter.Reset();
        s_dxgiFactory.Reset();
        s_device.Reset();

        s_isInitialized = false;

        core::LogInfo(LogRenderer, "D3D12RenderSystem shutdown completed");
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
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
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
            core::LogError(LogRenderer, "Fail to create D12Buffer");
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

    // ===== 类型安全的VertexBuffer/IndexBuffer创建API (Milestone 2.X新增) =====

    /**
     * @brief 创建类型安全的VertexBuffer
     *
     * 教学要点:
     * 1. 使用D12VertexBuffer构造函数，自动封装stride逻辑
     * 2. 返回具体类型指针，提供类型安全的接口
     * 3. 失败时返回nullptr（D12VertexBuffer构造函数内部assert）
     */
    std::unique_ptr<D12VertexBuffer> D3D12RenderSystem::CreateVertexBufferTyped(
        size_t size, size_t stride, const void* initialData, const char* debugName)
    {
        try
        {
            return std::make_unique<D12VertexBuffer>(size, stride, initialData, debugName);
        }
        catch (const std::exception& e)
        {
            core::LogError(LogRenderer,
                           "Failed to create D12VertexBuffer: %s", e.what());
            return nullptr;
        }
    }

    /**
     * @brief 创建类型安全的IndexBuffer
     *
     * 教学要点:
     * 1. 使用D12IndexBuffer构造函数，自动封装format逻辑
     * 2. 返回具体类型指针，提供类型安全的接口
     * 3. 失败时返回nullptr
     */
    std::unique_ptr<D12IndexBuffer> D3D12RenderSystem::CreateIndexBufferTyped(
        size_t size, D12IndexBuffer::IndexFormat format, const void* initialData, const char* debugName)
    {
        try
        {
            return std::make_unique<D12IndexBuffer>(size, format, initialData, debugName);
        }
        catch (const std::exception& e)
        {
            core::LogError(LogRenderer,
                           "Failed to create D12IndexBuffer: %s", e.what());
            return nullptr;
        }
    }

    /**
     * @brief 绑定VertexBuffer（重载：接受D12VertexBuffer*）
     *
     * 教学要点:
     * 1. 便捷接口，自动调用GetView()
     * 2. 空指针检查，避免崩溃
     * 3. 委托给现有的BindVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW&)实现
     */
    void D3D12RenderSystem::BindVertexBuffer(const D12VertexBuffer* vertexBuffer, UINT slot)
    {
        if (!vertexBuffer)
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: VertexBuffer is nullptr");
            return;
        }

        if (!vertexBuffer->IsValid())
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: VertexBuffer is invalid");
            return;
        }

        // 委托给现有的BindVertexBuffer(view, slot)实现
        BindVertexBuffer(vertexBuffer->GetView(), slot);
    }

    /**
     * @brief 绑定IndexBuffer（重载：接受D12IndexBuffer*）
     *
     * 教学要点:
     * 1. 便捷接口，自动调用GetView()
     * 2. 空指针检查，避免崩溃
     * 3. 委托给现有的BindIndexBuffer(const D3D12_INDEX_BUFFER_VIEW&)实现
     */
    void D3D12RenderSystem::BindIndexBuffer(const D12IndexBuffer* indexBuffer)
    {
        if (!indexBuffer)
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: IndexBuffer is nullptr");
            return;
        }

        if (!indexBuffer->IsValid())
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: IndexBuffer is invalid");
            return;
        }

        // 委托给现有的BindIndexBuffer(view)实现
        BindIndexBuffer(indexBuffer->GetView());
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
    std::unique_ptr<D12Texture> D3D12RenderSystem::CreateTexture(TextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
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
                core::LogError(LogRenderer, "Failed to create D12Texture resource");
                return nullptr;
            }

            return texture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(LogRenderer,
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
     * 从Image对象创建DirectX 12纹理
     * 
     * 教学要点：
     * 1. 直接从Image提取像素数据创建纹理
     * 2. 自动推断纹理格式(RGBA8)
     * 3. 使用已有的CreateTexture2D核心方法
     * 4. **不使用缓存** - Image对象是瞬时的,每次调用都创建新纹理
     */
    /**
     * 从Image对象创建DirectX 12纹理
     * 
     * 教学要点：
     * 1. 直接从Image提取像素数据创建纹理
     * 2. 自动推断纹理格式(RGBA8)
     * 3. 使用已有的CreateTexture2D核心方法
     * 4. **不使用缓存** - Image对象是瞬时的,每次调用都创建新纹理
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(Image& image, TextureUsage usage, const std::string& debugName)
    {
        // 验证Image有效性
        if (!image.GetRawData())
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Image data is null");
            return nullptr;
        }

        IntVec2 dimensions = image.GetDimensions();
        if (dimensions.x <= 0 || dimensions.y <= 0)
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Invalid dimensions (%d x %d)", dimensions.x, dimensions.y);
            return nullptr;
        }

        // 使用已有的CreateTexture2D方法创建纹理
        // 注意：Image始终提供RGBA8数据，所以使用DXGI_FORMAT_R8G8B8A8_UNORM
        auto texture = CreateTexture2D(
            static_cast<uint32_t>(dimensions.x),
            static_cast<uint32_t>(dimensions.y),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            usage,
            image.GetRawData(),
            debugName.empty() ? "Image Texture" : debugName.c_str()
        );

        if (!texture)
        {
            LogError(LogRenderer, "CreateTexture2D(Image): Failed to create texture");
            return nullptr;
        }

        // 转换unique_ptr到shared_ptr
        return std::shared_ptr<D12Texture>(texture.release());
    }

    /**
     * 从ResourceLocation加载并创建纹理（带缓存）
     * 
     * 教学要点：
     * 1. ResourceLocation作为缓存键，类型安全
     * 2. 使用std::mutex保护缓存访问（线程安全）
     * 3. weak_ptr缓存策略：自动释放不再使用的纹理
     * 4. 通过ResourceSubsystem加载ImageResource
     * 5. 缓存命中时直接返回，避免重复加载
     * 
     * 架构设计：
     * - 缓存查询 → 命中返回 → 未命中加载 → 创建纹理 → 插入缓存
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName)
    {
        // 1. 线程安全的缓存查询
        {
            std::lock_guard<std::mutex> lock(s_textureCacheMutex);
            auto                        it = s_textureCache.find(resourceLocation);
            if (it != s_textureCache.end())
            {
                // 尝试锁定weak_ptr
                auto cachedTexture = it->second.lock();
                if (cachedTexture)
                {
                    LogDebug(LogRenderer,
                             "CreateTexture2D(ResourceLocation): Cache hit for '%s'",
                             resourceLocation.ToString().c_str());
                    return cachedTexture;
                }
                else
                {
                    // weak_ptr已过期，从缓存中移除
                    s_textureCache.erase(it);
                    LogDebug(LogRenderer,
                             "CreateTexture2D(ResourceLocation): Expired cache entry removed for '%s'",
                             resourceLocation.ToString().c_str());
                }
            }
        }

        // 2. 缓存未命中，加载资源
        auto resourceSubsystem = g_theEngine->GetSubsystem<ResourceSubsystem>();
        if (!resourceSubsystem)
        {
            LogError(LogRenderer, "CreateTexture2D(ResourceLocation): ResourceSubsystem not found");
            ERROR_AND_DIE("Resource subsystem not found")
        }

        const auto imageResource = std::dynamic_pointer_cast<ImageResource>(resourceSubsystem->GetResource(resourceLocation));
        if (!imageResource)
        {
            LogError(LogRenderer,
                     "CreateTexture2D(ResourceLocation): Failed to load ImageResource for '%s'",
                     resourceLocation.ToString().c_str());
            ERROR_RECOVERABLE("Failed to get image resource")
            return nullptr;
        }

        // 3. 创建纹理（委托给ImageResource重载版本）
        auto d3d12Texture = CreateTexture2D(*imageResource, usage, debugName);
        if (!d3d12Texture)
        {
            LogError(LogRenderer,
                     "CreateTexture2D(ResourceLocation): Failed to create texture for '%s'",
                     resourceLocation.ToString().c_str());
            return nullptr;
        }

        // 4. 插入缓存（线程安全）
        {
            std::lock_guard<std::mutex> lock(s_textureCacheMutex);
            s_textureCache[resourceLocation] = d3d12Texture;
            LogInfo(LogRenderer,
                    "CreateTexture2D(ResourceLocation): Created and cached texture for '%s'",
                    resourceLocation.ToString().c_str());
        }

        return d3d12Texture;
    }

    /**
     * 从ImageResource创建纹理
     * 
     * 教学要点：
     * 1. 验证ImageResource已加载
     * 2. 提取Image对象并委托给Image重载版本
     * 3. 错误处理和日志记录
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const ImageResource& imageResource, TextureUsage usage, const std::string& debugName)
    {
        // 验证ImageResource已加载
        if (!imageResource.IsLoaded())
        {
            LogError(LogRenderer, "CreateTexture2D(ImageResource): ImageResource not loaded");
            ERROR_RECOVERABLE("Failed to get image resource")
            return nullptr;
        }

        // 提取Image对象
        const Image& image = imageResource.GetImage();

        // **修正逻辑错误** - 应该检查GetRawData()返回nullptr才报错
        if (!image.GetRawData())
        {
            LogError(LogRenderer, "CreateTexture2D(ImageResource): Image raw data is null");
            ERROR_RECOVERABLE("Image file is null in raw data")
            return nullptr;
        }

        // 委托给Image重载版本创建纹理
        return CreateTexture2D(const_cast<Image&>(image), usage, debugName);
    }

    /**
     * 从文件路径创建纹理
     * 
     * 教学要点：
     * 1. 直接从文件路径加载Image
     * 2. 不使用ResourceLocation缓存（因为是裸文件路径）
     * 3. 适用于运行时动态加载的纹理
     * 4. 错误处理和日志记录
     */
    std::shared_ptr<D12Texture> D3D12RenderSystem::CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName)
    {
        // 验证路径有效性
        if (imagePath.empty())
        {
            LogError(LogRenderer, "CreateTexture2D(string): Image path is empty");
            return nullptr;
        }

        // 从文件路径加载Image - 使用正确的构造函数
        Image image(imagePath.c_str());

        // 验证Image加载成功
        if (!image.GetRawData())
        {
            LogError(LogRenderer,
                     "CreateTexture2D(string): Failed to load image from path '%s'",
                     imagePath.c_str());
            return nullptr;
        }

        IntVec2 dimensions = image.GetDimensions();
        LogInfo(LogRenderer,
                "CreateTexture2D(string): Loaded image from '%s' (%dx%d)",
                imagePath.c_str(), dimensions.x, dimensions.y);

        // 委托给Image重载版本创建纹理
        return CreateTexture2D(image, usage, debugName.empty() ? imagePath : debugName);
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
    std::unique_ptr<D12DepthTexture> D3D12RenderSystem::CreateDepthTexture(DepthTextureCreateInfo& createInfo)
    {
        // 验证系统已初始化
        if (!s_isInitialized || !s_device)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized");
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
                core::LogError(LogRenderer,
                               "Failed to create D12DepthTexture resource: %s", createInfo.name.c_str());
                return nullptr;
            }

            // 记录成功创建的日志（调试用）
            core::LogInfo(LogRenderer,
                          "Created D12DepthTexture: %s (%dx%d, Format: %d)",
                          createInfo.name.c_str(),
                          createInfo.width,
                          createInfo.height,
                          static_cast<int>(createInfo.depthFormat));

            return depthTexture;
        }
        catch (const std::exception& e)
        {
            // 记录错误
            core::LogError(LogRenderer,
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

    // ============================================================================
    // M6.3: BackBuffer访问方法
    // ============================================================================

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12RenderSystem::GetBackBufferRTV()
    {
        if (!s_swapChain)
        {
            LogWarn(LogRenderer, "GetBackBufferRTV: SwapChain not initialized");
            return {};
        }
        return s_swapChainRTVs[s_currentBackBufferIndex];
    }

    ID3D12Resource* D3D12RenderSystem::GetBackBufferResource()
    {
        if (!s_swapChain)
        {
            LogWarn(LogRenderer, "GetBackBufferResource: SwapChain not initialized");
            return nullptr;
        }
        return s_swapChainBuffers[s_currentBackBufferIndex].Get();
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
            LogError(LogRenderer, "Failed to create D3D12 device");
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

    // ===== SM6.6 Bindless资源管理API实现 (Milestone 2.7) =====

    BindlessIndexAllocator* D3D12RenderSystem::GetBindlessIndexAllocator()
    {
        return s_bindlessIndexAllocator.get();
    }

    GlobalDescriptorHeapManager* D3D12RenderSystem::GetGlobalDescriptorHeapManager()
    {
        return s_globalDescriptorHeapManager.get();
    }

    BindlessRootSignature* D3D12RenderSystem::GetBindlessRootSignature()
    {
        if (!s_bindlessRootSignature)
            return nullptr;
        return s_bindlessRootSignature.get();
    }


    // ===== PSO创建API实现 (Milestone 3.0新增) =====

    Microsoft::WRL::ComPtr<ID3D12PipelineState> D3D12RenderSystem::CreateGraphicsPSO(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
    )
    {
        if (!s_device)
        {
            core::LogError(LogRenderer,
                           "Cannot create PSO: D3D12RenderSystem not initialized");
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        HRESULT                                     hr = s_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));

        if (FAILED(hr))
        {
            core::LogError(LogRenderer,
                           "Failed to create Graphics PSO: HRESULT = 0x%08X", hr);
            return nullptr;
        }

        return pso;
    }

    // ===== ResourceBarrier统一管理API实现 =====

    void D3D12RenderSystem::TransitionResource(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12Resource*            resource,
        D3D12_RESOURCE_STATES      stateBefore,
        D3D12_RESOURCE_STATES      stateAfter,
        const char*                debugName
    )
    {
        // [REQUIRED] 参数验证
        if (!cmdList)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] TransitionResource failed: cmdList is nullptr");
            return;
        }

        if (!resource)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] TransitionResource failed: resource is nullptr");
            return;
        }

        // [OPTIMIZATION] 状态相同检查 - 避免不必要的转换
        if (stateBefore == stateAfter)
        {
            core::LogWarn(LogRenderer,
                          "[ResourceBarrier] TransitionResource skipped: stateBefore == stateAfter (0x%X), resource: %s",
                          stateBefore, debugName ? debugName : "Unknown");
            return;
        }

        // [OK] 构造ResourceBarrier
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = resource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter  = stateAfter;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // [OK] 执行ResourceBarrier
        try
        {
            cmdList->ResourceBarrier(1, &barrier);

            // [DONE] 详细日志记录
            if (debugName)
            {
                core::LogInfo(LogRenderer,
                              "[ResourceBarrier] Transitioned '%s': 0x%X -> 0x%X",
                              debugName, stateBefore, stateAfter);
            }
            else
            {
                core::LogInfo(LogRenderer,
                              "[ResourceBarrier] Transitioned resource: 0x%X -> 0x%X",
                              stateBefore, stateAfter);
            }
        }
        catch (...)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] Exception during TransitionResource for '%s'",
                           debugName ? debugName : "Unknown");
        }
    }

    void D3D12RenderSystem::TransitionResources(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_RESOURCE_BARRIER*    barriers,
        UINT                       numBarriers,
        const char*                debugContext
    )
    {
        // [REQUIRED] 参数验证
        if (!cmdList)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] TransitionResources failed: cmdList is nullptr");
            return;
        }

        if (!barriers)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] TransitionResources failed: barriers is nullptr");
            return;
        }

        if (numBarriers == 0)
        {
            core::LogWarn(LogRenderer,
                          "[ResourceBarrier] TransitionResources called with numBarriers = 0, context: %s",
                          debugContext ? debugContext : "Unknown");
            return;
        }

        // [OK] 执行批量ResourceBarrier
        try
        {
            cmdList->ResourceBarrier(numBarriers, barriers);

            // [DONE] 批量转换日志
            if (debugContext)
            {
                core::LogInfo(LogRenderer,
                              "[ResourceBarrier] Batch transition completed: %u barriers, context: '%s'",
                              numBarriers, debugContext);
            }
            else
            {
                core::LogInfo(LogRenderer,
                              "[ResourceBarrier] Batch transition completed: %u barriers",
                              numBarriers);
            }
        }
        catch (...)
        {
            core::LogError(LogRenderer,
                           "[ResourceBarrier] Exception during TransitionResources, context: '%s', numBarriers: %u",
                           debugContext ? debugContext : "Unknown", numBarriers);
        }
    }

    // ===== Buffer管理API实现 - 细粒度操作 (Milestone M2新增) =====

    void D3D12RenderSystem::BindVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& bufferView, UINT slot)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "BindVertexBuffer: No active graphics command list (call BeginFrame first)");
            return;
        }

        // 绑定VertexBuffer到指定槽位
        s_currentGraphicsCommandList->IASetVertexBuffers(slot, 1, &bufferView);
    }

    void D3D12RenderSystem::BindIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& bufferView)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "BindIndexBuffer: No active graphics command list (call BeginFrame first)");
            return;
        }

        // 绑定IndexBuffer
        s_currentGraphicsCommandList->IASetIndexBuffer(&bufferView);
    }

    void D3D12RenderSystem::Draw(UINT vertexCount, UINT startVertex)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "Draw: No active graphics command list (call BeginFrame first)");
            return;
        }

        // 执行Draw指令（instanceCount=1, startInstance=0）
        s_currentGraphicsCommandList->DrawInstanced(vertexCount, 1, startVertex, 0);
    }

    void D3D12RenderSystem::DrawIndexed(UINT indexCount, UINT startIndex, INT baseVertex)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "DrawIndexed: No active graphics command list (call BeginFrame first)");
            return;
        }

        // 执行DrawIndexed指令（instanceCount=1, startInstance=0）
        s_currentGraphicsCommandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
    }

    void D3D12RenderSystem::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
    {
        if (!s_currentGraphicsCommandList)
        {
            core::LogError(LogRenderer,
                           "SetPrimitiveTopology: No active graphics command list (call BeginFrame first)");
            return;
        }

        // 设置图元拓扑
        s_currentGraphicsCommandList->IASetPrimitiveTopology(topology);
    }

    // ===== 渲染管线API实现 (Milestone 2.6新增) =====

    /**
     * 开始帧渲染 - 清屏操作的正确位置
     *
     * 教学要点：
     * 1. 这是每帧渲染的正确起始点，替代了TestClearScreen的测试逻辑
     * 2. 遵循DirectX 12标准管线：BeginFrame(清屏) → 渲染 → EndFrame(Present)
     * 3. 自动处理资源状态转换和命令列表管理
     * 4. 使用引擎Rgba8颜色系统，提供类型安全和便捷操作
     * 5. 成为正式渲染系统的重要组成部分
     */
    bool D3D12RenderSystem::BeginFrame(const Rgba8& clearColor, float clearDepth, uint8_t clearStencil)
    {
        UNUSED(clearStencil)
        UNUSED(clearDepth)
        if (!s_isInitialized || !s_commandListManager)
        {
            LogError(LogRenderer, "D3D12RenderSystem not initialized for BeginFrame");
            return false;
        }

        // 1. 准备下一帧 (更新SwapChain缓冲区索引)
        PrepareNextFrame();

        LogInfo(LogRenderer,
                "BeginFrame - SwapChain Buffer Index: %u", s_currentBackBufferIndex);

        // 2. 获取当前帧的图形命令列表（M2灵活渲染架构）
        // 教学要点：保持CommandList打开状态，用于后续的绘制操作
        s_currentGraphicsCommandList = s_commandListManager->AcquireCommandList(
            CommandListManager::Type::Graphics,
            "MainFrame Graphics Commands"
        );

        if (!s_currentGraphicsCommandList)
        {
            LogError(LogRenderer, "Failed to acquire command list for BeginFrame");
            return false;
        }

        // 2.5 绑定全局Descriptor Heaps（SM6.6 Bindless必需）
        // 教学要点：
        // 1. 使用DIRECTLY_INDEXED标志的Root Signature必须先绑定Heaps
        // 2. 每次获取新CommandList后都要重新绑定（CommandList状态不保留）
        // 3. SetDescriptorHeaps会同时绑定CBV/SRV/UAV堆和Sampler堆
        // 4. 必须在任何SetGraphicsRootSignature()调用之前完成
        if (s_globalDescriptorHeapManager)
        {
            s_globalDescriptorHeapManager->SetDescriptorHeaps(s_currentGraphicsCommandList);
            LogInfo(LogRenderer,
                    "BeginFrame: Descriptor Heaps bound to CommandList");
        }
        else
        {
            LogWarn(LogRenderer,
                    "BeginFrame: GlobalDescriptorHeapManager is null - Bindless features may not work");
        }

        // 2. 转换资源状态：PRESENT → RENDER_TARGET
        //
        // Bug Fix (2025-10-21 最终修复): 必须显式添加资源 barrier
        //
        // 根本原因：
        // - DirectX 12 **不会**自动转换 SwapChain buffer 的资源状态
        // - ClearRenderTargetView() 要求资源处于 RENDER_TARGET 状态
        // - Present() 要求资源处于 PRESENT/COMMON 状态
        // - 必须手动转换状态，否则会导致 D3D12 ERROR: INVALID_SUBRESOURCE_STATE
        //
        // 错误日志证据：
        // - "Resource state (0x0: D3D12_RESOURCE_STATE_[COMMON|PRESENT]) is invalid for use as a render target"
        // - "Expected State Bits: 0x4: D3D12_RESOURCE_STATE_RENDER_TARGET"
        //
        // DirectX 12 资源状态管理规则：
        // 1. 开发者负责显式管理所有资源状态转换
        // 2. 使用 ResourceBarrier() 转换状态
        // 3. SwapChain buffer 需要在渲染前转到 RENDER_TARGET
        // 4. SwapChain buffer 需要在 Present 前转回 PRESENT/COMMON
        //
        // 参考资料：
        // - DirectX 12 Programming Guide: Resource Barriers
        // - Microsoft D3D12HelloTriangle 示例确实有 barrier
        //
        // 结论：必须添加显式资源 barrier
        ID3D12Resource* currentBackBuffer = GetCurrentSwapChainBuffer();
        if (!currentBackBuffer)
        {
            LogError(LogRenderer, "BeginFrame: Failed to get current SwapChain buffer");
            return false;
        }

        TransitionResource(s_currentGraphicsCommandList, currentBackBuffer,
                           D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                           "SwapChain BackBuffer");

        // 3. 清除渲染目标（使用当前CommandList）
        auto currentRTV = GetCurrentSwapChainRTV();

        if (!ClearRenderTarget(s_currentGraphicsCommandList, &currentRTV, clearColor))
        {
            LogError(LogRenderer, "Failed to clear render target in BeginFrame");
            return false;
        }

        // 3.5 绑定渲染目标到Output Merger阶段
        // 教学要点：OMSetRenderTargets是所有Draw命令的前提条件
        // 教学要点：虽然ClearRenderTargetView不需要此调用，但DrawInstanced必须要
        // 教学要点：DirectX 12不会自动绑定RenderTarget，必须显式调用
        s_currentGraphicsCommandList->OMSetRenderTargets(
            1, // NumRenderTargetDescriptors - 绑定1个RenderTarget
            &currentRTV, // pRenderTargetDescriptors - RTV句柄
            FALSE, // RTsSingleHandleToDescriptorRange - 非连续堆
            nullptr // pDepthStencilDescriptor - 暂不使用深度缓冲
        );

        LogInfo(LogRenderer,
                "BeginFrame: RenderTarget bound to Output Merger (RTV=0x%p)",
                currentRTV.ptr);

        // 4. 设置视口（Viewport）
        // 教学要点：Viewport定义NDC坐标到屏幕像素坐标的映射
        // 教学要点：未设置Viewport会导致三角形被裁剪到空区域
        // 教学要点：每次获取新CommandList后必须重新设置（状态不保留）
        D3D12_VIEWPORT viewport = {};
        viewport.TopLeftX       = 0.0f;
        viewport.TopLeftY       = 0.0f;
        viewport.Width          = static_cast<float>(s_swapChainWidth);
        viewport.Height         = static_cast<float>(s_swapChainHeight);
        viewport.MinDepth       = 0.0f;
        viewport.MaxDepth       = 1.0f;
        s_currentGraphicsCommandList->RSSetViewports(1, &viewport);

        // 5. 设置裁剪矩形（Scissor Rect）
        // 教学要点：ScissorRect定义像素级别的裁剪区域
        // 教学要点：超出此矩形的像素会被丢弃
        // 教学要点：DirectX 12必须显式设置ScissorRect，否则无法渲染
        D3D12_RECT scissorRect = {};
        scissorRect.left       = 0;
        scissorRect.top        = 0;
        scissorRect.right      = static_cast<LONG>(s_swapChainWidth);
        scissorRect.bottom     = static_cast<LONG>(s_swapChainHeight);
        s_currentGraphicsCommandList->RSSetScissorRects(1, &scissorRect);

        LogInfo(LogRenderer,
                "BeginFrame: Viewport and ScissorRect set (%ux%u)",
                s_swapChainWidth, s_swapChainHeight);

        // 注意：CommandList不在此处执行，保持打开状态用于后续绘制
        // 将在EndFrame中统一执行并提交

        // 6. 清除深度模板缓冲 (如果有的话)
        // TODO: 当实现了深度缓冲系统后，在这里调用ClearDepthStencil
        // ClearDepthStencil(nullptr, nullptr, clearDepth, clearStencil);

        //LogInfo(LogRenderer,"BeginFrame completed - Color:(%d,%d,%d,%d), Depth:%.2f", clearColor.r, clearColor.g, clearColor.b, clearColor.a, clearDepth);

        return true;
    }

    /**
     * 结束帧渲染 - 执行CommandList并Present
     *
     * 教学要点：
     * 1. 资源状态转换：RENDER_TARGET → PRESENT（准备显示）
     * 2. 执行CommandList：提交所有渲染指令到GPU
     * 3. Present：将后台缓冲区显示到屏幕
     * 4. GPU同步：等待当前帧完成（临时方案）
     * 5. 清理引用：为下一帧做准备
     *
     * DirectX 12 API调用链：
     * - ID3D12GraphicsCommandList::ResourceBarrier() - 状态转换
     * - CommandListManager::ExecuteCommandList() - 执行指令
     * - IDXGISwapChain::Present() - 显示到屏幕
     * - CommandListManager::WaitForFence() - GPU同步
     */
    bool D3D12RenderSystem::EndFrame()
    {
        // 1. 检查CommandList有效性
        if (!s_currentGraphicsCommandList)
        {
            LogError(LogRenderer,
                     "EndFrame: No active command list (BeginFrame not called?)");
            return false;
        }

        if (!s_swapChain)
        {
            LogError(LogRenderer,
                     "EndFrame: SwapChain not initialized");
            return false;
        }

        // 2. 转换资源状态：RENDER_TARGET → PRESENT
        //
        // Bug Fix (2025-10-21 最终修复): 必须显式添加资源 barrier
        //
        // 根本原因：
        // - DirectX 12 **不会**自动转换 SwapChain buffer 的资源状态
        // - Present() 要求资源处于 PRESENT/COMMON 状态
        // - 渲染完成后资源处于 RENDER_TARGET 状态
        // - 必须手动转换回 PRESENT，否则会导致 D3D12 ERROR: INVALID_SUBRESOURCE_STATE
        //
        // 错误日志证据：
        // - "Resource state (0x4: D3D12_RESOURCE_STATE_RENDER_TARGET) is invalid for use as a PRESENT_SOURCE"
        // - "Expected State Bits: 0x0: D3D12_RESOURCE_STATE_[COMMON|PRESENT]"
        //
        // DirectX 12 资源状态管理规则：
        // 1. 开发者负责显式管理所有资源状态转换
        // 2. 使用 ResourceBarrier() 转换状态
        // 3. SwapChain buffer 需要在 Present 前转回 PRESENT/COMMON
        // 4. CommandList 执行前必须完成所有状态转换
        //
        // 参考资料：
        // - DirectX 12 Programming Guide: Resource Barriers
        // - Microsoft D3D12HelloTriangle 示例确实有 barrier
        //
        // 结论：必须添加显式资源 barrier
        ID3D12Resource* currentBackBuffer = GetCurrentSwapChainBuffer();
        if (!currentBackBuffer)
        {
            LogError(LogRenderer, "EndFrame: Failed to get current SwapChain buffer");
            return false;
        }

        TransitionResource(s_currentGraphicsCommandList, currentBackBuffer,
                           D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT,
                           "SwapChain BackBuffer");

        // 3. 执行CommandList（通过CommandListManager）
        // 教学要点：ExecuteCommandList会关闭CommandList并提交到GPU队列
        uint64_t fenceValue = s_commandListManager->ExecuteCommandList(s_currentGraphicsCommandList);
        if (fenceValue == 0)
        {
            LogError(LogRenderer,
                     "EndFrame: Failed to execute command list");
            return false;
        }

        // 3. Present SwapChain（显示到屏幕）
        // 教学要点：Present将后台缓冲区显示到屏幕，并轮换缓冲区
        // DirectX 12会在Present()内部自动处理资源状态转换
        if (!Present(true)) // vsync = true
        {
            LogError(LogRenderer,
                     "EndFrame: Failed to present frame");
            return false;
        }

        // 4. 等待GPU fence（临时同步方案）
        // 教学要点：这是简化版同步，实际项目应使用多帧并行
        // TODO (性能优化): 改为异步fence管理，支持2-3帧并行
        s_commandListManager->WaitForFence(fenceValue);

        // [IMPORTANT] 5. 显式回收已完成的CommandList
        // 教学要点：WaitForFence只负责等待GPU完成，不负责回收CommandList
        // 需要显式调用UpdateCompletedCommandLists将完成的CommandList移回available队列
        //
        // 原理：
        // - ExecuteCommandList已将wrapper添加到m_executingLists (State::Executing)
        // - WaitForFence确保GPU已完成（fence值已达到）
        // - UpdateCompletedCommandLists检查fence，将完成的wrapper移回available队列
        s_commandListManager->UpdateCompletedCommandLists();

        // 6. 重置s_currentGraphicsCommandList为nullptr
        // 教学要点：防止在下次BeginFrame之前误用已执行的CommandList
        s_currentGraphicsCommandList = nullptr;

        return true;
    }

    /**
     * 清除渲染目标
     * 从TestClearScreen迁移的核心清屏逻辑，适配为正式API并使用引擎颜色系统
     */
    bool D3D12RenderSystem::ClearRenderTarget(ID3D12GraphicsCommandList*         commandList,
                                              const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle,
                                              const Rgba8&                       clearColor)
    {
        if (!s_isInitialized || !s_commandListManager)
        {
            core::LogError(LogRenderer, "D3D12RenderSystem not initialized for ClearRenderTarget");
            return false;
        }

        // 1. 转换Rgba8为DirectX 12需要的float数组
        float clearColorAsFloats[4];
        clearColor.GetAsFloats(clearColorAsFloats);

        // 添加详细的颜色转换调试日志 (Milestone 2.6 诊断)
        /*core::LogInfo(LogRenderer,
                      "ClearRenderTarget - Input Rgba8: r=%d, g=%d, b=%d, a=%d",
                      clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        core::LogInfo(LogRenderer,
                      "ClearRenderTarget - Converted floats: r=%.3f, g=%.3f, b=%.3f, a=%.3f",
                      clearColorAsFloats[0], clearColorAsFloats[1], clearColorAsFloats[2], clearColorAsFloats[3]);*/

        // 2. 获取命令列表 (如果未提供)
        ID3D12GraphicsCommandList* actualCommandList = commandList;
        bool                       needToExecute     = false;

        if (!actualCommandList)
        {
            actualCommandList = s_commandListManager->AcquireCommandList(
                CommandListManager::Type::Graphics,
                "ClearRenderTarget Command List"
            );

            if (!actualCommandList)
            {
                core::LogError(LogRenderer, "Failed to acquire command list for ClearRenderTarget");
                return false;
            }
            needToExecute = true;
        }

        // 3. 获取RTV句柄 (如果未提供，使用当前SwapChain RTV)
        D3D12_CPU_DESCRIPTOR_HANDLE actualRtvHandle;
        ID3D12Resource*             targetResource = nullptr;

        if (rtvHandle)
        {
            actualRtvHandle = *rtvHandle;
            // TODO: 需要从rtvHandle追踪到对应的资源，暂时使用SwapChain资源
            targetResource = GetCurrentSwapChainBuffer();
        }
        else
        {
            actualRtvHandle = GetCurrentSwapChainRTV();
            targetResource  = GetCurrentSwapChainBuffer();
        }

        if (!targetResource)
        {
            core::LogError(LogRenderer, "No valid render target resource for ClearRenderTarget");
            return false;
        }

        // [OK] 资源状态管理 - 由调用者负责
        //
        // Bug Fix (2025-10-21 最终修复): ClearRenderTarget 不负责状态转换
        //
        // 设计决策：
        // - ClearRenderTarget 假设资源已处于 RENDER_TARGET 状态
        // - 调用者（如 BeginFrame）负责在调用前转换状态
        // - 这样避免了重复的状态转换和复杂的状态跟踪
        //
        // 调用约定：
        // 1. 外部提供 commandList：调用者负责状态转换（BeginFrame 的情况）
        // 2. 内部创建 commandList（needToExecute=true）：仍然假设外部已转换状态
        //
        // 为什么不在这里添加 barrier：
        // - BeginFrame 已经转换了状态（PRESENT → RENDER_TARGET）
        // - 避免重复的 barrier 调用
        // - 保持函数职责单一（清屏，不管理状态）
        //
        // 结论：不在 ClearRenderTarget 中添加 barrier

        // 4. 设置渲染目标
        actualCommandList->OMSetRenderTargets(1, &actualRtvHandle, FALSE, nullptr);

        // 5. 执行清屏操作 (使用转换后的float颜色数组)
        actualCommandList->ClearRenderTargetView(actualRtvHandle, clearColorAsFloats, 0, nullptr);

        // 6. 只有当我们内部获取命令列表时才自动执行，如果外部传入则由外部控制执行时机
        if (needToExecute)
        {
            uint64_t fenceValue = s_commandListManager->ExecuteCommandList(actualCommandList);
            if (fenceValue > 0)
            {
                // 等待命令执行完成
                s_commandListManager->WaitForFence(fenceValue);
            }
            else
            {
                core::LogError(LogRenderer, "Failed to execute ClearRenderTarget command list");
                return false;
            }
        }
        // 如果commandList是外部传入的，则不执行，让调用者决定何时执行

        return true;
    }

    // ===== SwapChain管理API实现 （基于A/A/A决策）=====

    /**
     * [IMPORTANT] DirectX 12官方最佳实践：SwapChain Buffer状态管理
     *
     * 关键发现（基于Microsoft DirectX 12 SDK d3d12.h）：
     *
     * 1. D3D12_RESOURCE_STATE_PRESENT == D3D12_RESOURCE_STATE_COMMON (都等于 0)
     *    - 这两个状态在数值上完全等价
     *    - SwapChain buffer创建后自动处于COMMON/PRESENT状态
     *    - 无需显式初始化转换！
     *
     * 2. 官方示例代码（如D3D12HelloWorld）的标准做法：
     *    - CreateSwapChain后不做任何状态初始化
     *    - BeginFrame: PRESENT → RENDER_TARGET
     *    - EndFrame: RENDER_TARGET → PRESENT
     *    - IDXGISwapChain::Present()兼容PRESENT和COMMON状态
     *
     * 3. 删除InitializeSwapChainBufferStates()的理由：
     *    - COMMON → PRESENT的转换实际上是0 → 0，完全无效
     *    - DirectX 12会忽略这种无操作Barrier
     *    - 创建额外的CommandList、执行、同步都是不必要的开销
     *    - 违反KISS原则和DirectX 12最佳实践
     *
     * 4. 正确的资源状态流程（符合Microsoft官方示例）：
     *    - SwapChain创建 → buffer自动处于COMMON(=PRESENT)
     *    - BeginFrame → PRESENT → RENDER_TARGET
     *    - 渲染操作
     *    - EndFrame → RENDER_TARGET → PRESENT
     *    - Present() → 显示（要求PRESENT或COMMON状态）
     *
     * 教学价值：
     * - 展示了如何根据官方API定义优化代码
     * - 证明了查阅官方文档的重要性
     * - 体现了"简单即是美"的工程哲学
     *
     * 参考资料：
     * - DirectX 12 SDK: C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\d3d12.h (Line 3177)
     * - Microsoft DirectX-Graphics-Samples: https://github.com/microsoft/DirectX-Graphics-Samples
     */

    /**
     * 创建SwapChain及其RTV描述符
     */
    bool D3D12RenderSystem::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount)
    {
        if (!s_device || !s_dxgiFactory || !s_commandListManager)
        {
            LogError("D3D12RenderSystem", "Device or DXGI Factory not initialized for SwapChain creation");
            return false;
        }
#undef min
        s_swapChainBufferCount = std::min(bufferCount, 3u); // 限制最多3个缓冲区

        // 存储SwapChain尺寸（用于BeginFrame中的Viewport和ScissorRect设置）
        s_swapChainWidth  = width;
        s_swapChainHeight = height;

        // 1. 创建SwapChain描述符
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount           = s_swapChainBufferCount;
        swapChainDesc.Width                 = width;
        swapChainDesc.Height                = height;
        swapChainDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count      = 1;
        swapChainDesc.SampleDesc.Quality    = 0;
        swapChainDesc.Flags                 = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        // 2. 创建SwapChain (使用CommandListManager的Graphics队列)
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT                                 hr = s_dxgiFactory->CreateSwapChainForHwnd(
            s_commandListManager->GetCommandQueue(CommandListManager::Type::Graphics), // 使用Graphics队列
            hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );

        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to create SwapChain");
            return false;
        }

        // 3. 转换为SwapChain3接口
        hr = swapChain1.As(&s_swapChain);
        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to get SwapChain3 interface");
            return false;
        }

        // 4. 禁用Alt+Enter全屏切换（可选）
        s_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        // 5. 为每个SwapChain缓冲区创建RTV描述符
        // 使用GlobalDescriptorHeapManager的RTV堆
        if (!s_globalDescriptorHeapManager)
        {
            LogError("D3D12RenderSystem", "GlobalDescriptorHeapManager not available for RTV creation");
            return false;
        }

        // 6. 为每个SwapChain缓冲区创建RTV
        for (uint32_t i = 0; i < s_swapChainBufferCount; ++i)
        {
            // 获取SwapChain缓冲区资源
            hr = s_swapChain->GetBuffer(i, IID_PPV_ARGS(&s_swapChainBuffers[i]));
            if (FAILED(hr))
            {
                LogError("D3D12RenderSystem", "Failed to get SwapChain buffer %d", i);
                return false;
            }

            // 分配RTV描述符
            auto rtvAllocation = s_globalDescriptorHeapManager->AllocateRtv();
            if (!rtvAllocation.isValid)
            {
                LogError("D3D12RenderSystem", "Failed to allocate RTV descriptor for SwapChain buffer %d", i);
                return false;
            }

            s_swapChainRTVs[i] = rtvAllocation.cpuHandle;

            // 创建RTV描述符
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice            = 0;

            s_device->CreateRenderTargetView(
                s_swapChainBuffers[i].Get(),
                &rtvDesc,
                s_swapChainRTVs[i]
            );

            // 设置调试名称
            std::string debugName = "SwapChain Buffer " + std::to_string(i);
            SetDebugName(s_swapChainBuffers[i].Get(), debugName.c_str());
        }

        // 7. 初始化当前缓冲区索引
        s_currentBackBufferIndex = s_swapChain->GetCurrentBackBufferIndex();

        LogInfo("D3D12RenderSystem", "SwapChain created successfully: %dx%d, %d buffers", width, height, s_swapChainBufferCount);

        // [IMPORTANT] DirectX 12官方最佳实践：无需显式初始化SwapChain buffer状态
        //
        // 理由（基于Microsoft DirectX 12 SDK官方定义）：
        // 1. D3D12_RESOURCE_STATE_PRESENT == D3D12_RESOURCE_STATE_COMMON == 0
        // 2. SwapChain buffer创建后自动处于COMMON/PRESENT状态
        // 3. BeginFrame会正确处理 PRESENT → RENDER_TARGET 转换
        // 4. 符合Microsoft官方示例代码（D3D12HelloWorld等）的标准做法
        //
        // 之前的错误设计：
        // - InitializeSwapChainBufferStates()执行0→0的无效状态转换
        // - 浪费了CommandList创建、执行、同步的开销
        // - 违反了KISS原则和DirectX 12最佳实践
        //
        // 正确的状态流程：
        // - CreateSwapChain → buffer自动处于COMMON(=PRESENT) ✅
        // - BeginFrame → PRESENT → RENDER_TARGET ✅
        // - EndFrame → RENDER_TARGET → PRESENT ✅
        // - Present() → 显示（兼容PRESENT和COMMON） ✅

        return true;
    }

    /**
     * 获取当前SwapChain后台缓冲区的RTV句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE D3D12RenderSystem::GetCurrentSwapChainRTV()
    {
        return s_swapChainRTVs[s_currentBackBufferIndex];
    }

    /**
     * 获取当前SwapChain后台缓冲区资源
     */
    ID3D12Resource* D3D12RenderSystem::GetCurrentSwapChainBuffer()
    {
        return s_swapChainBuffers[s_currentBackBufferIndex].Get();
    }

    /**
     * 呈现当前帧到屏幕
     */
    bool D3D12RenderSystem::Present(bool vsync)
    {
        if (!s_swapChain)
        {
            LogError("D3D12RenderSystem", "SwapChain not initialized");
            return false;
        }

        UINT syncInterval = vsync ? 1 : 0;
        UINT presentFlags = 0;

        HRESULT hr = s_swapChain->Present(syncInterval, presentFlags);
        if (FAILED(hr))
        {
            LogError("D3D12RenderSystem", "Failed to present frame");
            return false;
        }

        return true;
    }

    /**
     * 准备下一帧渲染（更新后台缓冲区索引）
     */
    void D3D12RenderSystem::PrepareNextFrame()
    {
        if (s_swapChain)
        {
            s_currentBackBufferIndex = s_swapChain->GetCurrentBackBufferIndex();
        }
    }

    // ===== Immediate模式渲染API实现 (Milestone 2.6新增) =====

    RenderCommandQueue* D3D12RenderSystem::GetRenderCommandQueue()
    {
        // 延迟初始化RenderCommandQueue
        if (!s_renderCommandQueue)
        {
            s_renderCommandQueue = std::make_unique<RenderCommandQueue>();
            if (!s_renderCommandQueue->Initialize())
            {
                LogError("D3D12RenderSystem", "Failed to initialize RenderCommandQueue");
                s_renderCommandQueue.reset();
                return nullptr;
            }
            LogDebug("D3D12RenderSystem", "RenderCommandQueue initialized successfully");
        }

        return s_renderCommandQueue.get();
    }

    bool D3D12RenderSystem::AddImmediateCommand(
        WorldRenderingPhase             phase,
        std::unique_ptr<IRenderCommand> command,
        const std::string&              debugTag
    )
    {
        if (!command)
        {
            LogError("D3D12RenderSystem", "AddImmediateCommand: command is nullptr");
            return false;
        }

        if (!command->IsValid())
        {
            LogError("D3D12RenderSystem", "AddImmediateCommand: command is invalid");
            return false;
        }

        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            LogError("D3D12RenderSystem", "AddImmediateCommand: Failed to get RenderCommandQueue");
            return false;
        }

        try
        {
            // 获取指令名称（在移动之前）
            std::string commandName = command->GetName();

            // 使用RenderCommandQueue的智能指针类型转换
            RenderCommandPtr commandPtr(command.release());
            queue->SubmitCommand(std::move(commandPtr), phase, debugTag);

            LogDebug("D3D12RenderSystem", "AddImmediateCommand: Successfully added command '%s' to phase %u",
                     commandName.c_str(), static_cast<uint32_t>(phase));
            return true;
        }
        catch (const std::exception& e)
        {
            LogError("D3D12RenderSystem", "AddImmediateCommand: Exception occurred: %s", e.what());
            return false;
        }
    }

    size_t D3D12RenderSystem::ExecuteImmediateCommands(WorldRenderingPhase phase)
    {
        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            LogError("D3D12RenderSystem", "ExecuteImmediateCommands: RenderCommandQueue not available");
            return 0;
        }

        size_t commandCount = queue->GetCommandCount(phase);
        if (commandCount == 0)
        {
            LogDebug("D3D12RenderSystem", "ExecuteImmediateCommands: No commands to execute for phase %u",
                     static_cast<uint32_t>(phase));
            return 0;
        }

        try
        {
            // 验证CommandListManager可用性
            if (!s_commandListManager)
            {
                LogError("D3D12RenderSystem", "ExecuteImmediateCommands: CommandListManager not available");
                return 0;
            }

            // 获取当前图形命令列表验证
            auto commandList = s_commandListManager->GetCommandQueue(CommandListManager::Type::Graphics);
            if (!commandList)
            {
                LogError("D3D12RenderSystem", "ExecuteImmediateCommands: No active graphics command list");
                return 0;
            }

            LogDebug("D3D12RenderSystem", "ExecuteImmediateCommands: Executing %zu commands for phase %u",
                     commandCount, static_cast<uint32_t>(phase));

            // 执行指定阶段的命令 - 从unique_ptr创建shared_ptr
            // 注意：使用空删除器确保不会删除unique_ptr管理的对象
            std::shared_ptr<CommandListManager> sharedCommandManager(
                s_commandListManager.get(),
                [](CommandListManager*)
                {
                } // 空删除器，不删除对象
            );
            queue->ExecutePhase(phase, sharedCommandManager);

            LogDebug("D3D12RenderSystem", "ExecuteImmediateCommands: Successfully executed %zu commands", commandCount);

            // 清空已执行的命令 - 防止指令累积
            // 教学要点：Immediate模式指令执行后必须清除，否则会不断累积
            queue->ClearPhase(phase);
            LogDebug("D3D12RenderSystem", "ExecuteImmediateCommands: Cleared %zu commands from phase %u",
                     commandCount, static_cast<uint32_t>(phase));

            return commandCount;
        }
        catch (const std::exception& e)
        {
            LogError("D3D12RenderSystem", "ExecuteImmediateCommands: Exception occurred: %s", e.what());
            return 0;
        }
    }

    void D3D12RenderSystem::ClearImmediateCommands(WorldRenderingPhase phase)
    {
        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            LogWarn("D3D12RenderSystem", "ClearImmediateCommands: RenderCommandQueue not available");
            return;
        }

        size_t commandCount = queue->GetCommandCount(phase);
        queue->ClearPhase(phase);

        LogDebug("D3D12RenderSystem", "ClearImmediateCommands: Cleared %zu commands from phase %u",
                 commandCount, static_cast<uint32_t>(phase));
    }

    void D3D12RenderSystem::ClearAllImmediateCommands()
    {
        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            LogWarn("D3D12RenderSystem", "ClearAllImmediateCommands: RenderCommandQueue not available");
            return;
        }

        size_t totalCommands = queue->GetTotalCommandCount();
        queue->Clear();

        LogDebug("D3D12RenderSystem", "ClearAllImmediateCommands: Cleared %zu total commands from all phases", totalCommands);
    }

    size_t D3D12RenderSystem::GetImmediateCommandCount(WorldRenderingPhase phase)
    {
        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            return 0;
        }

        return queue->GetCommandCount(phase);
    }

    bool D3D12RenderSystem::HasImmediateCommands(WorldRenderingPhase phase)
    {
        RenderCommandQueue* queue = GetRenderCommandQueue();
        if (!queue)
        {
            return false;
        }

        return queue->GetCommandCount(phase) > 0;
    }

    // ===== 纹理缓存管理API实现 (Milestone Bindless新增) =====

    /**
     * 清理未使用的纹理缓存条目
     * 
     * 教学要点：
     * 1. 遍历缓存map,检查weak_ptr是否已过期
     * 2. 移除所有过期的weak_ptr条目
     * 3. 线程安全操作(使用mutex)
     * 4. 返回清理的条目数量用于调试和统计
     * 
     * 使用场景：
     * - 定期调用以清理内存
     * - 在关键帧之前调用以优化性能
     * - 在资源加载后调用以整理缓存
     */
    void D3D12RenderSystem::ClearUnusedTextures()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);

        // 使用erase-remove惯用法清理过期weak_ptr
        size_t initialSize = s_textureCache.size();

        for (auto it = s_textureCache.begin(); it != s_textureCache.end();)
        {
            // 尝试锁定weak_ptr
            if (it->second.expired())
            {
                // weak_ptr已过期,删除该条目
                it = s_textureCache.erase(it);
            }
            else
            {
                ++it;
            }
        }

        size_t finalSize    = s_textureCache.size();
        size_t removedCount = initialSize - finalSize;

        if (removedCount > 0)
        {
            LogInfo(LogRenderer,
                    "ClearUnusedTextures: Removed %zu expired cache entries (%zu remaining)",
                    removedCount, finalSize);
        }
        else
        {
            LogDebug(LogRenderer,
                     "ClearUnusedTextures: No expired entries found (%zu total)",
                     finalSize);
        }
    }

    /**
     * 获取当前纹理缓存大小
     * 
     * 教学要点：
     * 1. 线程安全地获取缓存map大小
     * 2. 返回的是条目数量,而非内存大小
     * 3. 包括已过期的weak_ptr条目(调用ClearUnusedTextures清理)
     * 
     * 使用场景：
     * - 性能监控和调试
     * - 缓存管理策略决策
     * - UI显示资源统计信息
     */
    size_t D3D12RenderSystem::GetTextureCacheSize()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);
        return s_textureCache.size();
    }

    /**
     * 清空整个纹理缓存
     * 
     * 教学要点：
     * 1. 强制清空所有缓存条目
     * 2. 不会删除纹理资源本身(shared_ptr仍可能存活)
     * 3. 仅移除缓存map中的weak_ptr引用
     * 4. 线程安全操作
     * 
     * 使用场景：
     * - 关卡切换时清理缓存
     * - 内存压力大时强制释放
     * - 开发调试时重置资源状态
     * 
     * ⚠️ 警告：
     * - 调用后所有纹理将需要重新加载
     * - 可能导致短暂的性能下降
     * - 应谨慎使用,避免在渲染关键路径调用
     */
    void D3D12RenderSystem::ClearAllTextureCache()
    {
        std::lock_guard<std::mutex> lock(s_textureCacheMutex);

        size_t clearedCount = s_textureCache.size();
        s_textureCache.clear();

        if (clearedCount > 0)
        {
            LogInfo(LogRenderer,
                    "ClearAllTextureCache: Cleared entire cache (%zu entries removed)",
                    clearedCount);
        }
        else
        {
            LogDebug(LogRenderer,
                     "ClearAllTextureCache: Cache was already empty");
        }
    }
} // namespace enigma::graphic
