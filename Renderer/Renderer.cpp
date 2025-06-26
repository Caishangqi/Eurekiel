#include "ThirdParty/stb/stb_image.h"

#include "Engine/Renderer/IRenderer.hpp"
#include "Engine/Renderer/Renderer.hpp"

#include "VertexBuffer.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

#include <gl/gl.h>

#include "BitmapFont.hpp"
#include "ConstantBuffer.hpp"
#include "DefaultShader.hpp"
#include "Texture.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#pragma comment( lib, "opengl32" )	// Link in the OpenGL32.lib static library
extern HWND g_hWnd;

/// Constant


static constexpr int k_cameraConstantsSlot   = 2;
static constexpr int k_perFrameConstantsSlot = 1;
static constexpr int k_modelConstantsSlot    = 3;
static constexpr int k_lightConstantsSlot    = 4;

///

Renderer::Renderer(const RenderConfig& render_config)
{
    m_config = render_config;
}

//------------------------------------------------------------------------------------------------
// Given an existing OS Window, create a Rendering Context (RC) for OpenGL or DirectX to draw to it.
// #SD1ToDo: Move this to become Renderer::CreateRenderingContext() in Engine/Renderer/Renderer.cpp
// #SD1ToDo: By the end of SD1-A1, this function will be called from the function Renderer::Startup
//
void Renderer::Startup()
{
    //CreateRenderingContext(); // #SD1ToDo: this will move to Renderer.cpp, called by Renderer::Startup

    /// DirectX
    unsigned int deviceFlags = 0;
#if defined(ENGINE_DEBUG_RENDER)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Create debug module
#if defined(ENGINE_DEBUG_RENDER)
    m_dxgiDebugModule = static_cast<void*>(LoadLibraryA("dxgidebug.dll"));
    if (m_dxgiDebugModule == nullptr)
    {
        ERROR_AND_DIE("Could not load dxgidebug.dll.")
    }

    using GetDebugModuleCB = HRESULT(WINAPI *)(REFIID, void**);
    ((GetDebugModuleCB)GetProcAddress(static_cast<HMODULE>(m_dxgiDebugModule), "DXGIGetDebugInterface"))(__uuidof(IDXGIDebug), &m_dxgiDebug);
    if (m_dxgiDebug == nullptr)
    {
        ERROR_AND_DIE("Could not load debug module.")
    }
#endif

    // Create device and swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width     = m_config.m_window->GetClientDimensions().x;
    swapChainDesc.BufferDesc.Height    = m_config.m_window->GetClientDimensions().y;
    swapChainDesc.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count     = 1;
    swapChainDesc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount          = 2;
    swapChainDesc.OutputWindow         = static_cast<HWND>(m_config.m_window->GetWindowHandle());
    swapChainDesc.Windowed             = true;
    swapChainDesc.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // Call D3D11CreateDeviceAndSwapChain
    HRESULT hr;
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, &swapChainDesc, &m_swapChain, &m_device, nullptr, &m_deviceContext);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create D3D 11 device and swap chain")
    }

    // Get back buffer texture
    ID3D11Texture2D* backBuffer;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not get swap chain buffer.")
    }

    // Create render target view
    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create render target view for swap chain buffer")
    }

    DX_SAFE_RELEASE(backBuffer)

    m_immediateVBO     = CreateVertexBuffer(sizeof(Vertex_PCU), sizeof(Vertex_PCU));
    m_immediateVBO_TBN = CreateVertexBuffer(sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
    m_immediateIBO     = CreateIndexBuffer(sizeof(unsigned int));
    m_lightCBO         = CreateConstantBuffer(sizeof(LightingConstants));
    m_cameraCBO        = CreateConstantBuffer(sizeof(CameraConstants));
    m_modelCBO         = CreateConstantBuffer(sizeof(ModelConstants));
    m_perFrameCBO      = CreateConstantBuffer(sizeof(FrameConstants));

    // Set rasterizer state
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode              = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode              = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = false;
    //rasterizerDesc.DepthBias             = 0;
    //rasterizerDesc.DepthBiasClamp        = 0.f;
    //rasterizerDesc.SlopeScaledDepthBias  = 0.f;
    rasterizerDesc.DepthClipEnable = true;
    //rasterizerDesc.ScissorEnable         = false;
    //rasterizerDesc.MultisampleEnable     = false;
    rasterizerDesc.AntialiasedLineEnable = true;

    hr = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerStates[static_cast<int>(RasterizerMode::SOLID_CULL_NONE)]);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create rasterizer state -> RasterizerMode::SOLID_CULL_NONE")
    }
    rasterizerDesc.CullMode              = D3D11_CULL_BACK;
    rasterizerDesc.FrontCounterClockwise = true;
    hr                                   = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerStates[static_cast<int>(RasterizerMode::SOLID_CULL_BACK)]);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create rasterizer state -> RasterizerMode::SOLID_CULL_BACK")
    }

    rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    hr                      = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerStates[static_cast<int>(RasterizerMode::WIREFRAME_CULL_NONE)]);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create rasterizer state -> RasterizerMode::WIREFRANE_CULL_NONE")
    }

    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    hr                      = m_device->CreateRasterizerState(&rasterizerDesc, &m_rasterizerStates[static_cast<int>(RasterizerMode::WIREFRAME_CULL_BACK)]);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create rasterizer state -> RasterizerMode::WIREFRANE_CULL_BACK")
    }
    SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
    //m_deviceContext->RSSetState(m_rasterizerState);


    m_defaultShader = CreateShader("Default", rawShader);
    BindShader(m_defaultShader);

    // Create Blend State
    D3D11_BLEND_DESC blendDesc                      = {};
    blendDesc.RenderTarget[0].BlendEnable           = true;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = blendDesc.RenderTarget[0].SrcBlend;
    blendDesc.RenderTarget[0].DestBlendAlpha        = blendDesc.RenderTarget[0].DestBlend;
    blendDesc.RenderTarget[0].BlendOpAlpha          = blendDesc.RenderTarget[0].BlendOp;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr                                              = m_device->CreateBlendState(&blendDesc, &m_blendStates[static_cast<int>(BlendMode::OPAQUE)
                                    ]
    );
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateBlendState for BlendMode:OPAQUE failed.")
    // Alpha state
    blendDesc.RenderTarget[0].SrcBlend  = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    hr                                  = m_device->CreateBlendState(&blendDesc, &m_blendStates[static_cast<int>(BlendMode::ALPHA)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateBlendState for BlendMode:ALPHA failed.")

    // Additive state
    blendDesc.RenderTarget[0].SrcBlend  = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    hr                                  = m_device->CreateBlendState(&blendDesc, &m_blendStates[static_cast<int>(BlendMode::ADDITIVE)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateBlendState for BlendMode:ADDITIVE failed.")

    // Texture
    Image defaultImage(IntVec2(2, 2), Rgba8::WHITE); // Default 2x2 texture
    m_defaultTexture = CreateTextureFromImage(defaultImage);
    BindTexture(nullptr);

    // Sampler
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW           = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD             = D3D11_FLOAT32_MAX;
    hr                             = m_device->CreateSamplerState(&samplerDesc, &m_samplerStates[static_cast<int>(SamplerMode::POINT_CLAMP)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateSamplerState for SamplerMode::POINT_CLAMP failed.")

    samplerDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    hr                   = m_device->CreateSamplerState(&samplerDesc, &m_samplerStates[static_cast<int>(SamplerMode::BILINEAR_WRAP)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateSamplerState for SamplerMode::BILINEAR_WRAP failed.")

    m_samplerState = m_samplerStates[static_cast<int>(SamplerMode::POINT_CLAMP)]; // Default
    SetSamplerMode(SamplerMode::POINT_CLAMP);

    // Depth
    D3D11_TEXTURE2D_DESC depthTextureDesc = {};
    depthTextureDesc.Width                = m_config.m_window->GetClientDimensions().x;
    depthTextureDesc.Height               = m_config.m_window->GetClientDimensions().y;
    depthTextureDesc.MipLevels            = 1;
    depthTextureDesc.ArraySize            = 1;
    depthTextureDesc.Usage                = D3D11_USAGE_DEFAULT;
    depthTextureDesc.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthTextureDesc.BindFlags            = D3D11_BIND_DEPTH_STENCIL;
    depthTextureDesc.SampleDesc.Count     = 1;

    hr = m_device->CreateTexture2D(&depthTextureDesc, nullptr, &m_depthStencilTexture);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create texture for depth stencil.")
    }

    hr = m_device->CreateDepthStencilView(m_depthStencilTexture, nullptr, &m_depthStencilDSV);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create depth stencil view.")
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    hr                                        = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilStates[static_cast<int>(DepthMode::DISABLED)]);

    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("CreateDepthStencilState for DepthMode::DISABLED failed.")
    }
    depthStencilDesc.DepthEnable    = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    hr                              = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilStates[static_cast<int>(DepthMode::READ_ONLY_ALWAYS)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateDepthStencilState for DepthMode::READ_ONLY_ALWAYS failed.")

    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    hr                         = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilStates[static_cast<int>(DepthMode::READ_ONLY_LESS_EQUAL)]);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE("CreateDepthStencilState for DepthMode::READ_ONLY_LESS_EQUAL failed.")

    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    hr                              = m_device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilStates[static_cast<int>(DepthMode::READ_WRITE_LESS_EQUAL)]);


    /// 
}

void Renderer::BeginFrame()
{
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilDSV);
}

void Renderer::EndFrame()
{
    if (m_config.m_window)
    {
        // HDC displayContext = static_cast<HDC>(m_config.m_window->GetDisplayContext());
        // "Present" the backbuffer by swapping the front (visible) and back (working) screen buffers
        //SwapBuffers(displayContext); // Note: call this only once at the very end of each frame

        /// DirectX
        // Present
        HRESULT hr;
        hr = m_swapChain->Present(0, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            ERROR_AND_DIE("Device has been lost, application will now terminate")
        }
    }
}

void Renderer::Shutdown()
{
    /// Texture


    for (Texture* m_loaded_texture : m_loadedTextures)
    {
        delete m_loaded_texture;
        m_loaded_texture = nullptr;
    }
    //delete m_defaultTexture;
    //m_defaultTexture = nullptr;

    /// DirectX
    delete m_currentShader;
    m_currentShader = nullptr;

    POINTER_SAFE_DELETE(m_immediateIBO)
    POINTER_SAFE_DELETE(m_immediateVBO)
    POINTER_SAFE_DELETE(m_immediateVBO_TBN)
    POINTER_SAFE_DELETE(m_cameraCBO)
    POINTER_SAFE_DELETE(m_modelCBO)
    POINTER_SAFE_DELETE(m_lightCBO)
    POINTER_SAFE_DELETE(m_perFrameCBO)

    delete m_defaultShader;
    m_defaultShader = nullptr;

    for (Shader* m_loaded_shader : m_loadedShaders)
    {
        delete m_loaded_shader;
    }

    for (ID3D11SamplerState* m_sampler_state_element : m_samplerStates)
    {
        DX_SAFE_RELEASE(m_sampler_state_element)
    }

    for (ID3D11BlendState* m_blend_state_element : m_blendStates)
    {
        DX_SAFE_RELEASE(m_blend_state_element)
    }

    for (ID3D11DepthStencilState* m_depth_stencil_state : m_depthStencilStates)
    {
        DX_SAFE_RELEASE(m_depth_stencil_state)
    }

    for (ID3D11RasterizerState* m_rasterizer_state : m_rasterizerStates)
    {
        DX_SAFE_RELEASE(m_rasterizer_state)
    }

    DX_SAFE_RELEASE(m_depthStencilTexture)
    DX_SAFE_RELEASE(m_depthStencilDSV)
    DX_SAFE_RELEASE(m_renderTargetView)
    DX_SAFE_RELEASE(m_swapChain)
    DX_SAFE_RELEASE(m_deviceContext)
    DX_SAFE_RELEASE(m_device)

    // Report error leaks and release debug module
#if defined(ENGINE_DEBUG_RENDER)
    static_cast<IDXGIDebug*>(m_dxgiDebug)->ReportLiveObjects(DXGI_DEBUG_ALL, static_cast<DXGI_DEBUG_RLO_FLAGS>(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    static_cast<IDXGIDebug*>(m_dxgiDebug)->Release();
    m_dxgiDebug = nullptr;

    FreeLibrary(static_cast<HMODULE>(m_dxgiDebugModule));
    m_dxgiDebugModule = nullptr;
#endif

    /// 
}

void Renderer::ClearScreen(const Rgba8& clearColor)
{
    /*glClearColor(clearColor.r / 255.f, clearColor.g / 255.f, clearColor.b / 255.f, clearColor.a);
    // Note; glClearColor takes colors as floats in [0,1], not bytes in [0,255]
    glClear(GL_COLOR_BUFFER_BIT); // ALWAYS clear the screen at the top of each frame's Render()!*/

    /// DirectX
    // Set render target
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilDSV);
    float colorAsFloats[4];
    clearColor.GetAsFloats(colorAsFloats);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, colorAsFloats);
    m_deviceContext->ClearDepthStencilView(m_depthStencilDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Renderer::BeginCamera(const Camera& camera)
{
    //UNUSED(camera)
    /// DirectX
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX       = RangeMap(camera.m_viewPort.m_mins.x, 0.f, 1.f, 0, static_cast<float>(m_config.m_window->GetClientDimensions().x)); // Using different coordinate system
    viewport.TopLeftY       = RangeMap(1.f - camera.m_viewPort.m_maxs.y, 0.f, 1.f, 0, static_cast<float>(m_config.m_window->GetClientDimensions().y)); // revert the top left to bottom left
    viewport.Width          = RangeMap(camera.m_viewPort.GetDimensions().x, 0.f, 1.f, 0, static_cast<float>(m_config.m_window->GetClientDimensions().x));
    viewport.Height         = RangeMap(camera.m_viewPort.GetDimensions().y, 0.f, 1.f, 0, static_cast<float>(m_config.m_window->GetClientDimensions().y));
    viewport.MinDepth       = 0.f;
    viewport.MaxDepth       = 1.f;
    m_deviceContext->RSSetViewports(1, &viewport);

    CameraConstants cameraConstant         = {};
    cameraConstant.RenderToClipTransform   = camera.GetProjectionMatrix();
    cameraConstant.CameraToRenderTransform = camera.GetCameraToRenderTransform();
    cameraConstant.WorldToCameraTransform  = camera.GetWorldToCameraTransform();
    cameraConstant.CameraToWorldTransform  = camera.GetCameraToWorldTransform();

    CopyCPUToGPU(&cameraConstant, sizeof(cameraConstant), m_cameraCBO);
    BindConstantBuffer(k_cameraConstantsSlot, m_cameraCBO);

    /// TODO: 
    LightingConstants lightingConstant = {};

    SetModelConstants();

    /// OpenGL
    /*Vec2 bottom_left = camera.GetOrthoBottomLeft();
    Vec2 top_right   = camera.GetOrthoTopRight();
    glLoadIdentity();
    glOrtho(bottom_left.x, top_right.x, bottom_left.y, top_right.y, 0.f, 1.f);*/
}

void Renderer::EndCamera(const Camera& camera)
{
    UNUSED(camera)
}

void Renderer::DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes)
{
    /// Direct X
    unsigned int size = numVertexes * sizeof(Vertex_PCU);
    CopyCPUToGPU(vertexes, size, m_immediateVBO);
    DrawVertexBuffer(m_immediateVBO, numVertexes);

    /// OpenGL
    /*glBegin(GL_TRIANGLES);
    {
        for (int i = 0; i < numVertexes; i++)
        {
            glColor4ub(vertexes[i].m_color.r, vertexes[i].m_color.g, vertexes[i].m_color.b, vertexes[i].m_color.a);
            glTexCoord2f(vertexes[i].m_uvTextCoords.x, vertexes[i].m_uvTextCoords.y);
            glVertex2f(vertexes[i].m_position.x, vertexes[i].m_position.y);
        }
    }
    glEnd();*/
}

void Renderer::DrawVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes)
{
    unsigned int size = numVertexes * sizeof(Vertex_PCUTBN);
    CopyCPUToGPU(vertexes, size, m_immediateVBO_TBN);
    DrawVertexBuffer(m_immediateVBO_TBN, numVertexes);
}

void Renderer::DrawIndexedVertexArray(int numVertexes, const Vertex_PCU* vertexes, const unsigned int* indices, unsigned int numIndices)
{
    if (numVertexes == 0 || vertexes == nullptr || indices == nullptr || numIndices == 0)
        ERROR_RECOVERABLE("Invalid number of vertexes or vertexes pointer is null.")
    // 1. 上传顶点数据到 m_immediateVBO
    unsigned int vertexDataSize = numVertexes * sizeof(Vertex_PCU);
    unsigned int indexDataSize  = numIndices * sizeof(unsigned int);
    CopyCPUToGPU(vertexes, vertexDataSize, m_immediateVBO);
    CopyCPUToGPU(indices, indexDataSize, m_immediateIBO);

    // 3. 绘制索引顶点缓冲区
    BindVertexBuffer(m_immediateVBO);
    BindIndexBuffer(m_immediateIBO);
    m_deviceContext->DrawIndexed(numIndices, 0, 0);
}

void Renderer::DrawIndexedVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes, const unsigned int* indices, unsigned int numIndices)
{
    // 1. 上传顶点数据到 m_immediateVBO
    unsigned int vertexDataSize = numVertexes * sizeof(Vertex_PCUTBN);
    CopyCPUToGPU(vertexes, vertexDataSize, m_immediateVBO_TBN);

    // 2. 更新索引缓冲区 m_immediateIBO
    m_immediateIBO->Update(indices, numIndices, m_deviceContext);

    // 3. 绘制索引顶点缓冲区
    DrawIndexedVertexBuffer(m_immediateVBO_TBN, m_immediateIBO, numIndices);
}

void Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& vertexes)
{
    DrawVertexArray(static_cast<int>(vertexes.size()), vertexes.data());
}

void Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& vertexes)
{
    DrawVertexArray(static_cast<int>(vertexes.size()), vertexes.data());
}

void Renderer::DrawIndexedVertexArray(const std::vector<Vertex_PCU>& vertexes, const std::vector<unsigned>& indexes)
{
    // 利用前面实现的重载版本
    DrawIndexedVertexArray(static_cast<int>(vertexes.size()), vertexes.data(), indexes.data(), static_cast<unsigned int>(indexes.size()));
}

void Renderer::DrawIndexedVertexArray(const std::vector<Vertex_PCUTBN>& vertexes, const std::vector<unsigned>& indexes)
{
    DrawIndexedVertexArray(static_cast<int>(vertexes.size()), vertexes.data(), indexes.data(), static_cast<unsigned int>(indexes.size()));
}


BitmapFont* Renderer::CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension)
{
    for (BitmapFont* bitmap : m_loadedFonts)
    {
        if (bitmap->m_fontFilePathNameWithNoExtension == bitmapFontFilePathWithNoExtension)
        {
            return bitmap;
        }
    }
    Texture* fontTexture = CreateOrGetTextureFromFile(bitmapFontFilePathWithNoExtension);
    return CreateBitmapFont(bitmapFontFilePathWithNoExtension, *fontTexture);
}

