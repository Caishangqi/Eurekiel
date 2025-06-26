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
    RenderConfig                      m_config{};
    ComPtr<ID3D12Device>              m_device                         = nullptr;
    ComPtr<ID3D12Device2>             m_device2                        = nullptr;
    ComPtr<ID3D12CommandQueue>        m_commandQueue                   = nullptr;
    ComPtr<IDXGISwapChain3>           m_swapChain                      = nullptr;
    ComPtr<ID3D12DescriptorHeap>      m_renderTargetViewHeap           = nullptr;
    UINT                              m_renderTargetViewDescriptorSize = 0;
    ComPtr<ID3D12Resource>            m_backBuffers[kBackBufferCount];
    UINT                              m_currentBackBufferIndex = 0;
    ComPtr<ID3D12CommandAllocator>    m_commandAllocator       = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_commandList            = nullptr;
    ComPtr<ID3D12Fence>               m_fence                  = nullptr;
    UINT64                            m_fenceValue             = 0;
    HANDLE                            m_fenceEvent             = nullptr;

    /// Vertex Buffers
    VertexBuffer* m_currentVertexBuffer = nullptr;
    // Updated ring vertex buffer
    std::array<VertexBuffer*, kBackBufferCount> m_frameVertexBuffer;

    /// Index Buffers
    IndexBuffer* m_indexBuffer = nullptr;

    // Constant Buffers
    std::vector<ConstantBuffer*> m_constantBuffers;
    // Frame heap and the shader source view manager heap that 
    // TODO: Consider use Double-buffered swap chain to make CPU synchronize with GPU
    ComPtr<ID3D12DescriptorHeap> m_frameHeap                   = nullptr; // The heap that storage 14 constant buffer view descriptors
    ComPtr<ID3D12DescriptorHeap> m_shaderSourceViewManagerHeap = nullptr; // The shader resource view heap that contains kMaxTextureCached size of views

    /// depth Buffer
    ComPtr<ID3D12Resource>       m_depthStencilBuffer             = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilViewDescriptorHeap = nullptr;
    /// Root signature that expose shader needed data and buffers to GPU
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    /// Pipeline state object
    ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
    /// define viewport
    D3D12_VIEWPORT m_viewport{};
    /// define scissor rect
    CD3DX12_RECT m_scissorRect{};

    BlendMode      m_blend   = BlendMode::ALPHA;
    RasterizerMode m_raster  = RasterizerMode::SOLID_CULL_BACK;
    DepthMode      m_depth   = DepthMode::READ_WRITE_LESS_EQUAL;
    SamplerMode    m_sampler = SamplerMode::POINT_CLAMP;

    Texture*                 m_defaultTexture = nullptr;
    Shader*                  m_defaultShader  = nullptr;
    std::vector<Texture*>    m_textureCache; // key: 文件路径
    std::vector<Shader*>     m_shaderCache; // key: Shader 名称
    std::vector<BitmapFont*> m_fontsCache;

    void UploadToCB(ConstantBuffer* cb, const void* data, size_t size);
    void WaitForGPU();
};
