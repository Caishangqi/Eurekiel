#pragma once
#include <array>
#include <d3d12.h>
#include <map>
#include <unordered_map>

#include "Engine/Renderer/IRenderer.hpp"
#include <vector>
#include <ThirdParty/d3dx12/d3dx12.h>
#include <wrl/client.h>

#include "DirectX12/DescriptorAllocator.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

class Window;
class Shader;
class Texture;
class VertexBuffer;
class IndexBuffer;
class ConstantBuffer;
class Camera;

struct Vertex_PCUTBN;
struct Vertex_PCU;
struct DirectionalLightConstants;
struct BlurConstants;
struct LightConstants;

struct ID3D12Device;
struct IDXGISwapChain3;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12RootSignature;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;
struct ID3D12Fence;
struct ID3D12PipelineState;
struct D3D12_VIEWPORT;
struct D3D12_RASTERIZER_DESC;
struct D3D12_BLEND_DESC;
struct D3D12_DEPTH_STENCIL_DESC;

#define ENGINE_DEBUG_RENDER
static constexpr uint32_t kBackBufferCount         = 4;
constexpr uint32_t        kMaxConstantBufferSlot   = 14;
constexpr uint32_t        kMaxShaderSourceViewSlot = 128;
constexpr uint32_t        kMaxTextureCached        = 2048;
using Microsoft::WRL::ComPtr;

/**
 * Represents the configuration for a rendering pipeline state.
 * Encapsulates various settings that control how rendering operations
 * are carried out, including shaders, blend states, rasterizer states,
 * and other pipeline configurations.
 */
struct RenderState
{
    BlendMode      blendMode      = BlendMode::ALPHA;
    RasterizerMode rasterizerMode = RasterizerMode::SOLID_CULL_NONE;
    DepthMode      depthMode      = DepthMode::READ_WRITE_LESS_EQUAL;
    Shader*        shader         = nullptr; // Bind Shader

    // Add sampler state array, support up to 16 slots
    static constexpr size_t                   kMaxSamplerSlots = 16;
    std::array<SamplerMode, kMaxSamplerSlots> samplerModes;

    RenderState()
    {
        samplerModes.fill(SamplerMode::POINT_CLAMP);
    }

    // In order to be used in a map, a comparison operator is required
    bool operator<(const RenderState& other) const
    {
        return std::tie(blendMode, rasterizerMode, depthMode, shader, samplerModes) <
            std::tie(other.blendMode, other.rasterizerMode, other.depthMode, other.shader, other.samplerModes);
    }

    bool operator==(const RenderState& other) const
    {
        return blendMode == other.blendMode &&
            rasterizerMode == other.rasterizerMode &&
            depthMode == other.depthMode &&
            shader == other.shader &&
            samplerModes == other.samplerModes;
    }

    bool operator!=(const RenderState& other) const
    {
        return !(*this == other);
    }
};

/**
 * Manages a pool of constant buffers for efficient allocation and reuse in GPU rendering operations.
 *
 * This structure is primarily used to handle large constant buffers, avoid frequent memory allocation
 * by reusing temporary buffers, and ensure proper alignment for rendering requirements. It also
 * facilitates mapping and unmapping resources for CPU-GPU interaction.
 *
 * Key responsibilities include:
 * - Maintaining a large pool buffer for allocations.
 * - Managing offsets and sizes within the pool to track memory usage.
 * - Providing temporary constant buffers to reduce overhead from repeated allocations.
 * - Ensuring data alignment according to rendering API requirements (e.g., 256-byte alignment for DX12).
 */
struct ConstantBufferPool
{
    ConstantBuffer*         poolBuffer    = nullptr; // Large pool buffer
    uint8_t*                mappedData    = nullptr; // Mapped CPU address
    size_t                  currentOffset = 0; // Current allocation offset
    size_t                  poolSize      = 0; // Total pool size
    static constexpr size_t ALIGNMENT     = 256; // DX12 requires 256-byte alignment

    // Temporary ConstantBuffer object pool to avoid repeated new/delete
    std::vector<ConstantBuffer*> tempBuffers;
    size_t                       tempBufferIndex = 0;
};

