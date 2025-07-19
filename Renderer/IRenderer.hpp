// IRenderer.hpp
/*--------------------------------------------------------------
 *  Cross-API abstraction layer for the rendering backend.
 *  Any engine system should talk ONLY to this interface.
 *-------------------------------------------------------------*/
#pragma once
#include <dxgiformat.h>
#include <vector>

#include "RenderTarget.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Light/Light.hpp"
#include "Engine/Core/VertexUtils.hpp"

class Image;
class Window;
// Forward declarations
class BitmapFont;
class ConstantBuffer;
class IndexBuffer;
class Shader;
class Texture;
class VertexBuffer;
class Camera;
struct DirectionalLightConstants;
struct Vertex_PCUTBN;
struct BlurConstants;
struct LightConstants;

#if defined(OPAQUE)
#undef OPAQUE
#endif

#define DX_SAFE_RELEASE(p) { if(p) { (p)->Release(); (p) = nullptr; } }

enum class RendererBackend
{
    DirectX11,
    DirectX12,
    OpenGL
};


// Basic enums
struct RenderConfig
{
    Window*         m_window        = nullptr;
    std::string     m_defaultShader = "Default"; // This is useful for debugging
    RendererBackend m_backend       = RendererBackend::DirectX11;
};

enum class BlendMode
{
    ADDITIVE,
    ALPHA,
    OPAQUE,
    COUNT = 3
};

enum class SamplerMode
{
    POINT_CLAMP,
    BILINEAR_WRAP,
    COUNT = 2
};

enum class RasterizerMode
{
    SOLID_CULL_NONE,
    SOLID_CULL_BACK,
    WIREFRAME_CULL_NONE,
    WIREFRAME_CULL_BACK,
    COUNT = 4
};

enum class DepthMode
{
    DISABLED,
    READ_ONLY_ALWAYS,
    READ_ONLY_LESS_EQUAL,
    READ_WRITE_LESS_EQUAL,
    COUNT
};

enum class VertexType
{
    Vertex_PCU,
    Vertex_PCUTBN
};

struct EngineConstants
{
    Mat44 EngineConstant[4];
};

struct FrameConstants
{
    float Time;
    int   DebugInt;
    float DebugFloat;
    int   DebugViewMode;
    float Padding[12];
    Mat44 FrameConstant[3];
};

struct CameraConstants
{
    Mat44 WorldToCameraTransform; // View transform
    Mat44 CameraToRenderTransform; // Non-standard transform from game to DirectX conventions
    Mat44 RenderToClipTransform; // Project transform
    Mat44 CameraToWorldTransform; // Camera position, use for calculate Spectral
    /*float OrthoMixX;
    float OrthoMixY;
    float OrthoMixZ;
    float OrthoMaxX;
    float OrthoMaxY;
    float OrthoMaxZ;
    float pad0;
    float pad1;*/
};

///
///DirectX requires that the size of each constant buffer must be a multiple of 16 bytes.
///The size of the structure LightingConstants you pass in when creating the light constant
///buffer may be 20 bytes (for example, if Vec3 occupies 12 bytes, plus two floats are 4
///bytes each, a total of 20 bytes), which is not a multiple of 16 bytes, causing the CreateBuffer call to fail.
struct LightingConstants
{
    Vec3  SunDirection;
    float SunIntensity;
    float AmbientIntensity;
    int   NumLights;
    float pad0;
    float pad1;
    Light lights[8];
    float pad2[36]; // x 1.6875
};

struct ModelConstants
{
    Mat44 ModelToWorldTransform;
    float ModelColor[4];
    float padding[44];
};


struct ConversionBuffer
{
    std::vector<Vertex_PCUTBN> buffer;
    size_t                     cursor = 0;

    void Reset() { cursor = 0; }

    Vertex_PCUTBN* Allocate(size_t count);
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    // Life-cycle & per-frame
    virtual void Startup() = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Global clear / camera
    virtual void ClearScreen(const Rgba8& clear) = 0;
    virtual void BeginCamera(const Camera& cam) = 0;
    virtual void EndCamera(const Camera& cam) = 0;