Texture* Renderer::CreateOrGetTextureFromFile(const char* imageFilePath)
{
    // See if we already have this texture previously loaded
    Texture* existingTexture = GetTextureForFileName(imageFilePath); // You need to write this
    if (existingTexture)
    {
        return existingTexture;
    }

    // Never seen this texture before!  Let's load it.
    Texture* newTexture = CreateTextureFromFile(imageFilePath);
    return newTexture;
}

/*void Renderer::CreateRenderingContext()
{
    // Creates an OpenGL rendering context (RC) and binds it to the current window's device context (DC)
    PIXELFORMATDESCRIPTOR pixelFormatDescriptor;
    memset(&pixelFormatDescriptor, 0, sizeof(pixelFormatDescriptor));
    pixelFormatDescriptor.nSize        = sizeof(pixelFormatDescriptor);
    pixelFormatDescriptor.nVersion     = 1;
    pixelFormatDescriptor.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDescriptor.iPixelType   = PFD_TYPE_RGBA;
    pixelFormatDescriptor.cColorBits   = 24;
    pixelFormatDescriptor.cDepthBits   = 24;
    pixelFormatDescriptor.cAccumBits   = 0;
    pixelFormatDescriptor.cStencilBits = 8;

    // These two OpenGL-like functions (wglCreateContext and wglMakeCurrent) will remain here for now.

    HDC displayContext = static_cast<HDC>(m_config.m_window->GetDisplayContext());

    int pixelFormatCode = ChoosePixelFormat(displayContext, &pixelFormatDescriptor);
    SetPixelFormat(displayContext, pixelFormatCode, &pixelFormatDescriptor);
    m_apiRenderingContext = wglCreateContext(displayContext);
    wglMakeCurrent(displayContext, static_cast<HGLRC>(m_apiRenderingContext));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}*/

