#pragma once

#include "FrameContext.hpp"
#include "../../Resource/Buffer/D12Buffer.hpp"
#include "../../Resource/CommandListManager.hpp"
#include "QueueRoutingPolicy.hpp"
#include "../EnigmaGraphicCommon.hpp"
#include "../../Resource/BindlessIndexAllocator.hpp"                // SM6.6 bindless index allocator
#include "../../Resource/GlobalDescriptorHeapManager.hpp"           // Global descriptor heap manager
#include "../../Resource/BindlessRootSignature.hpp"                 // SM6.6 root signature
#include "../../Mipmap/MipmapConfig.hpp"                            // MipmapConfig for CreateTexture2DWithMips
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
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"

#undef min
#undef max
namespace enigma::resource
{
    class ImageResource;
}

namespace enigma::graphic
{
    class D12DepthTexture;
    class BufferTransferCoordinator;
    class RendererSubsystem;
    class D12Texture;
    class BindlessIndexAllocator;
    class ShaderResourceBinder;
    class IRenderCommand;
    struct TextureCreateInfo;
    struct DepthTextureCreateInfo;
    enum class TextureUsage : uint32_t;

    struct FrameSlotAcquisitionResult
    {
        bool                 success                      = false;
        uint32_t             frameIndex                   = 0;
        uint32_t             waitedFrameSlot              = 0;
        bool                 hasTrackedRetirement         = false;
        bool                 waitedOnRetirement           = false;
        uint32_t             requestedFramesInFlightDepth = 0;
        uint32_t             activeFramesInFlightDepth    = 0;
        FrameLifecyclePhase  lifecyclePhase               = FrameLifecyclePhase::Idle;
        QueueFenceSnapshot   completedFenceSnapshotBeforeWait = {};
        std::array<bool, kCommandQueueTypeCount> participatingQueues = {};
        std::array<bool, kCommandQueueTypeCount> expectedQueues      = {};
        std::array<bool, kCommandQueueTypeCount> missingQueues       = {};
        std::array<bool, kCommandQueueTypeCount> waitedQueues        = {};
        QueueSubmissionToken graphicsToken              = {};
        QueueSubmissionToken computeToken               = {};
        QueueSubmissionToken copyToken                  = {};
    };

    /**
     * Static DX12 backend facade used by RendererSubsystem.
     * Owns device setup, queue coordination, resource creation, and frame execution state.
     */
    using namespace enigma::core;
    using namespace enigma::resource;
    class D3D12RenderSystem
    {
    public:
        /**
         * Initialize the DX12 device, command system, and optional swap chain.
         *
         * @param enableDebugLayer Enables the D3D12 debug layer.
         * @param enableGPUValidation Enables GPU validation.
         * @param hwnd Window handle used for swap-chain creation. Pass nullptr for headless mode.
         * @param renderWidth Initial render width.
         * @param renderHeight Initial render height.
         * @return True on success.
         */
        static bool Initialize(bool        enableDebugLayer = true, bool enableGPUValidation = false, HWND hwnd = nullptr, uint32_t renderWidth = 1920, uint32_t renderHeight = 1080,
                               DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM);
        static bool PrepareDefaultTextures();
        /**
         * Shutdown the render system and release device, queues, and command resources.
         */
        static void Shutdown();

        /**
         * Returns true once the render system is initialized.
         */
        static bool IsInitialized() { return s_device != nullptr; }

        static void SetViewport(int width, int height);

        // ===== Buffer creation API =====

        /**
         * Create a generic buffer resource with optional initial data upload.
         */
        static std::unique_ptr<D12Buffer> CreateBuffer(const BufferCreateInfo& createInfo);

        // ===== Type-safe vertex/index buffer creation =====

        /**
         * Create a type-safe vertex buffer from the unified create-info contract.
         */
        static std::unique_ptr<class D12VertexBuffer> CreateVertexBuffer(const BufferCreateInfo& createInfo);

