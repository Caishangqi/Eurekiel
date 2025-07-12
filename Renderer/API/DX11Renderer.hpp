#pragma once
#include "Engine/Renderer/IRenderer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#define  ENGINE_DEBUG_RENDER
#endif

#if defined(ENGINE_DEBUG_RENDER)
#include <dxgidebug.h>
#pragma comment (lib, "dxguid.lib")
#endif

// Windows or DirectX headers as they also define it.
// have to #undefine OPAQUE
#if defined(OPAQUE)
#undef OPAQUE
#endif


class DX11Renderer : public IRenderer
{
public:
    DX11Renderer(RenderConfig& config);

    void            Startup() override;
    void            Shutdown() override;
    
    void            BeginFrame() override;
    void            EndFrame() override;
    void            ClearScreen(const Rgba8& clear) override;
    void            BeginCamera(const Camera& cam) override;
    void            EndCamera(const Camera& camera) override;
    void            SetModelConstants(const Mat44& modelToWorldTransform = Mat44(), const Rgba8& modelColor = Rgba8::WHITE) override;
    void            SetDirectionalLightConstants(const DirectionalLightConstants& dl) override;
    void            SetFrameConstants(const FrameConstants& frameConstants) override;
    void            SetLightConstants(const LightingConstants& lightConstants) override;
    void            SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, int slot) override;
    void            SetBlendMode(BlendMode mode) override;
    void            SetRasterizerMode(RasterizerMode mode) override;
    void            SetDepthMode(DepthMode mode) override;
    void            SetSamplerMode(SamplerMode mode, int slot = 0) override;
    Shader*         CreateShader(const char* shaderName, const char* src, VertexType t) override;
    Shader*         CreateShader(const char* name, VertexType vertexType) override;
    Shader*         CreateOrGetShader(const char* shaderName, VertexType vertexType = VertexType::Vertex_PCU) override;
    BitmapFont*     CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension) override;
    bool            CompileShaderToByteCode(std::vector<uint8_t>& outBytes, const char* name, const char* src, const char* entry, const char* target) override;
    void            BindShader(Shader* shader) override;
    Texture*        CreateOrGetTexture(const char* imageFilePath) override;
    Image*          CreateImageFromFile(const char* imageFilePath) override;
    Texture*        CreateTextureFromImage(Image& image) override;
    Texture*        CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData) override;
    Texture*        CreateTextureFromFile(const char* imageFilePath) override;
    Texture*        GetTextureForFileName(const char* imageFilePath) override;
    BitmapFont*     CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture) override;
    VertexBuffer*   CreateVertexBuffer(size_t size, unsigned stride) override;
    IndexBuffer*    CreateIndexBuffer(size_t size) override;
    ConstantBuffer* CreateConstantBuffer(size_t size) override;
    void            CopyCPUToGPU(const void* data, size_t size, VertexBuffer* v, size_t offset = 0) override;
    void            CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset = 0) override;
    void            CopyCPUToGPU(const Vertex_PCUTBN* data, size_t size, VertexBuffer* v, size_t offset = 0) override;
    void            CopyCPUToGPU(const void* data, size_t size, IndexBuffer* ibo) override;
    void            CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cb) override;
    void            BindVertexBuffer(VertexBuffer* v) override;
    void            BindIndexBuffer(IndexBuffer* i) override;
    void            BindConstantBuffer(int slot, ConstantBuffer* c) override;
    void            BindTexture(Texture* texture, int slot = 0) override;
    void            DrawVertexArray(int num, const Vertex_PCU* v) override;
    void            DrawVertexArray(int num, const Vertex_PCUTBN* v) override;
    void            DrawVertexArray(const std::vector<Vertex_PCU>& v) override;
    void            DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) override;
    void            DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx) override;
    void            DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx) override;
    void            DrawVertexBuffer(VertexBuffer* v, int count) override;
    void            DrawVertexIndexed(VertexBuffer* v, IndexBuffer* i, unsigned indexCount) override;

private:
    /// DirectX
    ID3D11RenderTargetView* m_renderTargetView = nullptr;
    ID3D11Device*           m_device           = nullptr;
    ID3D11DeviceContext*    m_deviceContext    = nullptr;
    IDXGISwapChain*         m_swapChain        = nullptr;
    // Shaders
    std::vector<Shader*> m_loadedShaders;
    Shader*              m_currentShader = nullptr;
    Shader*              m_defaultShader = nullptr;
    // Buffers
    VertexBuffer*   m_immediateVBO     = nullptr;
    VertexBuffer*   m_immediateVBO_TBN = nullptr;
    IndexBuffer*    m_immediateIBO     = nullptr;
    ConstantBuffer* m_cameraCBO        = nullptr;
    ConstantBuffer* m_modelCBO         = nullptr;
    ConstantBuffer* m_lightCBO         = nullptr;
    ConstantBuffer* m_perFrameCBO      = nullptr;
    // Blend
    ID3D11BlendState* m_blendState                                      = nullptr;
    BlendMode         m_desiredBlendMode                                = BlendMode::ALPHA;
    ID3D11BlendState* m_blendStates[static_cast<int>(BlendMode::COUNT)] = {};
    // Sampler
    ID3D11SamplerState* m_samplerState                                        = nullptr;
    SamplerMode         m_desiredSamplerMode                                  = SamplerMode::POINT_CLAMP;
    ID3D11SamplerState* m_samplerStates[static_cast<int>(SamplerMode::COUNT)] = {};
    // Rasterization
    ID3D11RasterizerState* m_rasterizerState                                           = nullptr;
    RasterizerMode         m_desiredRasterizerMode                                     = RasterizerMode::SOLID_CULL_BACK;
    ID3D11RasterizerState* m_rasterizerStates[static_cast<int>(RasterizerMode::COUNT)] = {};
    // Depth
    ID3D11Texture2D*         m_depthStencilTexture                                    = nullptr;
    ID3D11DepthStencilView*  m_depthStencilDSV                                        = nullptr;
    DepthMode                m_desiredDepthMode                                       = DepthMode::READ_WRITE_LESS_EQUAL;
    ID3D11DepthStencilState* m_depthStencilStates[static_cast<int>(DepthMode::COUNT)] = {};
    ID3D11DepthStencilState* m_depthStencilState                                      = nullptr;

#if defined(ENGINE_DEBUG_RENDER)
    void* m_dxgiDebug       = nullptr;
    void* m_dxgiDebugModule = nullptr;
#endif

private:
    std::vector<Texture*>    m_loadedTextures;
    std::vector<BitmapFont*> m_loadedFonts;
    RenderConfig             m_config;

    Texture* m_defaultTexture = nullptr;

    static constexpr int k_cameraConstantsSlot   = 2;
    static constexpr int k_perFrameConstantsSlot = 1;
    static constexpr int k_modelConstantsSlot    = 3;
    static constexpr int k_lightConstantsSlot    = 4;

private:
    DX11Renderer() = default;

private:
    void SetStatesIfChanged();

    void DrawIndexedVertexArray(int numVertexes, const Vertex_PCU* vertexes, const unsigned int* indices, unsigned int numIndices);
    void DrawIndexedVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes, const unsigned int* indices, unsigned int numIndices);
};