Image* Renderer::CreateImageFromFile(const char* imageFilePath)
{
    auto image = new Image(imageFilePath);
    return image;
}

Texture* Renderer::CreateTextureFromImage(Image& image)
{
    auto newTexture          = new Texture();
    newTexture->m_dimensions = image.GetDimensions();

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width                = image.GetDimensions().x;
    textureDesc.Height               = image.GetDimensions().y;
    textureDesc.MipLevels            = 1;
    textureDesc.ArraySize            = 1;
    textureDesc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count     = 1;
    textureDesc.Usage                = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureData;
    textureData.pSysMem     = image.GetRawData();
    textureData.SysMemPitch = 4 * image.GetDimensions().x;

    HRESULT hr;
    hr = m_device->CreateTexture2D(&textureDesc, &textureData, &newTexture->m_texture);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE(Stringf("CreateTextureFromImage failed for image file \"%s\".", image.GetImageFilePath().c_str()))

    hr = m_device->CreateShaderResourceView(newTexture->m_texture, nullptr, &newTexture->m_shaderResourceView);
    if (!SUCCEEDED(hr))
        ERROR_AND_DIE(Stringf("CreateShaderResourceView failed for image file \"%s\".", image.GetImageFilePath().c_str()));

    // Remember to keep the texture in thee list
    m_loadedTextures.push_back(newTexture);
    return newTexture;
}