        /**
         * Legacy convenience wrapper for dynamic-upload vertex buffers.
         */
        static std::unique_ptr<class D12VertexBuffer> CreateVertexBuffer(
            size_t      size,
            size_t      stride,
            const void* initialData = nullptr,
            const char* debugName   = "VertexBuffer"
        );

        /**
         * Create a type-safe index buffer from the unified create-info contract.
         */
        static std::unique_ptr<class D12IndexBuffer> CreateIndexBuffer(const BufferCreateInfo& createInfo);

        /**
         * Legacy convenience wrapper for dynamic-upload index buffers.
         */
        static std::unique_ptr<class D12IndexBuffer> CreateIndexBuffer(
            size_t      size,
            const void* initialData = nullptr,
            const char* debugName   = "IndexBuffer"
        );

        /**
         * Bind a vertex buffer wrapper directly.
         */
        static void BindVertexBuffer(const class D12VertexBuffer* vertexBuffer, UINT slot = 0);

        /**
         * Bind an index buffer wrapper directly.
         */
        static void BindIndexBuffer(const class D12IndexBuffer* indexBuffer);

        /**
         * Create a constant buffer with 256-byte alignment.
         */
        static std::unique_ptr<D12Buffer> CreateConstantBuffer(
            size_t      size,
            const void* initialData = nullptr,
            const char* debugName   = "ConstantBuffer");

        /**
         * Create a structured buffer for SSBO-style workloads.
         */
        static std::unique_ptr<D12Buffer> CreateStructuredBuffer(
            size_t      elementCount,
            size_t      elementSize,
            const void* initialData = nullptr,
            const char* debugName   = "StructuredBuffer");

        // ===== Texture creation API =====

        /**
         * Create a texture resource and its bindless-facing descriptors.
         */
        static std::unique_ptr<D12Texture>      CreateTexture(TextureCreateInfo& createInfo);
        static std::unique_ptr<D12DepthTexture> CreateDepthTexture(DepthTextureCreateInfo& createInfo);

        /**
         * Create a 2D texture with an explicit usage mask.
         */
        static std::unique_ptr<D12Texture> CreateTexture2D(
            uint32_t     width,
            uint32_t     height,
            DXGI_FORMAT  format,
            TextureUsage usage,
            const void*  initialData = nullptr,
            const char*  debugName   = "Texture2D");

        /**
         * Create a shader-resource 2D texture.
         */
        static std::unique_ptr<D12Texture> CreateTexture2D(
            uint32_t    width,
            uint32_t    height,
            DXGI_FORMAT format,
            const void* initialData = nullptr,
            const char* debugName   = "Texture2D");


        // ===== Image-backed texture creation =====

        /**
         * Create a cached DX12 texture from an Image object.
         */
        static std::shared_ptr<D12Texture> CreateTexture2D(Image& image, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const class ResourceLocation& resourceLocation, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const class ImageResource& imageResource, TextureUsage usage, const std::string& debugName = "");
        static std::shared_ptr<D12Texture> CreateTexture2D(const std::string& imagePath, TextureUsage usage, const std::string& debugName = "");

        /// Create texture with full mip chain and auto-generate mipmaps via compute shader.
        /// Adds UnorderedAccess flag automatically for mip generation.
        /// Safe to call during initialization (self-acquires command list if needed).
        /// @param maxMipLevels 0 = full chain, >0 = limit to this many levels
        /// @param mipConfig    Mipmap generation config (FilterMode, custom shader, etc.)
        static std::shared_ptr<D12Texture> CreateTexture2DWithMips(
            Image& image, TextureUsage usage, const std::string& debugName = "",
            uint32_t maxMipLevels = 0,
            const MipmapConfig& mipConfig = MipmapConfig::Default());

        static void   ClearUnusedTextures();
        static size_t GetTextureCacheSize();
        static void   ClearAllTextureCache();

        // ===== Device access API =====

        /**
         * Get the native DX12 device.
         */
        static ID3D12Device* GetDevice() { return s_device.Get(); }

