#pragma once
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

#include <vector>

#include "IndexBuffer.hpp"
#include "IRenderer.hpp"
#include "Shader.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Game/EngineBuildPreferences.hpp"
///
#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = nullptr; } }
struct LightingConstants;
class IndexBuffer;
class Image;
class ConstantBuffer;
class VertexBuffer;
///
class BitmapFont;
class Texture;
class Window;
class Camera;
struct Vertex_PCU;
struct IntVec2;
struct Rgba8;

// Windows or DirectX headers as they also define it.
// have to #undefine OPAQUE
#if defined(OPAQUE)
#undef OPAQUE
#endif


class Renderer
{
public:
    Renderer(const RenderConfig& render_config);

    void Startup();
    void BeginFrame();
    void EndFrame();
    void Shutdown();

    void ClearScreen(const Rgba8& clearColor);
    void BeginCamera(const Camera& camera);
    void EndCamera(const Camera& camera);
    void DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes);
    void DrawVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes);
    void DrawVertexArray(const std::vector<Vertex_PCU>& vertexes);
    void DrawVertexArray(const std::vector<Vertex_PCUTBN>& vertexes);
    void DrawIndexedVertexArray(int numVertexes, const Vertex_PCU* vertexes, const unsigned int* indices, unsigned int numIndices);
    void DrawIndexedVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes, const unsigned int* indices, unsigned int numIndices);
    void DrawIndexedVertexArray(const std::vector<Vertex_PCU>& vertexes, const std::vector<unsigned int>& indexes);
    void DrawIndexedVertexArray(const std::vector<Vertex_PCUTBN>& vertexes, const std::vector<unsigned int>& indexes);

    BitmapFont* CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension);
    Texture*    CreateOrGetTextureFromFile(const char* filename);
    void        BindTexture(Texture* texture, int slot = 0);

    /// DirectX Shader

    Shader* CreateShader(const char* shaderName, const char* shaderSource, VertexType vertexType = VertexType::Vertex_PCU);
    Shader* GetShader(const char* shaderName);
    Shader* CreateShader(const char* shaderName, VertexType vertexType = VertexType::Vertex_PCU);
    Shader* CreateShaderFromFile(const char* sourcePath, VertexType vertexType = VertexType::Vertex_PCU);
    bool    CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, const char* name, const char* source, const char* entryPoint, const char* target);
    void    BindShader(Shader* shader);

    VertexBuffer*   CreateVertexBuffer(unsigned int size, unsigned int stride);
    IndexBuffer*    CreateIndexBuffer(unsigned int size);
    ConstantBuffer* CreateConstantBuffer(unsigned int size);
    void            CopyCPUToGPU(const void* data, unsigned int size, VertexBuffer* vbo);
    void            CopyCPUToGPU(const void* data, unsigned int size, ConstantBuffer* cbo);
    void            CopyCPUToGPU(const void* data, unsigned int size, IndexBuffer* ibo);
    void            BindVertexBuffer(VertexBuffer* vbo);
    void            BindIndexBuffer(IndexBuffer* ibo);
    void            BindConstantBuffer(int slot, ConstantBuffer* cbo);

    void SetStatesIfChanged(); // Check whether the blend mode is changed, if so, update directX OMSetBlendState()
    void SetBlendMode(BlendMode blendMode);
    void SetRasterizerMode(RasterizerMode rasterizerMode);
    void SetDepthMode(DepthMode depthMode);
    void SetSamplerMode(SamplerMode samplerMode);

    void SetModelConstants(const Mat44& modelToWorldTransform = Mat44(), const Rgba8& modelColor = Rgba8::WHITE);
    void SetCustomConstants(void* data, int registerSlot, ConstantBuffer* constantBuffer);
    void SetFrameConstants(const FrameConstants& frameConstants);
    void SetLightConstants(const LightingConstants& lightConstants);
    void SetLightConstants(Vec3 sunDirection, float sunIntensity, float ambientIntensity);

    void DrawVertexBuffer(VertexBuffer* vbo, unsigned int vertexCount);
    void DrawIndexedVertexBuffer(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount);

protected:
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

    /// 
private:
    /// Personally, I want to put all resource through the list not the map because I want
    /// resource Manager to handle the resource registration <key, value> pairs

    // Private (internal) data members will go here
    //void*                    m_apiRenderingContext = nullptr; // ...becomes void* Renderer::m_apiRenderingContext
    std::vector<Texture*>    m_loadedTextures;
    std::vector<BitmapFont*> m_loadedFonts;
    RenderConfig             m_config;

    Texture* m_defaultTexture = nullptr;

    //void        CreateRenderingContext(); OpenGL

    // Texture
public:
    Image*      CreateImageFromFile(const char* imageFilePath);
    Texture*    CreateTextureFromImage(Image& image);
    Texture*    CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData);
    Texture*    CreateTextureFromFile(const char* imageFilePath);
    Texture*    GetTextureForFileName(const char* imageFilePath);
    BitmapFont* CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture);

private:
    Renderer() = default;
};