Texture* Renderer::CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData)
{
    // Check if the load was successful
    GUARANTEE_OR_DIE(texelData, Stringf( "CreateTextureFromData failed for \"%s\" - texelData was null!", name ))
    GUARANTEE_OR_DIE(bytesPerTexel >= 3 && bytesPerTexel <= 4,
                     Stringf( "CreateTextureFromData failed for \"%s\" - unsupported BPP=%i (must be 3 or 4)", name,
                         bytesPerTexel ))
    GUARANTEE_OR_DIE(dimensions.x > 0 && dimensions.y > 0,
                     Stringf( "CreateTextureFromData failed for \"%s\" - illegal texture dimensions (%i x %i)", name,
                         dimensions.x, dimensions.y ))

    auto newTexture          = new Texture();
    newTexture->m_name       = name; // NOTE: m_name must be a std::string, otherwise it may point to temporary data!
    newTexture->m_dimensions = dimensions;

    // Enable OpenGL texturing
    glEnable(GL_TEXTURE_2D);

    // Tell OpenGL that our pixel data is single-byte aligned
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Ask OpenGL for an unused texName (ID number) to use for this texture
    glGenTextures(1, &newTexture->m_openglTextureID);

    // Tell OpenGL to bind (set) this as the currently active texture
    glBindTexture(GL_TEXTURE_2D, newTexture->m_openglTextureID);

    // Set texture clamp vs. wrap (repeat) default settings
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // GL_CLAMP or GL_REPEAT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // GL_CLAMP or GL_REPEAT

    // Set magnification (texel > pixel) and minification (texel < pixel) filters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // one of: GL_NEAREST, GL_LINEAR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // one of: GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR

    // Pick the appropriate OpenGL format (RGB or RGBA) for this texel data
    GLenum bufferFormat = GL_RGBA;
    // the format our source pixel data is in; any of: GL_RGB, GL_RGBA, GL_LUMINANCE, GL_LUMINANCE_ALPHA, ...
    if (bytesPerTexel == 3)
    {
        bufferFormat = GL_RGB;
    }
    GLenum internalFormat = bufferFormat;
    // the format we want the texture to be on the card; technically allows us to translate into a different texture format as we upload to OpenGL

    // Upload the image texel data (raw pixels bytes) to OpenGL under the currently-bound OpenGL texture ID
    glTexImage2D( // Upload this pixel data to our new OpenGL texture
        GL_TEXTURE_2D, // Creating this as a 2d texture
        0, // Which mipmap level to use as the "root" (0 = the highest-quality, full-res image), if mipmaps are enabled
        internalFormat, // Type of texel format we want OpenGL to use for this texture internally on the video card
        dimensions.x,
        // Texel-width of image; for maximum compatibility, use 2^N + 2^B, where N is some integer in the range [3,11], and B is the border thickness [0,1]
        dimensions.y,
        // Texel-height of image; for maximum compatibility, use 2^M + 2^B, where M is some integer in the range [3,11], and B is the border thickness [0,1]
        0, // Border size, in texels (must be 0 or 1, recommend 0)
        bufferFormat, // Pixel format describing the composition of the pixel data in buffer
        GL_UNSIGNED_BYTE, // Pixel color components are unsigned bytes (one byte per color channel/component)
        texelData); // Address of the actual pixel data bytes/buffer in system memory

    m_loadedTextures.push_back(newTexture);
    return newTexture;
}