        /**
         * Get the RTV for the current back buffer.
         */
        static D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV();

        // ===== Default fallback textures =====

        /**
         * Get the default white texture used by material fallback paths.
         */
        static std::shared_ptr<D12Texture> GetDefaultWhiteTexture();

        /**
         * Get the default black texture used by material fallback paths.
         */
        static std::shared_ptr<D12Texture> GetDefaultBlackTexture();

        /**
         * Get the default flat normal map used by material fallback paths.
         */
        static std::shared_ptr<D12Texture> GetDefaultNormalTexture();

        /**
         * Get the current back-buffer resource.
         */
        static ID3D12Resource* GetBackBufferResource();

        /**
         * @brief Get the DXGI format of the backbuffer
         * @return DXGI_FORMAT_R8G8B8A8_UNORM (matches SwapChain creation)
         * @note Used by PresentRenderTarget for format compatibility check
         */
        static DXGI_FORMAT GetBackBufferFormat();

        /**
         * Get the DXGI factory used for swap-chain creation.
         */
        static IDXGIFactory4* GetDXGIFactory() { return s_dxgiFactory.Get(); }

        /**
         * Get the selected graphics adapter.
         */
        static IDXGIAdapter1* GetAdapter() { return s_adapter.Get(); }

        // ===== Command management API =====

        /**
         * Get the command-list manager owned by the render system.
         */
        static CommandListManager* GetCommandListManager();

        /**
         * @brief Set the requested queue execution mode for future routing decisions.
         */
        static void SetRequestedQueueExecutionMode(QueueExecutionMode mode);

        /**
         * @brief Get the requested queue execution mode.
         */
        static QueueExecutionMode GetRequestedQueueExecutionMode();

        /**
         * @brief Get the currently active queue execution mode after fallback resolution.
         */
        static QueueExecutionMode GetActiveQueueExecutionMode();

        /**
         * @brief Get queue execution diagnostics collected by the runtime.
         */
        static const QueueExecutionDiagnostics& GetQueueExecutionDiagnostics();

        /**
         * @brief Force a full synchronization across every active queue before CPU-side resource mutation.
         * @param reason Optional debug string describing why the full sync is required.
         * @return True when all active queues are idle and completed wrappers are recycled.
         */
        static bool SynchronizeActiveQueues(const char* reason = nullptr);

        static QueueSubmittedFenceSnapshot GetLastSubmittedFenceSnapshot();
        static QueueFenceSnapshot GetCompletedFenceSnapshot();

        // ===== Swap-chain API =====

        /**
         * Create the swap chain managed directly by D3D12RenderSystem.
         */
        static bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount = 3);

        /**
         * Get the RTV of the current swap-chain back buffer.
         */
        static D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentSwapChainRTV();

        /**
         * Get the current swap-chain back-buffer resource.
         */
        static ID3D12Resource* GetCurrentSwapChainBuffer();

        /**
         * Present the current frame.
         */
        static bool Present(bool vsync = true);

        /**
         * Advance to the next frame slot and back-buffer index.
         */
        static void PrepareNextFrame();

        // ===== Debug API =====

        /**
         * Assign a debug name to a native DX12 object.
         */
        static void SetDebugName(ID3D12Object* object, const char* name);

        /**
         * Query whether a DX12 feature is supported.
         */
        static bool CheckFeatureSupport(D3D12_FEATURE feature);

        // ===== Memory API =====

        /**
         * Query local GPU memory usage.
         */
        static DXGI_QUERY_VIDEO_MEMORY_INFO GetVideoMemoryInfo();

        // ===== Frame execution API =====

        /**
         * Begin the current frame, retire the frame slot, and clear the swap chain.
         */
        static bool BeginFrame(const Rgba8& clearColor = Rgba8::BLACK, float clearDepth = 1.0f, uint8_t clearStencil = 0);

        /**
         * End the current frame, submit the main graphics work, and present.
         */
        static bool EndFrame();