/**
 * Represents a single draw call in a rendering pipeline.
 * Encapsulates all the resources, states, and descriptors needed
 * to execute a draw call, including descriptors for shaders,
 * constant buffers, textures, and the current rendering state.
 */
struct DrawCall
{
    TieredDescriptorHandler::FrameDescriptorTable          descriptorTable;
    RenderState                                            renderState;
    std::array<DescriptorHandle, kMaxConstantBufferSlot>   constantBuffers;
    std::array<DescriptorHandle, kMaxShaderSourceViewSlot> textures;
    UINT                                                   numCBVs = 0;
    UINT                                                   numSRVs = 0;
};

/**
 * Represents a collection of resources bound to the rendering pipeline,
 * including constant buffers and shader resource views (textures).
 * This structure tracks the bindings for GPU resource descriptors
 * and facilitates the management of bound resources during rendering operations.
 */
struct BoundResources
{
    struct BoundCBV
    {
        ConstantBuffer* buffer  = nullptr;
        bool            isValid = false;
    };

    struct BoundSRV
    {
        Texture* texture = nullptr;
        bool     isValid = false;
    };

    std::array<BoundCBV, kMaxConstantBufferSlot>   constantBuffers;
    std::array<BoundSRV, kMaxShaderSourceViewSlot> textures;
    UINT                                           numCBVs = 0;
    UINT                                           numSRVs = 0;
};

// TODO: Package into Texture Class
// Modify the texture structure to store the persistent descriptor handle
struct TextureData
{
    ComPtr<ID3D12Resource> resource;
    ComPtr<ID3D12Resource> uploadHeap;
    DescriptorHandle       persistentSRV; // Persistent SRV in CPU heap
    IntVec2                dimensions;
    std::string            name;
};

/**
 * DirectX 12 implementation of the IRenderer interface.
 * Handles rendering operations and resource management using the DirectX 12 API.
 */
class DX12Renderer final : public IRenderer
{
public:
    explicit DX12Renderer(const RenderConfig& cfg);
    ~DX12Renderer() override;

    void Startup() override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;

    void ClearScreen(const Rgba8& clr) override;
    void BeginCamera(const Camera& cam) override;
    void EndCamera(const Camera& cam) override;

    BitmapFont* CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension) override;
    Shader*     CreateShader(const char* name, const char* src, VertexType t = VertexType::Vertex_PCU) override;
    Shader*     CreateShader(const char* name, VertexType t = VertexType::Vertex_PCU) override;
    Shader*     CreateShader(const char* name, const char* shaderPath, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType) override;
    Shader*     CreateShaderFromSource(const char* name, const char* shaderSource, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType) override;
    Shader*     CreateOrGetShader(const char* shaderName, VertexType vertexType = VertexType::Vertex_PCU) override;
    bool        CompileShaderToByteCode(std::vector<uint8_t>& outByteCode, const char* name, const char* src, const char* entryPoint, const char* target) override;
    Texture*    CreateTextureFromImage(Image& image) override;
    Texture*    CreateOrGetTexture(const char* imageFilePath) override;
    Texture*    CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData) override;
    Texture*    CreateTextureFromFile(const char* imageFilePath) override;
    Texture*    GetTextureForFileName(const char* imageFilePath) override;
    BitmapFont* CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture) override;

    void BindShader(Shader* s) override;

    VertexBuffer*   CreateVertexBuffer(size_t size, unsigned stride) override;
    IndexBuffer*    CreateIndexBuffer(size_t size) override;
    ConstantBuffer* CreateConstantBuffer(size_t size) override;

    void CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset) override;
    void CopyCPUToGPU(const Vertex_PCUTBN* data, size_t size, VertexBuffer* v, size_t offset) override; // directly call general CopyCPUToGPU
    void CopyCPUToGPU(const void* data, size_t sz, VertexBuffer* vbo, size_t off = 0) override;
    void CopyCPUToGPU(const void* data, size_t sz, IndexBuffer* ibo) override;
    void CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cb) override;

    void BindVertexBuffer(VertexBuffer* vbo) override;
    void BindIndexBuffer(IndexBuffer* ibo) override;
    void BindConstantBuffer(int slot, ConstantBuffer* cbo) override;
    void BindTexture(Texture* tex, int slot = 0) override;

    void SetModelConstants(const Mat44& modelToWorldTransform = Mat44(), const Rgba8& tint = Rgba8::WHITE) override;
    void SetDirectionalLightConstants(const DirectionalLightConstants&) override;
    void SetLightConstants(const LightingConstants& lightConstants) override;
    void SetFrameConstants(const FrameConstants& frameConstants) override;
    void SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, int slot) override;

    void SetBlendMode(BlendMode) override;
    void SetRasterizerMode(RasterizerMode) override;
    void SetDepthMode(DepthMode) override;
    void SetSamplerMode(SamplerMode, int slot = 0) override;

    void DrawVertexArray(int n, const Vertex_PCU* v) override;
    void DrawVertexArray(int n, const Vertex_PCUTBN* v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx) override;
    void DrawVertexBuffer(VertexBuffer* vbo, int count) override; // DrawVertexBuffer - handles user buffered data
    void DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount) override;

    RenderTarget* CreateRenderTarget(IntVec2 dimension, DXGI_FORMAT format) override;
    void          SetRenderTarget(RenderTarget* renderTarget) override;
    void          SetRenderTarget(RenderTarget** renderTargets, int count) override;
    void          ClearRenderTarget(RenderTarget* renderTarget, const Rgba8& clearColor) override;
    RenderTarget* GetBackBufferRenderTarget() override;

    void SetViewport(const IntVec2& dimension) override;