void Renderer::BindTexture(Texture* texture, int slot)
{
    /// OpenGL
    /*if (texture)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture->m_openglTextureID);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }*/
    ///

    /// DirectX
    const Texture* bindTexture = nullptr;
    if (texture == nullptr)
        bindTexture = m_defaultTexture;
    else
    {
        bindTexture = texture;
    }

    m_deviceContext->PSSetShaderResources(slot, 1, &bindTexture->m_shaderResourceView);
    /// 
}

void Renderer::SetBlendMode(BlendMode blendMode)
{
    /// OpenGL
    /*if (blendMode == BlendMode::ALPHA) // enum class BlendMode, defined near top of Renderer.hpp
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if (blendMode == BlendMode::ADDITIVE)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    else
    {
        ERROR_AND_DIE(Stringf( "Unknown / unsupported blend mode #%i", blendMode ))
    }*/
    ///

    /// DirectX
    m_desiredBlendMode = blendMode;
    /// 
}

void Renderer::SetRasterizerMode(RasterizerMode rasterizerMode)
{
    m_desiredRasterizerMode = rasterizerMode;
}

void Renderer::SetDepthMode(DepthMode depthMode)
{
    m_desiredDepthMode = depthMode;
}

void Renderer::SetSamplerMode(SamplerMode samplerMode)
{
    m_samplerState                  = m_samplerStates[static_cast<int>(samplerMode)];
    ID3D11SamplerState* samplers[3] = {m_samplerState, m_samplerState, m_samplerState};
    //m_deviceContext->PSSetSamplers(0, 1, &m_samplerState);
    m_deviceContext->PSSetSamplers(0, 3, samplers);
}

void Renderer::SetModelConstants(const Mat44& modelToWorldTransform, const Rgba8& modelColor)
{
    ModelConstants modelConstants        = {};
    modelConstants.ModelToWorldTransform = modelToWorldTransform;
    modelColor.GetAsFloats(modelConstants.ModelColor);
    CopyCPUToGPU(&modelConstants, sizeof(modelConstants), m_modelCBO);
    BindConstantBuffer(k_modelConstantsSlot, m_modelCBO);
}

void Renderer::SetCustomConstants(void* data, int registerSlot, ConstantBuffer* constantBuffer)
{
    CopyCPUToGPU(data, static_cast<unsigned int>(constantBuffer->m_size), constantBuffer);
    BindConstantBuffer(registerSlot, constantBuffer);
}

void Renderer::SetFrameConstants(const FrameConstants& frameConstants)
{
    CopyCPUToGPU(&frameConstants, sizeof(frameConstants), m_perFrameCBO);
    BindConstantBuffer(k_perFrameConstantsSlot, m_perFrameCBO);
}

void Renderer::SetLightConstants(const LightingConstants& lightConstants)
{
    CopyCPUToGPU(&lightConstants, sizeof(lightConstants), m_lightCBO);
    BindConstantBuffer(k_lightConstantsSlot, m_lightCBO);
}

void Renderer::SetLightConstants(Vec3 sunDirection, float sunIntensity, float ambientIntensity)
{
    LightingConstants lightConstants = {};
    lightConstants.SunDirection      = sunDirection.GetNormalized();
    lightConstants.AmbientIntensity  = ambientIntensity;
    lightConstants.SunIntensity      = sunIntensity;
    CopyCPUToGPU(&lightConstants, sizeof(lightConstants), m_lightCBO);
    BindConstantBuffer(k_lightConstantsSlot, m_lightCBO);
}

/// When creating shaders for vertexs that should be lit, such as the map, pass the type Vertex_PCUTBN.
/// @param shaderName 
/// @param shaderSource 
/// @param vertexType 
/// @return 
Shader* Renderer::CreateShader(const char* shaderName, const char* shaderSource, VertexType vertexType)
{
    ShaderConfig shaderConfig;
    shaderConfig.m_name               = shaderName;
    auto                       shader = new Shader(shaderConfig);
    std::vector<unsigned char> outVertexShaderByteCode;
    std::vector<unsigned char> outPixelShaderByteCode;
    HRESULT                    hr;

    CompileShaderToByteCode(outVertexShaderByteCode, shaderName, shaderSource, shader->m_config.m_vertexEntryPoint.c_str(), "vs_5_0");
    hr = m_device->CreateVertexShader(outVertexShaderByteCode.data(), outVertexShaderByteCode.size(), nullptr, &shader->m_vertexShader);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE(Stringf("Could not create vertex shader."))
    }
    CompileShaderToByteCode(outPixelShaderByteCode, shaderName, shaderSource, shader->m_config.m_pixelEntryPoint.c_str(), "ps_5_0");

    hr = m_device->CreatePixelShader(outPixelShaderByteCode.data(), outPixelShaderByteCode.size(), nullptr, &shader->m_pixelShader);

    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE(Stringf("Could not create pixel shader."))
    }

    if (vertexType == VertexType::Vertex_PCU)
    {
        // Create an input layout and store it on the new shader we created
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        UINT numElements = ARRAYSIZE(inputElementDesc);
        hr               = m_device->CreateInputLayout(
            inputElementDesc, numElements, outVertexShaderByteCode.data(), outVertexShaderByteCode.size(), &shader->m_inputLayout);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create vertex layout")
        }

        return shader;
    }
    if (vertexType == VertexType::Vertex_PCUTBN)
    {
        // Create an input layout and store it on the new shader we created
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, /* Be careful of the alignment*/D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        UINT numElements = ARRAYSIZE(inputElementDesc);
        hr               = m_device->CreateInputLayout(
            inputElementDesc, numElements, outVertexShaderByteCode.data(), outVertexShaderByteCode.size(), &shader->m_inputLayout);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create vertex layout")
        }

        return shader;
    }
    return nullptr;
}

