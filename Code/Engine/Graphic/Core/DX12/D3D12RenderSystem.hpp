#pragma once

#include "../../Resource/Buffer/D12Buffer.hpp"
#include "../../Resource/BindlessResourceTypes.hpp"
#include "../../Resource/CommandListManager.hpp"
#include "../../Resource/BindlessIndexAllocator.hpp"                // SM6.6索引分配器
#include "../../Resource/GlobalDescriptorHeapManager.hpp"           // 全局描述符堆管理器
#include "../../Resource/BindlessRootSignature.hpp"                 // SM6.6 Root Signature
#include "../../Immediate/RenderCommandQueue.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>

#include "Engine/Resource/Model/ModelResource.hpp"
#include "Engine/Core/Image.hpp"

#undef min
#undef max
namespace enigma::resource
{
    class ImageResource;
}

namespace enigma::graphic
{
    class D12DepthTexture;
    class RendererSubsystem;
    class D12Texture;
    class BindlessIndexAllocator;
    class ShaderResourceBinder;
    class RenderCommandQueue;
    class IRenderCommand;
    struct TextureCreateInfo;
    struct DepthTextureCreateInfo;
    enum class TextureUsage : uint32_t;
    enum class WorldRenderingPhase : uint32_t;

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
    using namespace enigma::core;
    using namespace enigma::resource;
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
         * 初始化DirectX 12渲染系统（设备、命令系统和SwapChain）
         * DirectX 12 API: D3D12CreateDevice, CreateDXGIFactory2, CreateSwapChainForHwnd
         * 对应IrisRenderSystem.initialize()方法的完整DirectX 12实现
         *
         * 教学要点：
         * - 这是引擎层的统一初始化入口点，遵循 RendererSubsystem → D3D12RenderSystem → SwapChain 初始化流程
         * - SwapChain自动创建，无需应用层手动管理
         * - 完整的DirectX 12系统一次性初始化
         *
         * @param enableDebugLayer 是否启用调试层
         * @param enableGPUValidation 是否启用GPU验证
         * @param hwnd 窗口句柄，用于SwapChain创建（如果为nullptr则不创建SwapChain）
         * @param renderWidth 渲染分辨率宽度（默认1280）
         * @param renderHeight 渲染分辨率高度（默认720）
         * @return 是否初始化成功
         */
        static bool Initialize(bool enableDebugLayer = true, bool        enableGPUValidation = false,
                               HWND hwnd             = nullptr, uint32_t renderWidth         = 1280, uint32_t renderHeight = 720);

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
        static std::unique_ptr<D12Texture>      CreateTexture(TextureCreateInfo& createInfo);
        static std::unique_ptr<D12DepthTexture> CreateDepthTexture(DepthTextureCreateInfo& createInfo);

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


        // ===== Image-based纹理创建API（Bindless集成） =====

        /**
         * 从Image对象创建DirectX 12纹理（支持缓存）
         */
        static std::shared_ptr<D12Texture> CreateTexture2D(Image& image, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const class ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const class ImageResource& imageResource, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName = "");

        static void   ClearUnusedTextures();
        static size_t GetTextureCacheSize();
        static void   ClearAllTextureCache();

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

        // ===== SwapChain管理API （基于A/A/A决策的直接管理）=====