        /**
         * Clear a render target using the supplied or implicit command list.
         */
        static bool ClearRenderTarget(ID3D12GraphicsCommandList*         commandList = nullptr,
                                      const D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandle   = nullptr,
                                      const Rgba8&                       clearColor  = Rgba8::BLACK);

        /**
         * Clear a depth-stencil target using the supplied or implicit command list.
         */
        static bool ClearDepthStencil(ID3D12GraphicsCommandList*         commandList = nullptr,
                                      const D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle   = nullptr,
                                      float                              clearDepth  = 1.0f, uint8_t clearStencil = 0,
                                      D3D12_CLEAR_FLAGS                  clearFlags  = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);

        // ===== Config-based Clear API - RenderTarget Format Refactor =====

        /**
         * @brief Clear render target using RTConfig settings
         * @param commandList Command list to record clear command
         * @param rtvHandle RTV handle of the render target
         * @param config RTConfig containing clearValue and loadAction
         *
         * Only clears if config.loadAction == LoadAction::Clear.
         * Uses config.clearValue for the clear color.
         */
        static void ClearRenderTargetByConfig(
            ID3D12GraphicsCommandList*       commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE      rtvHandle,
            const struct RenderTargetConfig& config
        );

        /**
         * @brief Clear depth-stencil using RTConfig settings
         * @param commandList Command list to record clear command
         * @param dsvHandle DSV handle of the depth-stencil buffer
         * @param config RTConfig containing clearValue and loadAction
         *
         * Only clears if config.loadAction == LoadAction::Clear.
         * Uses config.clearValue.depthStencil for clear values.
         */
        static void ClearDepthStencilByConfig(
            ID3D12GraphicsCommandList*       commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE      dsvHandle,
            const struct RenderTargetConfig& config
        );

        // ===== Resource creation API =====

        /**
         * Shared committed-resource creation helper used by engine resource wrappers.
         */
        static HRESULT CreateCommittedResource(
            const D3D12_HEAP_PROPERTIES& heapProps,
            const D3D12_RESOURCE_DESC&   desc,
            D3D12_RESOURCE_STATES        initialState,
            const D3D12_CLEAR_VALUE*     pOptimizedClearValue,
            ID3D12Resource**             resource);

        // ===== Bindless resource API =====

        /**
         * Get the bindless index allocator.
         */
        static BindlessIndexAllocator* GetBindlessIndexAllocator();

        /**
         * Get the global descriptor-heap manager.
         */
        static GlobalDescriptorHeapManager* GetGlobalDescriptorHeapManager();

        /**
         * Get the shared bindless root signature.
         */
        static BindlessRootSignature* GetBindlessRootSignature();

        // ===== Draw-state API =====