Shader* Renderer::GetShader(const char* shaderName)
{
    for (Shader* shader : m_loadedShaders)
    {
        if (shader->GetName() == shaderName)
        {
            return shader;
        }
    }
    return nullptr;
}

Shader* Renderer::CreateShader(const char* shaderName, VertexType vertexType)
{
    std::string shaderSource;
    std::string shaderNameWithExtension = ".enigma/data/Shaders/" + std::string(shaderName) + ".hlsl";
    int         readResult              = FileReadToString(shaderSource, shaderNameWithExtension);
    if (readResult == 0)
    {
        ERROR_RECOVERABLE(Stringf( "Could not read shader \"%s\"", shaderName ))
    }
    return CreateShader(shaderName, shaderSource.c_str(), vertexType);
}

Shader* Renderer::CreateShaderFromFile(const char* sourcePath, VertexType vertexType)
{
    std::string shaderNameWithExtension = std::string(sourcePath) + ".hlsl";
    Strings     pathVec                 = SplitStringOnDelimiter(shaderNameWithExtension, '/');
    Strings     shaderNamePath          = SplitStringOnDelimiter(sourcePath, '/');
    std::string shaderName              = shaderNamePath.back();
    if (shaderName == "Default")
    {
        printf("Renderer::CreateShaderFromFile    ! Shader name is Default, using Default shader and return nullptr.\n");
        return nullptr;
    }

    if (pathVec.size() == 0)
    {
        ERROR_AND_DIE(Stringf("Could not split path \"%s\"", sourcePath));
    }
    std::string shaderSource;
    int         readResult = FileReadToString(shaderSource, shaderNameWithExtension);
    if (readResult == 0)
    {
        ERROR_RECOVERABLE(Stringf( "Could not read shader \"%s\"", readResult ))
    }
    Strings fileSuffixVec = SplitStringOnDelimiter(pathVec.back(), '.');

    return CreateShader(fileSuffixVec[0].c_str(), shaderSource.c_str(), vertexType);
}