        /**
         * 创建SwapChain（基于2.A决策 - D3D12RenderSystem直接管理）
         * @param hwnd 窗口句柄
         * @param width 缓冲区宽度
         * @param height 缓冲区高度
         * @param bufferCount 后台缓冲区数量（默认3个）
         * @return 是否创建成功
         */
        static bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount = 3);

        /**
         * 获取当前SwapChain后台缓冲区的RTV句柄
         * @return 当前RTV句柄，用于ClearRenderTargetView和OMSetRenderTargets
         */
        static D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentSwapChainRTV();

        /**
         * 获取当前SwapChain后台缓冲区资源
         * @return 当前后台缓冲区ID3D12Resource指针
         */
        static ID3D12Resource* GetCurrentSwapChainBuffer();

        /**
         * 呈现当前帧到屏幕
         * @param vsync 是否启用垂直同步（默认true）
         * @return 是否呈现成功
         */
        static bool Present(bool vsync = true);

        /**
         * 准备下一帧渲染（更新后台缓冲区索引）
         */
        static void PrepareNextFrame();

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

        // ===== 渲染管线API (Milestone 2.6新增) =====

        /**
         * 开始帧渲染 - 清屏操作的正确位置
         * 在标准DirectX 12管线中，Clear操作应该在BeginFrame执行，而非EndFrame
         *
         * 教学要点：
         * - 这是每帧渲染的起始点，在这里执行清屏和初始化
         * - 遵循DirectX 12最佳实践：BeginFrame → 清屏 → 渲染 → EndFrame
         * - 替代了TestClearScreen中的测试逻辑，成为正式渲染管线的一部分
         * - 使用引擎Rgba8颜色系统，提供类型安全和便捷的颜色操作
         *
         * @param clearColor 清屏颜色，使用引擎Rgba8类型(可选，默认为黑色)
         * @param clearDepth 深度缓冲清除值(可选，默认为1.0f)
         * @param clearStencil 模板缓冲清除值(可选，默认为0)
         * @return 是否成功开始帧渲染
         */
        static bool BeginFrame(const Rgba8& clearColor = Rgba8::BLACK, float clearDepth = 1.0f, uint8_t clearStencil = 0);

        /**
         * 清除渲染目标
         * DirectX 12 API: ID3D12GraphicsCommandList::ClearRenderTargetView()
         *
         * @param commandList 要使用的命令列表(如果为nullptr则自动获取)
         * @param rtvHandle 渲染目标视图句柄(如果为空则使用当前SwapChain RTV)
         * @param clearColor 清屏颜色，使用引擎Rgba8类型(如果使用默认值则为黑色)
         * @return 是否清除成功
         */
        static bool ClearRenderTarget(ID3D12GraphicsCommandList*         commandList = nullptr,
                                      const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle   = nullptr,
                                      const Rgba8&                       clearColor  = Rgba8::BLACK);

        /**
         * 清除深度模板缓冲
         * DirectX 12 API: ID3D12GraphicsCommandList::ClearDepthStencilView()
         *
         * @param commandList 要使用的命令列表(如果为nullptr则自动获取)
         * @param dsvHandle 深度模板视图句柄(如果为空则使用默认深度缓冲)
         * @param clearDepth 深度清除值(0.0-1.0)
         * @param clearStencil 模板清除值(0-255)
         * @param clearFlags 清除标志(深度、模板或两者)
         * @return 是否清除成功
         */
        static bool ClearDepthStencil(ID3D12GraphicsCommandList*         commandList = nullptr,
                                      const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle   = nullptr,
                                      float                              clearDepth  = 1.0f, uint8_t clearStencil = 0,
                                      D3D12_CLEAR_FLAGS                  clearFlags  = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);

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

        // ===== SM6.6 Bindless资源管理API (Milestone 2.7重构) =====

        /**
         * @brief 获取Bindless索引分配器
         * @return BindlessIndexAllocator指针
         *
         * 教学要点:
         * 1. 纯索引分配器：只负责索引的分配和释放（0-1,999,999）
         * 2. 职责分离：索引管理与描述符创建完全分离
         * 3. 资源使用流程：
         *    - 分配索引：allocator->AllocateTextureIndex()
         *    - 存储索引：resource->SetBindlessIndex(index)
         *    - 创建描述符：resource->CreateDescriptorInGlobalHeap(device, heapManager)
         */
        static BindlessIndexAllocator* GetBindlessIndexAllocator();

        /**
         * @brief 获取全局描述符堆管理器
         * @return GlobalDescriptorHeapManager指针
         *
         * 教学要点:
         * 1. 全局堆管理：1,000,000容量的CBV_SRV_UAV堆 + 独立RTV/DSV堆
         * 2. SM6.6索引创建：提供CreateShaderResourceView(device, resource, desc, index)等方法
         * 3. 描述符堆绑定：提供SetDescriptorHeaps(commandList)方法
         */
        static GlobalDescriptorHeapManager* GetGlobalDescriptorHeapManager();

        /**
         * @brief 获取SM6.6 Bindless Root Signature
         * @return Root Signature对象
         *
         * 教学要点:
         * 1. 极简Root Signature：只含128字节Root Constants
         * 2. 全局唯一：所有PSO共享同一个Root Signature
         * 3. 直接索引标志：D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
         * 4. 性能优化：Root Signature切换从1000次/帧降至1次/帧（99.9%优化）
         */
        static ID3D12RootSignature* GetBindlessRootSignature();

        // ===== PSO创建API (Milestone 3.0新增) =====

        /**
         * @brief 创建图形管线状态对象 (PSO)
         * @param desc PSO描述符
         * @return ComPtr<ID3D12PipelineState> PSO智能指针
         *
         * 教学要点:
         * 1. 对应Iris的Program创建（OpenGL glCreateProgram + glLinkProgram）
         * 2. PSO封装了完整的图形管线状态（着色器、混合、光栅化、深度模板等）
         * 3. 使用ComPtr自动管理生命周期
         * 4. 失败时返回nullptr，调用者需检查
         *
         * DirectX 12 API:
         * - ID3D12Device::CreateGraphicsPipelineState()
         *
         * 使用示例:
         * @code
         * D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
         * psoDesc.pRootSignature = GetBindlessRootSignature();
         * psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
         * psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
         * // ... 配置其他状态 ...
         * auto pso = D3D12RenderSystem::CreateGraphicsPSO(psoDesc);
         * @endcode
         */
        static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPSO(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
        );

        // ===== Immediate模式渲染API (Milestone 2.6新增) =====

        /**
         * 获取Immediate模式渲染命令队列
         * @return RenderCommandQueue指针，用于immediate模式指令提交
         * @note 遵循分层架构：D3D12RenderSystem管理RenderCommandQueue实例
         *
         * 教学要点：
         * - RenderCommandQueue按WorldRenderingPhase分类存储指令
         * - 支持延迟执行和批量处理优化
         * - 与EnigmaRenderingPipeline的阶段分发机制完美集成
         */
        static RenderCommandQueue* GetRenderCommandQueue();

        /**
         * 添加immediate渲染指令到指定阶段
         * @param phase 目标渲染阶段
         * @param command 渲染指令智能指针
         * @param debugTag 调试标签(可选)
         * @return 是否成功添加
         *
         * 教学要点：
         * - 这是RendererSubsystem.hpp:264绘制指令调用的底层实现
         * - 遵循分层原则：RendererSubsystem → D3D12RenderSystem → RenderCommandQueue
         * - 支持按phase分类，与Iris 10阶段渲染管线完全兼容
         */
        static bool AddImmediateCommand(
            WorldRenderingPhase             phase,
            std::unique_ptr<IRenderCommand> command,
            const std::string&              debugTag = ""
        );

        /**
         * 执行指定阶段的所有immediate命令
         * @param phase 目标渲染阶段
         * @return 执行的指令数量
         *
         * 教学要点：
         * - 这是EnigmaRenderingPipeline::ExecuteDebugStage()等方法的底层实现
         * - 使用现有的CommandListManager获取命令列表，遵循架构一致性
         * - 批量执行优化，减少API调用开销
         * - 自动命令列表管理，简化上层调用
         */
        static size_t ExecuteImmediateCommands(WorldRenderingPhase phase);

        /**
         * 清空指定阶段的immediate命令队列
         * @param phase 目标渲染阶段
         *
         * 教学要点：
         * - 帧结束时清理，避免内存累积
         * - 支持选择性清理，便于调试和性能分析
         */
        static void ClearImmediateCommands(WorldRenderingPhase phase);

        /**
         * 清空所有阶段的immediate命令队列
         *
         * 教学要点：
         * - 帧结束或管线重置时的完整清理
         * - 确保下一帧从干净状态开始
         */
        static void ClearAllImmediateCommands();

        /**
         * 获取指定阶段的immediate命令数量
         * @param phase 目标渲染阶段
         * @return 当前队列中的指令数量
         *
         * 教学要点：
         * - 性能分析和调试信息
         * - 帮助识别渲染瓶颈和优化机会
         */
        static size_t GetImmediateCommandCount(WorldRenderingPhase phase);

        /**
         * 检查指定阶段是否有待执行的immediate命令
         * @param phase 目标渲染阶段
         * @return true如果有命令等待执行
         *
         * 教学要点：
         * - 条件执行优化，跳过空阶段
         * - 减少不必要的API调用和状态切换
         */
        static bool HasImmediateCommands(WorldRenderingPhase phase);

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

        // SwapChain管理（基于A/A/A决策的直接管理）
        static Microsoft::WRL::ComPtr<IDXGISwapChain3> s_swapChain; // DXGI SwapChain
        static Microsoft::WRL::ComPtr<ID3D12Resource>  s_swapChainBuffers[3]; // SwapChain后台缓冲区
        static D3D12_CPU_DESCRIPTOR_HANDLE             s_swapChainRTVs[3]; // SwapChain RTV句柄
        static uint32_t                                s_currentBackBufferIndex; // 当前后台缓冲区索引
        static uint32_t                                s_swapChainBufferCount; // SwapChain缓冲区数量

        // 命令系统管理（对应IrisRenderSystem的命令管理职责）
        static std::unique_ptr<CommandListManager> s_commandListManager; // 命令列表管理器

        // SM6.6 Bindless资源管理系统（Milestone 2.7重构）
        static std::unique_ptr<BindlessIndexAllocator>      s_bindlessIndexAllocator; // 纯索引分配器（0-1,999,999）
        static std::unique_ptr<GlobalDescriptorHeapManager> s_globalDescriptorHeapManager; // 全局描述符堆管理器（1M容量）
        static std::unique_ptr<BindlessRootSignature>       s_bindlessRootSignature; // SM6.6极简Root Signature

        // Immediate模式渲染系统（Milestone 2.6新增）
        static std::unique_ptr<RenderCommandQueue> s_renderCommandQueue; // Immediate模式命令队列

        // 纹理缓存系统（Milestone Bindless新增）
        static std::unordered_map<ResourceLocation, std::weak_ptr<D12Texture>> s_textureCache;
        static std::mutex                                                      s_textureCacheMutex; // 缓存互斥锁（线程安全）


        // 系统状态（移除重复的配置结构）
        static bool s_isInitialized; // 初始化状态

        // ===== 内部初始化方法 =====
        static bool CreateDevice(bool enableGPUValidation); // 简化的设备创建方法
        static void EnableDebugLayer();

        // ===== 内部辅助方法 =====
        static size_t AlignConstantBufferSize(size_t size); // 常量缓冲区大小对齐
    };
} // namespace enigma::graphic