private:
    // Device-level resources (StartUp / Shutdown lifecycle)
    RenderConfig                 m_config{};
    ComPtr<ID3D12Device>         m_device                         = nullptr;
    ComPtr<ID3D12Device2>        m_device2                        = nullptr;
    ComPtr<IDXGISwapChain3>      m_swapChain                      = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap           = nullptr;
    UINT                         m_renderTargetViewDescriptorSize = 0;
    ComPtr<ID3D12Resource>       m_backBuffers[kBackBufferCount];
    UINT                         m_currentBackBufferIndex = 0;
    // Main Renderer Command Allocator
    ComPtr<ID3D12GraphicsCommandList> m_commandList      = nullptr;
    ComPtr<ID3D12CommandQueue>        m_commandQueue     = nullptr;
    ComPtr<ID3D12CommandAllocator>    m_commandAllocator = nullptr;
    // Upload Renderer Command Allocator
    ComPtr<ID3D12GraphicsCommandList> m_uploadCommandList      = nullptr;
    ComPtr<ID3D12CommandQueue>        m_copyCommandQueue       = nullptr;
    ComPtr<ID3D12CommandAllocator>    m_uploadCommandAllocator = nullptr;

    // Fence and synchronize
    ComPtr<ID3D12Fence> m_fence      = nullptr;
    UINT64              m_fenceValue = 0;
    HANDLE              m_fenceEvent = nullptr;

    /// Vertex Buffers
    VertexBuffer* m_currentVertexBuffer = nullptr;
    // Updated ring vertex buffer
    std::array<VertexBuffer*, kBackBufferCount> m_frameVertexBuffer{nullptr};
    static constexpr size_t                     kVertexRingSize = sizeof(Vertex_PCU) * 1024 * 1024; // 96MB

    std::array<ConversionBuffer, kBackBufferCount> m_conversionBuffers;
    ConversionBuffer*                              m_currentConversionBuffer = nullptr;
    /// Index Buffers  
    IndexBuffer* m_currentIndexBuffer = nullptr;

    // Updating ring index buffer
    std::array<IndexBuffer*, kBackBufferCount> m_frameIndexBuffer{nullptr};
    static constexpr size_t                    kIndexRingSize = sizeof(unsigned int) * 256 * 1024; // 1MB for indices

    /// Constant buffer pools
    std::array<ConstantBufferPool, kBackBufferCount> m_constantBufferPools;
    static constexpr size_t                          kConstantBufferPoolSize = (size_t)2 * 1024 * 1024;

    /// Descriptor handler
    std::unique_ptr<TieredDescriptorHandler> m_descriptorHandler;

    /// Draw calls
    DrawCall m_currentDrawCall;

    std::unordered_map<Texture*, std::unique_ptr<TextureData>> m_textureData;


    /// depth Buffer
    ComPtr<ID3D12Resource>       m_depthStencilBuffer             = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilViewDescriptorHeap = nullptr;
    /// Root signature that expose shader needed data and buffers to GPU
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    /// define viewport
    D3D12_VIEWPORT m_viewport{};
    /// define scissor rect
    CD3DX12_RECT m_scissorRect{};

    BoundResources m_boundResources;

    Texture*                 m_defaultTexture = nullptr;
    Shader*                  m_defaultShader  = nullptr;
    Shader*                  m_currentShader  = nullptr;
    std::vector<BitmapFont*> m_loadedFonts;
    std::vector<Texture*>    m_textureCache; // key: 文件路径
    std::vector<Shader*>     m_shaderCache; // key: Shader 名称
    std::vector<BitmapFont*> m_fontsCache;

