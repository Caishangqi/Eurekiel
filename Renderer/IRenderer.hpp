// IRenderer.hpp
/*--------------------------------------------------------------
 *  Cross-API abstraction layer for the rendering backend.
 *  Any engine system should talk ONLY to this interface.
 *-------------------------------------------------------------*/
#pragma once
#include <vector>
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Mat44.hpp"

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
    Mat44 EngineConstant_0;
    Mat44 EngineConstant_1;
    Mat44 EngineConstant_2;
    Mat44 EngineConstant_3;
};

struct FrameConstants
{
    float Time;
    int   DebugInt;
    float DebugFloat;
    float Padding[13];
    Mat44 FrameConstant_1;
    Mat44 FrameConstant_2;
    Mat44 FrameConstant_3;
};

struct CameraConstants
{
    Mat44 WorldToCameraTransform; // View transform
    Mat44 CameraToRenderTransform; // Non-standard transform from game to DirectX conventions
    Mat44 RenderToClipTransform; // Project transform
    float padding[16];
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
    float pad0;
    float pad1;
    float pad2;
};

struct ModelConstants
{
    Mat44 ModelToWorldTransform;
    float ModelColor[4];
    float padding[44];
};

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    //------------------------------------------------------------
    // Life-cycle & per-frame
    //------------------------------------------------------------
    virtual void Startup() = 0;
    virtual void Shutdown() = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    //------------------------------------------------------------
    // Global clear / camera
    //------------------------------------------------------------
    virtual void ClearScreen(const Rgba8& clear) = 0;
    virtual void BeginCamera(const Camera& cam) = 0;
    virtual void EndCamera(const Camera& cam) = 0;

    //------------------------------------------------------------
    // Constant-buffer helpers
    //------------------------------------------------------------
    virtual void SetModelConstants(const Mat44& model = Mat44(),
                                   const Rgba8& tint  = Rgba8::WHITE) = 0;
    virtual void SetDirectionalLightConstants(
        const DirectionalLightConstants& dl) = 0;
    virtual void SetLightConstants(Vec3         lightPos, float ambient,
                                   const Mat44& lightView,
                                   const Mat44& lightProj) = 0;
    virtual void SetCustomConstantBuffer(ConstantBuffer*& cbo,
                                         void*            data, size_t size,
                                         int              slot) = 0;

    //------------------------------------------------------------
    // State setters
    //------------------------------------------------------------
    virtual void SetBlendMode(BlendMode mode) = 0;
    virtual void SetRasterizerMode(RasterizerMode mode) = 0;
    virtual void SetDepthMode(DepthMode mode) = 0;
    virtual void SetSamplerMode(SamplerMode mode) = 0;

    //------------------------------------------------------------
    // Resource creation
    //------------------------------------------------------------
    virtual Shader* CreateShader(char const* name, char const* src, VertexType t = VertexType::Vertex_PCU) = 0;
    virtual Shader* CreateShader(char const* name, VertexType t = VertexType::Vertex_PCU) = 0;
    virtual Shader* CreateOrGetShader(const char* shaderName) = 0;
    virtual bool    CompileShaderToByteCode(std::vector<uint8_t>& outBytes,
                                         char const*              name,
                                         char const*              src,
                                         char const*              entry,
                                         char const*              target) = 0;
    virtual void BindShader(Shader* s) = 0;
    //virtual Texture* CreateOrGetTexture(const char)
    virtual Image*      CreateImageFromFile(const char* imageFilePath);
    virtual Texture*    CreateTextureFromImage(Image& image) = 0;
    virtual Texture*    CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData) = 0;
    virtual Texture*    CreateTextureFromFile(const char* imageFilePath) = 0;
    virtual Texture*    GetTextureForFileName(const char* imageFilePath) = 0;
    virtual BitmapFont* CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture) = 0;


    virtual VertexBuffer*   CreateVertexBuffer(size_t size, unsigned stride) = 0;
    virtual IndexBuffer*    CreateIndexBuffer(size_t size) = 0;
    virtual ConstantBuffer* CreateConstantBuffer(size_t size) = 0;

    virtual void CopyCPUToGPU(const void* data, size_t size, VertexBuffer* v, size_t offset = 0) = 0;
    virtual void CopyCPUToGPU(const void* data, size_t size, IndexBuffer* i) = 0;

    //------------------------------------------------------------
    // Binding helpers
    //------------------------------------------------------------
    virtual void BindVertexBuffer(VertexBuffer* v) = 0;
    virtual void BindIndexBuffer(IndexBuffer* i) = 0;
    virtual void BindConstantBuffer(int slot, ConstantBuffer* c) = 0;
    virtual void BindTexture(Texture* tex, int slot = 0) = 0;

    //------------------------------------------------------------
    // Draw family
    //------------------------------------------------------------
    virtual void DrawVertexArray(int num, const Vertex_PCU* v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCU>& v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCU>& v,
                                 const std::vector<unsigned>&   idx) = 0;
    virtual void DrawVertexArray(const std::vector<Vertex_PCUTBN>& v,
                                 const std::vector<unsigned>&      idx) = 0;

    virtual void DrawVertexBuffer(VertexBuffer* v, int count, int offset = 0) = 0;
    virtual void DrawVertexIndexed(VertexBuffer* v, IndexBuffer* i,
                                   int           idxCount, int   idxOffset = 0) = 0;

    static IRenderer* CreateRenderer(RenderConfig& config);

    //------------------------------------------------------------
    // Optional high-level effects（DX11 已实现，DX12 暂返回错误）
    //------------------------------------------------------------
    virtual void RenderEmissive()
    {
    }

    virtual Texture* GetCurScreenAsTexture() { return nullptr; }

    template <typename T>
    constexpr T AlignUp(T value, T alignment)
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }
};
