#pragma once
#include <array>
#include <d3d12.h>
#include <map>

#include "Engine/Renderer/IRenderer.hpp"
#include <unordered_map>
#include <vector>
#include <ThirdParty/d3dx12/d3dx12.h>
#include <wrl/client.h>

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
static constexpr uint32_t kBackBufferCount         = 2;
static constexpr size_t   kVertexRingSize          = sizeof(Vertex_PCU) * 1024 * 1024; // 96MB
constexpr uint32_t        kMaxConstantBufferSlot   = 14;
constexpr uint32_t        kMaxShaderSourceViewSlot = 128;
constexpr uint32_t        kMaxTextureCached        = 4096;
using Microsoft::WRL::ComPtr;


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

    Shader*     CreateShader(const char* name, const char* src, VertexType t = VertexType::Vertex_PCU) override;
    Shader*     CreateShader(const char* name, VertexType t = VertexType::Vertex_PCU) override;
    Shader*     CreateOrGetShader(const char* shaderName) override;
    bool        CompileShaderToByteCode(std::vector<uint8_t>& outByteCode, const char* name, const char* source, const char* entryPoint, const char* target) override;
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
    void            CopyCPUToGPU(const void* data, size_t sz, VertexBuffer* vbo, size_t off = 0) override;
    void            CopyCPUToGPU(const void* data, size_t sz, IndexBuffer* ibo) override;
    void            CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cb) override;

    void BindVertexBuffer(VertexBuffer* vbo) override;
    void BindIndexBuffer(IndexBuffer* ibo) override;
    void BindConstantBuffer(int slot, ConstantBuffer* cbo) override;
    void BindTexture(Texture* tex, int slot = 0) override;

    void SetModelConstants(const Mat44& model = Mat44(), const Rgba8& tint = Rgba8::WHITE) override;
    void SetDirectionalLightConstants(const DirectionalLightConstants&) override;
    void SetLightConstants(Vec3 lightPos, float ambient, const Mat44& view, const Mat44& proj) override;
    void SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, size_t sz, int slot) override;

    void SetBlendMode(BlendMode) override;
    void SetRasterizerMode(RasterizerMode) override;
    void SetDepthMode(DepthMode) override;
    void SetSamplerMode(SamplerMode) override;

    void DrawVertexArray(int n, const Vertex_PCU* v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx) override;

    void DrawVertexBuffer(VertexBuffer* vbo, int count, int offset = 0) override;
    void DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, int idxCount = 0, int idxOffset = 0) override;

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
    std::array<VertexBuffer*, kBackBufferCount> m_frameVertexBuffer;

    /// Index Buffers
    IndexBuffer* m_indexBuffer = nullptr;

    // Constant Buffers
    std::vector<ConstantBuffer*> m_constantBuffers;

    /// Frame heap and the shader source view manager heap
    /// m_frameHeap (Shader-Visible):
    /// [CBV0][CBV1]...[CBV13] | [Set0: t0-t127] | [Set1: t0-t127] | [Set2: t0-t127] ...
    /// |<----- 14 slots ----->|<-- 128 slots -->|<-- 128 slots -->|
    ///                        ^                 ^                 ^
    ///                        |                 |                 |
    ///                   baseIndex=14      baseIndex=142      baseIndex=270
    /// TODO: Consider use Double-buffered swap chain to make CPU synchronize with GPU
    ComPtr<ID3D12DescriptorHeap> m_frameHeap                   = nullptr; // The heap that storage 14 constant buffer view descriptors
    ComPtr<ID3D12DescriptorHeap> m_shaderSourceViewManagerHeap = nullptr; // The shader resource view heap that contains kMaxTextureCached size of views

    struct RenderState
    {
        BlendMode      blendMode      = BlendMode::ALPHA;
        RasterizerMode rasterizerMode = RasterizerMode::SOLID_CULL_NONE;
        DepthMode      depthMode      = DepthMode::READ_WRITE_LESS_EQUAL;
        SamplerMode    samplerMode    = SamplerMode::POINT_CLAMP;
        Shader*        shader         = nullptr; // Bind Shader
        // In order to be used in a map, a comparison operator is required
        bool operator<(const RenderState& other) const
        {
            if (blendMode != other.blendMode) return blendMode < other.blendMode;
            if (rasterizerMode != other.rasterizerMode) return rasterizerMode < other.rasterizerMode;
            if (depthMode != other.depthMode) return depthMode < other.depthMode;
            return samplerMode < other.samplerMode;
        }


        bool operator==(const RenderState& other) const
        {
            return blendMode == other.blendMode &&
                rasterizerMode == other.rasterizerMode &&
                depthMode == other.depthMode &&
                samplerMode == other.samplerMode;
        }
    };

    /// DescriptorSet for each DrawCall
    /// - Each draw call uses a separate descriptor set
    /// - BindTexture modifies the texture of the specified slot in the current descriptor set
    /// - DrawVertexArray commits the current descriptor set before drawing, and then prepares for the next
    /// - This ensures that each draw call has its own independent texture binding state
    struct DescriptorSet
    {
        UINT        baseIndex; // Starting index in the heap
        Texture*    boundTextures[kMaxShaderSourceViewSlot]; //Record the bound texture pointer
        RenderState renderState; // The rendering state used by this descriptor set
    };

    std::vector<DescriptorSet> m_descriptorSets;
    UINT                       m_currentDescriptorSet     = 0;
    static constexpr UINT      kMaxDescriptorSetsPerFrame = 128;


    /// depth Buffer
    ComPtr<ID3D12Resource>       m_depthStencilBuffer             = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilViewDescriptorHeap = nullptr;
    /// Root signature that expose shader needed data and buffers to GPU
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    /// define viewport
    D3D12_VIEWPORT m_viewport{};
    /// define scissor rect
    CD3DX12_RECT m_scissorRect{};


    Texture*                 m_defaultTexture = nullptr;
    Shader*                  m_defaultShader  = nullptr;
    Shader*                  m_currentShader  = nullptr;
    std::vector<Texture*>    m_textureCache; // key: 文件路径
    std::vector<Shader*>     m_shaderCache; // key: Shader 名称
    std::vector<BitmapFont*> m_fontsCache;