    // Constant-buffer helpers
    virtual void SetModelConstants(const Mat44& modelToWorldTransform = Mat44(), const Rgba8& tint = Rgba8::WHITE) = 0;
    virtual void SetDirectionalLightConstants(const DirectionalLightConstants& dl) = 0;
    virtual void SetLightConstants(const LightingConstants& lightConstants) = 0;
    virtual void SetFrameConstants(const FrameConstants& frameConstants) = 0;
    virtual void SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, int slot) = 0;

    // State setters
    virtual void SetBlendMode(BlendMode mode) = 0;
    virtual void SetRasterizerMode(RasterizerMode mode) = 0;
    virtual void SetDepthMode(DepthMode mode) = 0;
    virtual void SetSamplerMode(SamplerMode mode, int slot = 0) = 0;

    // Resource creation
    virtual Shader*     CreateShader(const char* name, const char* src, VertexType t = VertexType::Vertex_PCU) = 0;
    virtual Shader*     CreateShader(const char* name, VertexType t = VertexType::Vertex_PCU) = 0;
    virtual Shader*     CreateShader(const char* name, const char* shaderPath, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType = VertexType::Vertex_PCU) = 0;
    virtual Shader*     CreateShaderFromSource(const char* name, const char* shaderSource, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType = VertexType::Vertex_PCU) = 0;
    virtual Shader*     CreateOrGetShader(const char* shaderName, VertexType vertexType = VertexType::Vertex_PCU) = 0;
    virtual BitmapFont* CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension) = 0;
    virtual bool        CompileShaderToByteCode(std::vector<uint8_t>& outBytes, const char* name, const char* src, const char* entry, const char* target) = 0;
    virtual void        BindShader(Shader* s) = 0;
    virtual Texture*    CreateOrGetTexture(const char* imageFilePath) = 0; // TODO: abstract to IResource interface
    virtual Image*      CreateImageFromFile(const char* imageFilePath);
    virtual Texture*    CreateTextureFromImage(Image& image) = 0;
    virtual Texture*    CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData) = 0;
    virtual Texture*    CreateTextureFromFile(const char* imageFilePath) = 0;
    virtual Texture*    GetTextureForFileName(const char* imageFilePath) = 0;
    virtual BitmapFont* CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture) = 0;


    virtual VertexBuffer*   CreateVertexBuffer(size_t size, unsigned stride) = 0;
    virtual IndexBuffer*    CreateIndexBuffer(size_t size) = 0;
    virtual ConstantBuffer* CreateConstantBuffer(size_t size) = 0;

    /**
     * @brief Copies vertex data from CPU memory to GPU memory while converting it into the required format. This raw copy function
     * do not handle any Vertex Type check that user should not use the functions
     * @param data Pointer to the array of vertices in the Vertex_PCU format that need to be copied.
     * @param size The size of the input vertex data in bytes.
     * @param v Pointer to the vertex buffer object where the data will be copied to.
     * @param offset The byte offset into the GPU vertex buffer where the data will be written.
     * @see virtual void CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset = 0)
     */
    virtual void CopyCPUToGPU(const void* data, size_t size, VertexBuffer* v, size_t offset = 0) = 0;
    /**
     * @brief Copies vertex data from CPU memory to GPU memory while converting it into the required format. This method served
     * as an backward compatibility API between DirectX11 and DirectX12
     * @param data Pointer to the array of vertices in the Vertex_PCU format that need to be copied.
     * @param size The size of the input vertex data in bytes.
     * @param v Pointer to the vertex buffer object where the data will be copied to.
     * @param offset The byte offset into the GPU vertex buffer where the data will be written.
     */
    virtual void CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset = 0) = 0;
    /**
     * @brief Copies vertex data from CPU memory to GPU memory while converting it into the required format. This method served
     * as an backward compatibility API between DirectX11 and DirectX12
     * @param data Pointer to the array of vertices in the Vertex_PCU format that need to be copied.
     * @param size The size of the input vertex data in bytes.
     * @param v Pointer to the vertex buffer object where the data will be copied to.
     * @param offset The byte offset into the GPU vertex buffer where the data will be written.
     */
    virtual void CopyCPUToGPU(const Vertex_PCUTBN* data, size_t size, VertexBuffer* v, size_t offset = 0) = 0;
    virtual void CopyCPUToGPU(const void* data, size_t size, IndexBuffer* i) = 0;
    virtual void CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cb) = 0;

    // Binding helpers
    virtual void BindVertexBuffer(VertexBuffer* v) = 0;
    virtual void BindIndexBuffer(IndexBuffer* i) = 0;
    virtual void BindConstantBuffer(int slot, ConstantBuffer* c) = 0;
    virtual void BindTexture(Texture* tex, int slot = 0) = 0;

    // Draw family
    virtual void DrawVertexArray(int num, const Vertex_PCU* v) = 0;
    virtual void DrawVertexArray(int num, const Vertex_PCUTBN* v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCU>& v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx) = 0;

    virtual void DrawVertexBuffer(VertexBuffer* v, int count) = 0;
    virtual void DrawVertexIndexed(VertexBuffer* v, IndexBuffer* i, unsigned int indexCount) = 0;

    // Render Targets
    [[maybe_unused]] virtual RenderTarget* CreateRenderTarget(IntVec2 dimension, DXGI_FORMAT format) = 0;
    virtual void                           SetRenderTarget(RenderTarget* renderTarget) = 0;
    virtual void                           SetRenderTarget(RenderTarget** renderTargets, int count) = 0;
    virtual void                           ClearRenderTarget(RenderTarget* renderTarget, const Rgba8& clearColor) = 0;
    [[maybe_unused]] virtual RenderTarget* GetBackBufferRenderTarget() = 0;

    [[maybe_unused]] virtual void SetViewport(const IntVec2& dimension) = 0;

    static IRenderer* CreateRenderer(RenderConfig& config);

    /// Conversion

protected:
    // These are part of the interface contract, all implementations require
    BlendMode      m_currentBlendMode      = BlendMode::ALPHA;
    RasterizerMode m_currentRasterizerMode = RasterizerMode::SOLID_CULL_BACK;
    DepthMode      m_currentDepthMode      = DepthMode::READ_WRITE_LESS_EQUAL;
    SamplerMode    m_currentSamplerMode    = SamplerMode::POINT_CLAMP;

protected:
    /// Render Targets
    RenderTarget* m_currentRenderTarget = nullptr;
    RenderTarget* m_backBufferRenderTarget;
};

template <typename T>
static constexpr T AlignUp(T value, T alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}