// TODO: Check the Error and Shader Blob memory leak
bool Renderer::CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, const char* name, const char* source, const char* entryPoint, const char* target)
{
    bool outFlag = false;
    // Compile vertex shader
    DWORD shaderFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(ENGINE_DEBUG_RENDER)
    shaderFlags = D3DCOMPILE_DEBUG;
    shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    shaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
#endif
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob  = nullptr;

    HRESULT hr = D3DCompile(source, strlen(source), name, nullptr, nullptr, entryPoint, target, shaderFlags, 0, &shaderBlob, &errorBlob);
    if (SUCCEEDED(hr))
    {
        outByteCode.resize(shaderBlob->GetBufferSize());
        memcpy(outByteCode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
        outFlag = true;
    }
    else
    {
        outFlag = false;
        if (errorBlob != nullptr)
        {
            DebuggerPrintf(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ERROR_AND_DIE(Stringf("Could not compile vertex shader."))
    }
    DX_SAFE_RELEASE(errorBlob)
    DX_SAFE_RELEASE(shaderBlob)
    return outFlag;
}

void Renderer::BindShader(Shader* shader)
{
    if (shader == nullptr)
    {
        shader = m_defaultShader;
    }

    m_deviceContext->VSSetShader(shader->m_vertexShader, nullptr, 0);
    m_deviceContext->PSSetShader(shader->m_pixelShader, nullptr, 0);
    m_deviceContext->IASetInputLayout(shader->m_inputLayout);
}

VertexBuffer* Renderer::CreateVertexBuffer(const unsigned int size, unsigned int stride)
{
    return new VertexBuffer(m_device, size, stride);
}

IndexBuffer* Renderer::CreateIndexBuffer(unsigned int size)
{
    return new IndexBuffer(m_device, size);
}

ConstantBuffer* Renderer::CreateConstantBuffer(const unsigned int size)
{
    auto              constantBuffer = new ConstantBuffer(size);
    D3D11_BUFFER_DESC bufferDesc     = {};
    bufferDesc.Usage                 = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth             = static_cast<UINT>(size);
    bufferDesc.BindFlags             = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags        = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer->m_buffer);
    if (!SUCCEEDED(hr))
    {
        ERROR_AND_DIE("Could not create constant buffer.")
    }
    return constantBuffer;
}

void Renderer::CopyCPUToGPU(const void* data, unsigned int size, VertexBuffer* vbo)
{
    if (size > vbo->GetSize())
    {
        vbo->Resize(size);
    }

    // Copy vertices
    D3D11_MAPPED_SUBRESOURCE resource;
    m_deviceContext->Map(vbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    memcpy(resource.pData, data, size); // Only memory copy we specify not the whole buffer size
    m_deviceContext->Unmap(vbo->m_buffer, 0);
}

void Renderer::CopyCPUToGPU(const void* data, unsigned int size, ConstantBuffer* cbo)
{
    D3D11_MAPPED_SUBRESOURCE resource;
    m_deviceContext->Map(cbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    memcpy(resource.pData, data, size);
    m_deviceContext->Unmap(cbo->m_buffer, 0);
}

void Renderer::CopyCPUToGPU(const void* data, unsigned int size, IndexBuffer* ibo)
{
    if (size > ibo->GetSize())
    {
        ibo->Resize(size);
    }
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    m_deviceContext->Map(ibo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, data, size);
    m_deviceContext->Unmap(ibo->m_buffer, 0);
}

void Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
    UINT offset = 0;
    /// 你直接传递了 0，但根据 DirectX 的 API 设计，pOffsets 必须是一个指向偏移量的指针。
    /// 你的代码传递了 0，这会被解释为 NULL 指针，导致 DirectX 内部无法正确读取偏移量。
    m_deviceContext->IASetVertexBuffers(0, 1, &vbo->m_buffer, &vbo->m_stride, &offset);
    m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
    UINT offset = 0;
    m_deviceContext->IASetIndexBuffer(ibo->m_buffer, DXGI_FORMAT_R32_UINT, offset);
}

void Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
    m_deviceContext->VSSetConstantBuffers(slot, 1, &cbo->m_buffer);
    m_deviceContext->PSSetConstantBuffers(slot, 1, &cbo->m_buffer);
}

void Renderer::SetStatesIfChanged()
{
    if (m_blendStates[static_cast<int>(m_desiredBlendMode)] != m_blendState)
    {
        m_blendState         = m_blendStates[static_cast<int>(m_desiredBlendMode)];
        float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        UINT  sampleMask     = 0xffffffff;
        m_deviceContext->OMSetBlendState(m_blendState, blendFactor, sampleMask);
    }

    if (m_rasterizerStates[static_cast<int>(m_desiredRasterizerMode)] != m_rasterizerState)
    {
        m_rasterizerState = m_rasterizerStates[static_cast<int>(m_desiredRasterizerMode)];
        m_deviceContext->RSSetState(m_rasterizerStates[static_cast<int>(m_desiredRasterizerMode)]);
    }

    if (m_depthStencilStates[static_cast<int>(m_desiredDepthMode)] != m_depthStencilState)
    {
        m_depthStencilState = m_depthStencilStates[static_cast<int>(m_desiredDepthMode)];
        m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);
    }
}

void Renderer::DrawVertexBuffer(VertexBuffer* vbo, unsigned int vertexCount)
{
    SetStatesIfChanged();
    BindVertexBuffer(vbo);
    m_deviceContext->Draw(vertexCount, 0);
}

void Renderer::DrawIndexedVertexBuffer(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount)
{
    SetStatesIfChanged();
    BindVertexBuffer(vbo);
    BindIndexBuffer(ibo);
    m_deviceContext->DrawIndexed(indexCount, 0, 0);
}

Texture* Renderer::CreateTextureFromFile(const char* imageFilePath)
{
    /// Deprecated
    /*IntVec2 dimensions    = IntVec2::ZERO; // This will be filled in for us to indicate image width & height
    int     bytesPerTexel = 0; // ...and how many color components the image had (e.g. 3=RGB=24bit, 4=RGBA=32bit)

    // Load (and decompress) the image RGB(A) bytes from a file on disk into a memory buffer (array of bytes)
    stbi_set_flip_vertically_on_load(1); // We prefer uvTexCoords has origin (0,0) at BOTTOM LEFT
    unsigned char* texelData = stbi_load(imageFilePath, &dimensions.x, &dimensions.y, &bytesPerTexel, 0);

    // Check if the load was successful
    GUARANTEE_OR_DIE(texelData, Stringf( "Failed to load image \"%s\"", imageFilePath ))

    Texture* newTexture = CreateTextureFromData(imageFilePath, dimensions, bytesPerTexel, texelData);

    // Free the raw image texel data now that we've sent a copy of it down to the GPU to be stored in video memory
    stbi_image_free(texelData);

    return newTexture;*/

    ///

    Image*   image   = CreateImageFromFile(imageFilePath);
    Texture* texture = CreateTextureFromImage(*image);
    delete image;
    return texture;
}

Texture* Renderer::GetTextureForFileName(const char* imageFilePath)
{
    for (Texture*& m_loaded_texture : m_loadedTextures)
    {
        if (m_loaded_texture && m_loaded_texture->GetImageFilePath() == imageFilePath)
        {
            return m_loaded_texture;
        }
    }
    return nullptr;
}

BitmapFont* Renderer::CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture)
{
    auto bitmapFont = new BitmapFont(bitmapFontFilePathWithNoExtension, fontTexture);
    m_loadedFonts.push_back(bitmapFont);
    return bitmapFont;
}