private:
    void UploadToCB(ConstantBuffer* cb, const void* data, size_t size);
    void WaitForGPU();

    /// Descriptor Set
    void CommitDescriptorsForCurrentDraw();
    void PrepareNextDrawCall();

    /// Constant buffer pool
    void            InitializeConstantBufferPools();
    ConstantBuffer* AllocateFromConstantBufferPool(size_t dataSize);
    ConstantBuffer* GetTempConstantBuffer(ConstantBufferPool& pool, size_t alignedSize);

private:
    /**
     * Represents a unique key for defining a sampler state configuration.
     * This structure is used to store and compare sampler modes across multiple
     * sampler slots, enabling efficient caching and lookup of states in rendering systems.
     */
    struct SamplerStateKey
    {
        std::array<SamplerMode, RenderState::kMaxSamplerSlots> samplerModes;

        bool operator<(const SamplerStateKey& other) const
        {
            return samplerModes < other.samplerModes;
        }
    };

    std::map<SamplerStateKey, ComPtr<ID3D12RootSignature>> m_rootSignatureCache;

    // PSO Base Templates - These are the fixed parts shared by all PSOs
    // TODO: Consider use hash value to directly access the memory
    struct PSOTemplate
    {
        D3D12_INPUT_ELEMENT_DESC      inputLayout[6]; // PCU layout
        UINT                          inputLayoutCount;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;
        ComPtr<ID3DBlob>              defaultVertexShader;
        ComPtr<ID3DBlob>              defaultPixelShader;
        DXGI_FORMAT                   renderTargetFormat;
        DXGI_FORMAT                   depthStencilFormat;
        ComPtr<ID3D12RootSignature>   rootSignature;
    } m_psoTemplate = {};

    /// Pipeline state cache, Cache the created PSO based on the state combination
    std::map<RenderState, ComPtr<ID3D12PipelineState>> m_pipelineStateCache;
    /// @FIX: DirectX 12 requires that all resources referenced by the command list must remain valid until the command list is executed.
    /// When change the BlendMode, a new PSO is created. The old PSO reference is overwritten by the new one, causing the reference count to zero and be released.
    /// However, the old PSO is still referenced in the recorded command list.
    std::vector<ComPtr<ID3D12PipelineState>> m_frameUsedPSOs;

    // Current waiting application rendering status
    RenderState                 m_pendingRenderState;
    ComPtr<ID3D12PipelineState> m_currentPipelineStateObject = nullptr;


    // Get or create the PSO of the corresponding state
    [[maybe_unused]] ComPtr<ID3D12PipelineState> GetOrCreatePipelineState(const RenderState& state);
    // Auxiliary method for creating PSO based on status
    [[nodiscard]] ComPtr<ID3D12PipelineState> CreatePipelineStateForRenderState(const RenderState& state);
    [[nodiscard]] ID3D12RootSignature*        GetOrCreateRootSignature(const std::array<SamplerMode, RenderState::kMaxSamplerSlots>& samplerModes);
    [[nodiscard]] CD3DX12_STATIC_SAMPLER_DESC GetSamplerDesc(SamplerMode mode, UINT shaderRegister);

    /// Internal auxiliary function: only responsible for executing drawing commands, not copying data
    [[maybe_unused]] void DrawVertexBufferInternal(VertexBuffer* vbo, int count);
    [[maybe_unused]] void DrawVertexIndexedInternal(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount);
};