        /**
         * Bind a vertex-buffer view to the input assembler.
         */
        static void BindVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& bufferView, UINT slot = 0);

        /**
         * Bind an index-buffer view to the input assembler.
         */
        static void BindIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& bufferView);

        /**
         * Issue a non-indexed draw on the active graphics command list.
         */
        static void Draw(UINT vertexCount, UINT startVertex = 0);

        /**
         * Issue an indexed draw on the active graphics command list.
         */
        static void DrawIndexed(UINT indexCount, UINT startIndex = 0, INT baseVertex = 0);

        /**
         * Set the primitive topology for subsequent draws.
         */
        static void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);

        // ===== PSO API =====

        /**
         * Create a graphics pipeline state object.
         */
        static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPSO(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
        );

        // ===== Resource-barrier API =====

        /**
         * Transition a resource between explicit DX12 states.
         */
        static void TransitionResource(
            ID3D12GraphicsCommandList* cmdList,
            ID3D12Resource*            resource,
            D3D12_RESOURCE_STATES      stateBefore,
            D3D12_RESOURCE_STATES      stateAfter,
            const char*                debugName = nullptr
        );

        /**
         * @brief Insert a UAV barrier to synchronize unordered access writes
         * @param cmdList Active command list
         * @param resource The resource to synchronize (nullptr = all UAV resources)
         *
         * Unlike TransitionResource, this does NOT change the resource state.
         * It ensures all prior UAV writes are visible before subsequent reads/writes.
         * Typical use: between compute shader dispatches that read from the
         * previous dispatch's output (e.g. mipmap chain generation).
         */
        static void UAVBarrier(
            ID3D12GraphicsCommandList* cmdList,
            ID3D12Resource*            resource
        );

        /**
         * @brief Allocate a bindless texture index and create a UAV descriptor
         * @param resource The D3D12 resource to create the UAV for
         * @param desc UAV description (caller builds format, dimension, mip slice, etc.)
         * @return Bindless index for the UAV, or INVALID_INDEX on failure
         *
         * Generic factory: caller is responsible for building the desc and
         * freeing the returned index via BindlessIndexAllocator::FreeTextureIndex().
         */
        static uint32_t CreateUAV(
            ID3D12Resource*                              resource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC&      desc
        );

        // ===== Command-list access API =====

        /**
         * Get the active graphics command list for the current frame.
         */
        static ID3D12GraphicsCommandList* GetCurrentCommandList() { return s_currentGraphicsCommandList; }

        // ===== Frame Resource API (Multi-Frame In-Flight) =====

        // Current frame slot index [0, MAX_FRAMES_IN_FLIGHT)
        static uint32_t GetFrameIndex() { return s_frameIndex; }

        // Global monotonically increasing frame counter
        static uint64_t GetFrameCount() { return s_globalFrameCount; }

        // Current frame lifecycle phase for development-time orchestration checks.
        static FrameLifecyclePhase GetFrameLifecyclePhase() { return s_frameLifecyclePhase; }

        // Most recent frame-slot acquisition result captured by BeginFrame().
        static const FrameSlotAcquisitionResult& GetLastFrameSlotAcquisitionResult() { return s_lastFrameSlotAcquisitionResult; }

        // Update the frame lifecycle phase from renderer orchestration code.
        static void SetFrameLifecyclePhase(FrameLifecyclePhase phase);

    private:
        friend class D12Resource;
        friend class BufferTransferCoordinator;
        friend class MipmapGenerator;

        // Static-only type.
        D3D12RenderSystem()                                    = delete;
        ~D3D12RenderSystem()                                   = delete;
        D3D12RenderSystem(const D3D12RenderSystem&)            = delete;
        D3D12RenderSystem& operator=(const D3D12RenderSystem&) = delete;

        // ===== Core DX12 objects =====
        static Microsoft::WRL::ComPtr<ID3D12Device>  s_device; // Primary device
        static Microsoft::WRL::ComPtr<IDXGIFactory4> s_dxgiFactory; // DXGI factory
        static Microsoft::WRL::ComPtr<IDXGIAdapter1> s_adapter; // Selected adapter

        // Swap-chain state.
        static Microsoft::WRL::ComPtr<IDXGISwapChain3> s_swapChain; // DXGI swap chain
        static Microsoft::WRL::ComPtr<ID3D12Resource>  s_swapChainBuffers[3]; // Swap-chain back buffers
        static D3D12_CPU_DESCRIPTOR_HANDLE             s_swapChainRTVs[3]; // Swap-chain RTVs
        static uint32_t                                s_currentBackBufferIndex; // Active back-buffer index
        static uint32_t                                s_swapChainBufferCount; // Swap-chain buffer count
        static uint32_t                                s_swapChainWidth; // Swap-chain width
        static uint32_t                                s_swapChainHeight; // Swap-chain height
        static DXGI_FORMAT                             s_backbufferFormat; // Backbuffer format (configurable via renderer.yml)

        // Command-system state.
        static std::unique_ptr<CommandListManager> s_commandListManager; // Command-list manager
        static ID3D12GraphicsCommandList*          s_currentGraphicsCommandList; // Active graphics command list

        // Bindless state.
        static std::unique_ptr<BindlessIndexAllocator>      s_bindlessIndexAllocator; // Bindless index allocator
        static std::unique_ptr<GlobalDescriptorHeapManager> s_globalDescriptorHeapManager; // Global descriptor heap manager
        static std::unique_ptr<BindlessRootSignature>       s_bindlessRootSignature; // Shared bindless root signature

        // Texture cache.
        static std::unordered_map<ResourceLocation, std::weak_ptr<D12Texture>> s_textureCache;
        static std::mutex                                                      s_textureCacheMutex; // Texture-cache mutex

        // Default fallback textures.
        static std::shared_ptr<D12Texture> s_defaultWhiteTexture; // White fallback texture
        static std::shared_ptr<D12Texture> s_defaultBlackTexture; // Black fallback texture
        static std::shared_ptr<D12Texture> s_defaultNormalTexture; // Flat normal fallback texture


        // System state.
        static bool s_isInitialized; // Initialization state
        static bool s_isShuttingDown; // Shutdown fence for backend teardown

        // Multi-Frame In-Flight resources (Phase 1)
        static FrameContext s_frameContexts[4];   // per-frame GPU resources, upper bound 4
        static uint32_t     s_frameIndex;         // current frame slot [0, MAX_FRAMES_IN_FLIGHT)
        static uint64_t     s_globalFrameCount;   // global monotonic frame counter
        static FrameLifecyclePhase       s_frameLifecyclePhase;
        static FrameSlotAcquisitionResult s_lastFrameSlotAcquisitionResult;

        static QueueRoutingPolicy        s_queueRoutingPolicy;
        static QueueExecutionMode        s_requestedQueueExecutionMode;
        static QueueExecutionMode        s_activeQueueExecutionMode;
        static QueueExecutionDiagnostics s_queueExecutionDiagnostics;

        // ===== Internal initialization =====
        static bool CreateDevice(bool enableGPUValidation); // Device creation helper
        static void EnableDebugLayer();

        // ===== Internal helpers =====
        static size_t AlignConstantBufferSize(size_t size); // Constant-buffer alignment helper
        static QueueRouteDecision ResolveQueueRoute(const QueueRouteContext& context);
        static ID3D12GraphicsCommandList* AcquireCommandListForWorkload(const QueueRouteContext& context,
                                                                        ID3D12CommandAllocator* externalAllocator,
                                                                        const std::string&      debugName,
                                                                        bool                    useFrameAllocator);
        static QueueSubmissionToken SubmitFrameScopedCommandList(ID3D12GraphicsCommandList* commandList,
                                                                 const QueueRouteContext&   context);
        static QueueSubmissionToken SubmitStandaloneCommandList(ID3D12GraphicsCommandList* commandList,
                                                                QueueWorkloadClass         workload);
        static void RegisterFrameSubmission(const QueueSubmissionToken& token);
        static void RecordQueueRouteDecision(const QueueRouteDecision& decision);
        static void RecordQueueSubmission(const QueueSubmissionToken& token,
                                          QueueWorkloadClass         workload);
        static void RecordQueueWaitInsertion(CommandQueueType producerQueue,
                                             CommandQueueType consumerQueue);
        static bool WaitForStandaloneSubmission(const QueueSubmissionToken& token);
        static void SetActiveQueueExecutionModeInternal(QueueExecutionMode mode);
        static bool ShouldDeferIndividualResourceRelease();
        static void DeferResourceRelease(ID3D12Resource* resource, const char* debugName);
        static void ProcessDeferredResourceReleases(bool forceRelease = false) noexcept;
        static void DrainQueuesForShutdown() noexcept;
        static void ResetFrameExecutionState() noexcept;
        static void ReleaseFallbackTextures() noexcept;
        static void ReleaseBindlessInfrastructure() noexcept;
        static void ReleaseSwapChainResources() noexcept;
        static void ReportLiveObjectsBeforeDeviceRelease() noexcept;
        static void ReleaseDeviceObjects() noexcept;
    };
} // namespace enigma::graphic