private:
    void UploadToCB(ConstantBuffer* cb, const void* data, size_t size);
    void WaitForGPU();

    /// Descriptor Set
    void CommitCurrentDescriptorSet();
    void PrepareNextDescriptorSet();

private:
    
    // PSO Base Templates - These are the fixed parts shared by all PSOs
    // TODO: Consider use hash value to directly access the memory
    struct PSOTemplate
    {
        D3D12_INPUT_ELEMENT_DESC      inputLayout[3]; // PCU layout
        UINT                          inputLayoutCount;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;
        ComPtr<ID3DBlob>              defaultVertexShader;
        ComPtr<ID3DBlob>              defaultPixelShader;
        DXGI_FORMAT                   renderTargetFormat;
        DXGI_FORMAT                   depthStencilFormat;
        ComPtr<ID3D12RootSignature>   rootSignature;
    } m_psoTemplate;

    /// Pipeline state cache, Cache the created PSO based on the state combination
    std::map<RenderState, ComPtr<ID3D12PipelineState>> m_pipelineStateCache;

    // Current waiting application rendering status
    RenderState m_pendingRenderState;

    ComPtr<ID3D12PipelineState> m_currentPipelineStateObject = nullptr;

    [[maybe_unused]]
    // Get or create the PSO of the corresponding state
    ID3D12PipelineState* GetOrCreatePipelineState(const RenderState& state);

    // Auxiliary method for creating PSO based on status
    [[nodiscard]]
    ComPtr<ID3D12PipelineState> CreatePipelineStateForRenderState(const RenderState& state);
};
