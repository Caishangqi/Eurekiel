﻿//=========================================================
// File: Engine/Renderer/DX12/DX12Renderer.hpp
// 描述: DirectX-12 后端 - 完整实现 IRenderer 接口
//=========================================================
#pragma once
#include <d3d12.h>

#include "Engine/Renderer/IRenderer.hpp"
#include <unordered_map>
#include <vector>
#include <ThirdParty/d3dx12/d3dx12.h>
#include <wrl/client.h>

#include "Engine/Renderer/IndexBuffer.hpp"

//---------------------------------------------------------
// 一些前向声明，避免头文件互相包含
//---------------------------------------------------------
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
static constexpr uint32_t kBufferCount = 2;

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

    //—— 资源创建 ——//
    Shader* CreateShader(const char* name, const char* src, VertexType t = VertexType::Vertex_PCU) override;
    Shader* CreateShader(const char* name, VertexType t = VertexType::Vertex_PCU) override;
    Shader* CreateOrGetShader(const char* shaderName) override;
    bool    CompileShaderToByteCode(std::vector<uint8_t>& outByteCode,
                                 const char*              name,
                                 const char*              source,
                                 const char*              entryPoint,
                                 const char*              target) override;
    Texture*    CreateTextureFromImage(Image& image) override;
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

    void BindVertexBuffer(VertexBuffer* vbo) override;
    void BindIndexBuffer(IndexBuffer* ibo) override;
    void BindConstantBuffer(int slot, ConstantBuffer* cbo) override;
    void BindTexture(Texture* tex, int slot) override;

    //—— 常量缓冲 & 状态 ——//
    void SetModelConstants(const Mat44& model = Mat44(), const Rgba8& tint = Rgba8::WHITE) override;
    void SetDirectionalLightConstants(const DirectionalLightConstants&) override;
    void SetLightConstants(Vec3         lightPos, float    ambient,
                           const Mat44& view, const Mat44& proj) override;
    void SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, size_t sz, int slot) override;

    void SetBlendMode(BlendMode) override;
    void SetRasterizerMode(RasterizerMode) override;
    void SetDepthMode(DepthMode) override;
    void SetSamplerMode(SamplerMode) override;

    //—— Draw 家族 ——//
    void DrawVertexArray(int n, const Vertex_PCU* v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) override;
    void DrawVertexArray(const std::vector<Vertex_PCU>& v,
                         const std::vector<unsigned>&   idx) override;
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v,
                         const std::vector<unsigned>&      idx) override;

    void DrawVertexBuffer(VertexBuffer* vbo, int count, int offset = 0) override;
    void DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo,
                           int           idxCount, int     idxOffset = 0) override;

    //—— 可选高级接口 ——//
    void RenderEmissive() override
    {
    }

    Texture* GetCurScreenAsTexture() override { return nullptr; }

private:
    //--------------------------------------------------------
    // 设备级资源（StartUp / Shutdown 生命周期）
    //--------------------------------------------------------
    RenderConfig                          m_config{};
    ComPtr<ID3D12Device>                  m_device                         = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device2> m_device2                        = nullptr;
    ComPtr<ID3D12CommandQueue>            m_commandQueue                   = nullptr;
    ComPtr<IDXGISwapChain3>               m_swapChain                      = nullptr;
    ComPtr<ID3D12DescriptorHeap>          m_renderTargetViewHeap           = nullptr;
    UINT                                  m_renderTargetViewDescriptorSize = 0;
    ComPtr<ID3D12Resource>                m_backBuffers[kBufferCount];
    UINT                                  m_currentBackBufferIndex = 0;
    ComPtr<ID3D12CommandAllocator>        m_commandAllocator       = nullptr;
    ComPtr<ID3D12GraphicsCommandList>     m_commandList            = nullptr;
    ComPtr<ID3D12Fence>                   m_fence                  = nullptr;
    UINT64                                m_fenceValue             = 0;
    HANDLE                                m_fenceEvent             = nullptr;

    /// Buffers Version 1
    VertexBuffer* m_vertexBuffer = nullptr;
    /// Root signature that expose shader needed data and buffers to GPU
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    /// Pipeline state object
    ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
    /// define viewport
    D3D12_VIEWPORT m_viewport{};
    /// define scissor rect
    CD3DX12_RECT m_scissorRect{};

    // 当前帧状态 ---------------------------------------------
    uint32_t        m_drawCount = 0; // 已提交 draw 数
    CameraConstants m_curCamCB{};
    ModelConstants  m_curModelCB{};


    UINT                  m_rtvStride       = 0;
    ID3D12Resource*       m_depthStencilBuf = nullptr;
    ID3D12DescriptorHeap* m_dsvHeap         = nullptr;

    // 每帧临时 VB/IB，EndFrame 清理
    std::vector<VertexBuffer*> m_tmpVB;
    std::vector<IndexBuffer*>  m_tmpIB;


    //--------------------------------------------------------
    // PSO 缓存 —— 单变量对比，无 Current/Desired 结构
    //--------------------------------------------------------
    std::unordered_map<uint64_t, ID3D12PipelineState*> m_psoCache;
    uint64_t                                           m_curPSOKey  = ~0ull; // 上一次 draw 用的 key
    ID3D12PipelineState*                               m_curPSO     = nullptr;
    ID3D12PipelineState*                               m_defaultPSO = nullptr;

    BlendMode      m_blend   = BlendMode::ALPHA;
    RasterizerMode m_raster  = RasterizerMode::SOLID_CULL_BACK;
    DepthMode      m_depth   = DepthMode::READ_WRITE_LESS_EQUAL;
    SamplerMode    m_sampler = SamplerMode::POINT_CLAMP;

    //--------------------------------------------------------
    // 纹理 & Shader 缓存 —— 替代 Renderer.cpp 里的 m_loadedTextures / m_loadedShaders
    //--------------------------------------------------------
    Texture*                 m_defaultTexture = nullptr;
    Shader*                  m_defaultShader  = nullptr;
    std::vector<Texture*>    m_textureCache; // key: 文件路径
    std::vector<Shader*>     m_shaderCache; // key: Shader 名称
    std::vector<BitmapFont*> m_fontsCache;

    //--------------------------------------------------------
    // 常量缓冲（预分配，避免运行时创建）
    //--------------------------------------------------------
    ID3D12DescriptorHeap* m_cbvHeap  = nullptr; // ▲ 新增
    static constexpr int  kCBPerDraw = 4; // model / camera / dirLight / extra
    static constexpr int  kMaxDraws  = 1024;
    ConstantBuffer*       m_cbs[kCBPerDraw * kMaxDraws]{};
    int                   m_drawIndex = 0;

    //--------------------------------------------------------
    // 内部帮助函数
    //--------------------------------------------------------
    uint64_t CalcPSOHash() const; // 根据 Shader & 枚举状态拼 64‑bit key
    void     ApplyStatesIfNeeded();
    void     UploadToCB(ConstantBuffer* cb, const void* data, size_t size);
    void     WaitForGPU();
};
