#include "DX11Renderer.hpp"

#include <filesystem>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Window/Window.hpp"

#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/GraphicsError.hpp"
#include "Engine/Renderer/RenderTarget.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Texture.hpp"

DX11Renderer::DX11Renderer(RenderConfig& config)
{
    m_config = config;
}

void DX11Renderer::Startup()
{
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
    /// @link https://stackoverflow.com/questions/65246961/does-the-backbuffer-that-a-rendertargetview-points-to-automagically-change-after
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


    m_defaultShader = CreateOrGetShader(m_config.m_defaultShader.c_str(), VertexType::Vertex_PCU);
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
    hr                                              = m_device->CreateBlendState(&blendDesc, &m_blendStates[static_cast<int>(BlendMode::OPAQUE)]);
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
}

void DX11Renderer::Shutdown()
{
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

    for (Shader* m_loaded_shader : m_loadedShaders)
    {
        POINTER_SAFE_DELETE(m_loaded_shader)
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
}

void DX11Renderer::BeginFrame()
{
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilDSV);
}

void DX11Renderer::EndFrame()
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

void DX11Renderer::ClearScreen(const Rgba8& clear)
{
    // Set render target
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilDSV);
    float colorAsFloats[4];
    clear.GetAsFloats(colorAsFloats);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, colorAsFloats);
    m_deviceContext->ClearDepthStencilView(m_depthStencilDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void DX11Renderer::BeginCamera(const Camera& camera)
{
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
}

void DX11Renderer::EndCamera(const Camera& camera)
{
    UNUSED(camera)
}

void DX11Renderer::SetModelConstants(const Mat44& modelToWorldTransform, const Rgba8& modelColor)
{
    ModelConstants modelConstants        = {};
    modelConstants.ModelToWorldTransform = modelToWorldTransform;
    modelColor.GetAsFloats(modelConstants.ModelColor);
    CopyCPUToGPU(&modelConstants, sizeof(modelConstants), m_modelCBO);
    BindConstantBuffer(k_modelConstantsSlot, m_modelCBO);
}

void DX11Renderer::SetDirectionalLightConstants(const DirectionalLightConstants& dl)
{
    UNUSED(dl)
}

void DX11Renderer::SetFrameConstants(const FrameConstants& frameConstants)
{
    CopyCPUToGPU(&frameConstants, sizeof(frameConstants), m_perFrameCBO);
    BindConstantBuffer(k_perFrameConstantsSlot, m_perFrameCBO);
}

void DX11Renderer::SetLightConstants(const LightingConstants& lightConstants)
{
    CopyCPUToGPU(&lightConstants, sizeof(lightConstants), m_lightCBO);
    BindConstantBuffer(k_lightConstantsSlot, m_lightCBO);
}

void DX11Renderer::SetCustomConstantBuffer(ConstantBuffer*& constantBuffer, void* data, int registerSlot)
{
    CopyCPUToGPU(data, static_cast<unsigned int>(constantBuffer->m_size), constantBuffer);
    BindConstantBuffer(registerSlot, constantBuffer);
}

void DX11Renderer::SetBlendMode(BlendMode mode)
{
    m_desiredBlendMode = mode;
}

void DX11Renderer::SetRasterizerMode(RasterizerMode mode)
{
    m_desiredRasterizerMode = mode;
}

void DX11Renderer::SetDepthMode(DepthMode mode)
{
    m_desiredDepthMode = mode;
}

void DX11Renderer::SetSamplerMode(SamplerMode mode, int slot)
{
    UNUSED(slot)
    m_samplerState                  = m_samplerStates[static_cast<int>(mode)];
    ID3D11SamplerState* samplers[3] = {m_samplerState, m_samplerState, m_samplerState};
    //m_deviceContext->PSSetSamplers(0, 1, &m_samplerState);
    // TODO: Explicit define the max samplers, in general should consist with DX12Renderer and should defined in IRenderer
    m_deviceContext->PSSetSamplers(0, 3, samplers);
}

Shader* DX11Renderer::CreateShader(const char* shaderName, const char* shaderSource, VertexType vertexType)
{
    return CreateShaderFromSource(shaderName, shaderSource, "VertexMain", "PixelMain", vertexType);
}

Shader* DX11Renderer::CreateShader(const char* shaderName, VertexType vertexType)
{
    std::string shaderSource;
    std::string shaderNameWithExtension = std::string(shaderName) + ".hlsl";
    int         readResult              = FileReadToString(shaderSource, shaderNameWithExtension);
    if (readResult == 0)
    {
        ERROR_RECOVERABLE(Stringf( "Could not read shader \"%s\"", shaderName ))
    }
    return CreateShader(shaderName, shaderSource.c_str(), vertexType);
}

/**
 * Creates a shader from the specified file path and entry points for the vertex and pixel shaders.
 *
 * This method reads the shader source from the given file, handles errors if the file cannot be read,
 * and delegates the shader creation to `CreateShaderFromSource`.
 *
 * @param name The name of the shader, used for identification purposes.
 * @param shaderPath The file path to the shader source code.
 * @param vsEntryPoint The entry point function name for the vertex shader.
 * @param psEntryPoint The entry point function name for the pixel shader.
 * @param vertexType The type of vertex data that the shader will handle, specified as a VertexType enum.
 * @return A pointer to the created Shader, or nullptr if the shader creation failed.
 *         The caller is responsible for managing the lifetime of the returned Shader.
 */
Shader* DX11Renderer::CreateShader(const char* name, const char* shaderPath, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType)
{
    std::string           shaderSource;
    std::filesystem::path path       = std::filesystem::path(shaderPath);
    int                   readResult = FileReadToString(shaderSource, path.string());
    if (readResult <= 0)
    {
        ERROR_RECOVERABLE(Stringf("Failed to read shader file: %s", path.c_str()));
        return nullptr;
    }
    return CreateShaderFromSource(name, shaderSource.c_str(), vsEntryPoint, psEntryPoint, vertexType);
}

/**
 * Creates a shader from source code and entry points for vertex and pixel shaders.
 *
 * This method compiles the provided shader source into bytecode, creates vertex and pixel shaders,
 * and initializes the shader's input layout based on the specified vertex type. If any step fails,
 * an error is logged and the method terminates the application.
 *
 * @param name The name of the shader, used for identification purposes.
 * @param shaderSource The source code of the shader to be compiled.
 * @param vsEntryPoint The entry point function name for the vertex shader.
 * @param psEntryPoint The entry point function name for the pixel shader.
 * @param vertexType The type of vertex data that the shader will handle, specified as a VertexType enum.
 * @return A pointer to the created Shader. The caller is responsible for managing the lifetime of the returned Shader.
 *         If an error occurs during creation, the application terminates.
 */
Shader* DX11Renderer::CreateShaderFromSource(const char* name, const char* shaderSource, const char* vsEntryPoint, const char* psEntryPoint, VertexType vertexType)
{
    // Shader Config
    ShaderConfig shaderConfig;
    shaderConfig.m_name             = name;
    shaderConfig.m_vertexEntryPoint = vsEntryPoint;
    shaderConfig.m_pixelEntryPoint  = psEntryPoint;

    auto                 shader = new Shader(shaderConfig);
    std::vector<uint8_t> vertexShaderByteCode;
    std::vector<uint8_t> pixelShaderByteCode;

    if (!CompileShaderToByteCode(vertexShaderByteCode, name, shaderSource, vsEntryPoint, "vs_5_0"))
    {
        delete shader;
        ERROR_AND_DIE(Stringf("Failed to compile vertex shader '%s' with entry point '%s'", name, vsEntryPoint))
    }
    // Vertex Shader
    HRESULT hr = m_device->CreateVertexShader(vertexShaderByteCode.data(), vertexShaderByteCode.size(), nullptr, &shader->m_vertexShader);
    if (FAILED(hr))
    {
        delete shader;
        ERROR_AND_DIE(Stringf("Failed to create vertex shader '%s'", name))
    }

    if (!CompileShaderToByteCode(pixelShaderByteCode, name, shaderSource, psEntryPoint, "ps_5_0"))
    {
        shader->m_vertexShader->Release();
        delete shader;
        ERROR_AND_DIE(Stringf("Failed to compile pixel shader '%s' with entry point '%s'", name, psEntryPoint));
    }

    // Pixel Shader
    hr = m_device->CreatePixelShader(pixelShaderByteCode.data(), pixelShaderByteCode.size(), nullptr, &shader->m_pixelShader);
    if (FAILED(hr))
    {
        shader->m_vertexShader->Release();
        delete shader;
        ERROR_AND_DIE(Stringf("Failed to create pixel shader '%s'", name))
    }

    CreateInputLayoutFromShader(shader, vertexShaderByteCode, vertexType); // Create input layout
    return shader;
}

Shader* DX11Renderer::CreateOrGetShader(const char* sourcePath, VertexType vertexType)
{
    std::string shaderNameWithExtension = std::string(sourcePath) + ".hlsl";
    Strings     pathVec                 = SplitStringOnDelimiter(shaderNameWithExtension, '/');
    Strings     shaderNamePath          = SplitStringOnDelimiter(sourcePath, '/');
    std::string shaderName              = shaderNamePath.back();
    for (Shader* shader : m_loadedShaders)
    {
        if (shader->GetName() == shaderName)
        {
            return shader;
        }
    }
    Shader* newShader = CreateShader(sourcePath, vertexType);
    m_loadedShaders.push_back(newShader);
    return newShader;
}

BitmapFont* DX11Renderer::CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension)
{
    for (BitmapFont* bitmap : m_loadedFonts)
    {
        if (bitmap->m_fontFilePathNameWithNoExtension == bitmapFontFilePathWithNoExtension)
        {
            return bitmap;
        }
    }
    Texture* fontTexture = CreateOrGetTexture(bitmapFontFilePathWithNoExtension);
    return CreateBitmapFont(bitmapFontFilePathWithNoExtension, *fontTexture);
}

bool DX11Renderer::CompileShaderToByteCode(std::vector<uint8_t>& outBytes, const char* name, const char* source, const char* entryPoint, const char* target)
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
        outBytes.resize(shaderBlob->GetBufferSize());
        memcpy(outBytes.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
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

void DX11Renderer::BindShader(Shader* shader)
{
    if (shader == nullptr)
    {
        shader = m_defaultShader;
    }

    m_deviceContext->VSSetShader(shader->m_vertexShader, nullptr, 0);
    m_deviceContext->PSSetShader(shader->m_pixelShader, nullptr, 0);
    m_deviceContext->IASetInputLayout(shader->m_inputLayout);
}

Texture* DX11Renderer::CreateOrGetTexture(const char* imageFilePath)
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

Image* DX11Renderer::CreateImageFromFile(const char* imageFilePath)
{
    return IRenderer::CreateImageFromFile(imageFilePath);
}

Texture* DX11Renderer::CreateTextureFromImage(Image& image)
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

Texture* DX11Renderer::CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData)
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

    m_loadedTextures.push_back(newTexture);
    return newTexture;
}

Texture* DX11Renderer::CreateTextureFromFile(const char* imageFilePath)
{
    Image*   image   = CreateImageFromFile(imageFilePath);
    Texture* texture = CreateTextureFromImage(*image);
    delete image;
    return texture;
}

Texture* DX11Renderer::GetTextureForFileName(const char* imageFilePath)
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

BitmapFont* DX11Renderer::CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture)
{
    auto bitmapFont = new BitmapFont(bitmapFontFilePathWithNoExtension, fontTexture);
    m_loadedFonts.push_back(bitmapFont);
    return bitmapFont;
}

VertexBuffer* DX11Renderer::CreateVertexBuffer(size_t size, unsigned stride)
{
    return new VertexBuffer(m_device, (int)size, stride);
}

IndexBuffer* DX11Renderer::CreateIndexBuffer(size_t size)
{
    return new IndexBuffer(m_device, (int)size);
}

ConstantBuffer* DX11Renderer::CreateConstantBuffer(size_t size)
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

void DX11Renderer::CopyCPUToGPU(const void* data, size_t size, VertexBuffer* vbo, size_t offset)
{
    UNUSED(offset)
    if (size > vbo->GetSize())
    {
        vbo->Resize((int)size);
    }

    // Copy vertices
    D3D11_MAPPED_SUBRESOURCE resource;
    m_deviceContext->Map(vbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    memcpy(resource.pData, data, size); // Only memory copy we specify not the whole buffer size
    m_deviceContext->Unmap(vbo->m_buffer, 0);
}

void DX11Renderer::CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset)
{
    CopyCPUToGPU((void*)data, size, v, offset);
}

void DX11Renderer::CopyCPUToGPU(const Vertex_PCUTBN* data, size_t size, VertexBuffer* v, size_t offset)
{
    CopyCPUToGPU((void*)data, size, v, offset);
}

void DX11Renderer::CopyCPUToGPU(const void* data, size_t size, IndexBuffer* ibo)
{
    if (size > ibo->GetSize())
    {
        ibo->Resize((int)size);
    }
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    m_deviceContext->Map(ibo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, data, size);
    m_deviceContext->Unmap(ibo->m_buffer, 0);
}

void DX11Renderer::CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cbo)
{
    D3D11_MAPPED_SUBRESOURCE resource;
    m_deviceContext->Map(cbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    memcpy(resource.pData, data, size);
    m_deviceContext->Unmap(cbo->m_buffer, 0);
}

void DX11Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
    UINT offset = 0;
    /// 你直接传递了 0，但根据 DirectX 的 API 设计，pOffsets 必须是一个指向偏移量的指针。
    /// 你的代码传递了 0，这会被解释为 NULL 指针，导致 DirectX 内部无法正确读取偏移量。
    m_deviceContext->IASetVertexBuffers(0, 1, &vbo->m_buffer, &vbo->m_stride, &offset);
    m_deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DX11Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
    UINT offset = 0;
    m_deviceContext->IASetIndexBuffer(ibo->m_buffer, DXGI_FORMAT_R32_UINT, offset);
}

void DX11Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
    m_deviceContext->VSSetConstantBuffers(slot, 1, &cbo->m_buffer);
    m_deviceContext->PSSetConstantBuffers(slot, 1, &cbo->m_buffer);
}

void DX11Renderer::BindTexture(Texture* texture, int slot)
{
    const Texture* bindTexture = nullptr;
    if (texture == nullptr)
        bindTexture = m_defaultTexture;
    else
    {
        bindTexture = texture;
    }

    m_deviceContext->PSSetShaderResources(slot, 1, &bindTexture->m_shaderResourceView);
}

void DX11Renderer::DrawVertexArray(int numVertexes, const Vertex_PCU* v)
{
    unsigned int size = numVertexes * sizeof(Vertex_PCU);
    CopyCPUToGPU(v, size, m_immediateVBO);
    DrawVertexBuffer(m_immediateVBO, numVertexes);
}

void DX11Renderer::DrawVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes)
{
    unsigned int size = numVertexes * sizeof(Vertex_PCUTBN);
    CopyCPUToGPU(vertexes, size, m_immediateVBO_TBN);
    DrawVertexBuffer(m_immediateVBO_TBN, numVertexes);
}

void DX11Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& vertexes)
{
    DrawVertexArray(static_cast<int>(vertexes.size()), vertexes.data());
}

void DX11Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& vertexes)
{
    DrawVertexArray(static_cast<int>(vertexes.size()), vertexes.data());
}

void DX11Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& vertexes, const std::vector<unsigned>& indexes)
{
    DrawIndexedVertexArray(static_cast<int>(vertexes.size()), vertexes.data(), indexes.data(), static_cast<unsigned int>(indexes.size()));
}

void DX11Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& vertexes, const std::vector<unsigned>& indexes)
{
    DrawIndexedVertexArray(static_cast<int>(vertexes.size()), vertexes.data(), indexes.data(), static_cast<unsigned int>(indexes.size()));
}

void DX11Renderer::DrawVertexBuffer(VertexBuffer* vbo, int vertexCount)
{
    SetStatesIfChanged();
    BindVertexBuffer(vbo);
    m_deviceContext->Draw(vertexCount, 0);
}

void DX11Renderer::DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, unsigned indexCount)
{
    SetStatesIfChanged();
    BindVertexBuffer(vbo);
    BindIndexBuffer(ibo);
    m_deviceContext->DrawIndexed(indexCount, 0, 0);
}

/**
 * Creates a new render target with the specified dimensions and format.
 *
 * This method initializes a 2D texture as the base resource for the render target.
 * It also creates a render target view (RTV) and a shader resource view (SRV)
 * for the texture, which can be used for rendering and shader binding operations.
 *
 * @param dimension The resolution of the render target, specified as an IntVec2.
 * @param format The DXGI_FORMAT that specifies the pixel format of the render target.
 * @return A pointer to the created RenderTarget. The caller is responsible for managing its lifetime.
 */
RenderTarget* DX11Renderer::CreateRenderTarget(IntVec2 dimension, DXGI_FORMAT format)
{
    RenderTarget* rt = new RenderTarget();
    rt->dimensions   = dimension;

    // Create texture 2D
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width                = dimension.x;
    textureDesc.Height               = dimension.y;
    textureDesc.MipLevels            = 1;
    textureDesc.ArraySize            = 1;
    textureDesc.Format               = format;
    textureDesc.SampleDesc.Count     = 1;
    textureDesc.SampleDesc.Quality   = 0;
    textureDesc.Usage                = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags            = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE; // 0
    textureDesc.MiscFlags            = 0;

    m_device->CreateTexture2D(&textureDesc, nullptr, &rt->texture) >> chk;

    // Create Render Target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format                        = format;
    rtvDesc.ViewDimension                 = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice            = 0;
    m_device->CreateRenderTargetView(rt->texture, &rtvDesc, &rt->rtv) >> chk;

    // Create Shader Resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                          = format;
    srvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip       = 0;
    srvDesc.Texture2D.MipLevels             = 1;
    m_device->CreateShaderResourceView(rt->texture, &srvDesc, &rt->srv) >> chk;

    return rt;
}

/**
 * Sets the active render target for subsequent rendering operations.
 *
 * This method replaces the current render target with the specified one.
 * If the provided render target is null, the method reverts to using the default
 * background buffer as the render target.
 *
 * @param renderTarget A pointer to the custom RenderTarget to set, or nullptr to use the default render target.
 *                      The render target must have a valid render target view (RTV).
 */
void DX11Renderer::SetRenderTarget(RenderTarget* renderTarget)
{
    if (renderTarget == nullptr)
    {
        // Revert to the background buffer
        m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilDSV);
        m_currentRenderTarget = &m_backBufferRenderTarget;
    }
    else
    {
        // Set a custom render target (without depth buffering)
        m_deviceContext->OMSetRenderTargets(1, &renderTarget->rtv, nullptr);
        m_currentRenderTarget = renderTarget;
    }
}

/**
 * Binds the specified render targets to the Direct3D 11 pipeline.
 *
 * This method sets up to 8 render targets for the output-merger stage of the pipeline.
 * It retrieves the render target views (RTVs) from the provided render targets and applies
 * them to the pipeline. The first render target in the array will be considered as the
 * primary render target.
 *
 * @param renderTargets An array of pointers to RenderTarget objects whose render views should be bound.
 *                      Each RenderTarget in the array must have a valid render target view (RTV).
 * @param count The number of render targets to bind. Must not exceed the DirectX limit of 8.
 */
void DX11Renderer::SetRenderTarget(RenderTarget** renderTargets, int count)
{
    ID3D11RenderTargetView* rtvs[8] = {}; // DirectX11 Support max 8 Render Target View
    for (int i = 0; i < 8; ++i)
    {
        if (renderTargets[i]->rtv)
        {
            rtvs[i] = renderTargets[i]->rtv;
        }
    }

    m_deviceContext->OMSetRenderTargets(count, rtvs, nullptr);
    m_currentRenderTarget = count > 0 ? renderTargets[0] : nullptr;
}

/**
 * Clears the specified render target and fills it with the provided clear color.
 *
 * This method resets the contents of the render target to the given color, which
 * is converted internally to a floating-point representation before being applied.
 * If the render target or its render target view (RTV) is invalid, the method returns
 * without performing any operation.
 *
 * @param renderTarget Pointer to the RenderTarget to be cleared. Must be valid and have an active RTV.
 * @param clearColor An Rgba8 structure specifying the color to fill the render target with.
 */
void DX11Renderer::ClearRenderTarget(RenderTarget* renderTarget, const Rgba8& clearColor)
{
    if (!renderTarget || !renderTarget->rtv)
        return;

    float colorAsFloat[4];
    clearColor.GetAsFloats(colorAsFloat);
    m_deviceContext->ClearRenderTargetView(renderTarget->rtv, colorAsFloat);
}

RenderTarget* DX11Renderer::GetBackBufferRenderTarget()
{
    // Lazy initialization of the backbuffer RenderTarget encapsulation
    if (!m_backBufferRenderTarget.rtv)
    {
        // Get the background buffer texture
        ID3D11Texture2D* backBuffer = nullptr;
        m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer) >> chk;

        m_backBufferRenderTarget.texture    = backBuffer;
        m_backBufferRenderTarget.rtv        = m_renderTargetView; // Use the current render target view
        m_backBufferRenderTarget.srv        = nullptr;
        m_backBufferRenderTarget.dimensions = m_config.m_window->GetClientDimensions();
    }
    return &m_backBufferRenderTarget;
}

void DX11Renderer::SetStatesIfChanged()
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

void DX11Renderer::DrawIndexedVertexArray(int numVertexes, const Vertex_PCU* vertexes, const unsigned int* indices, unsigned int numIndices)
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

void DX11Renderer::DrawIndexedVertexArray(int numVertexes, const Vertex_PCUTBN* vertexes, const unsigned int* indices, unsigned int numIndices)
{
    // 1. 上传顶点数据到 m_immediateVBO
    unsigned int vertexDataSize = numVertexes * sizeof(Vertex_PCUTBN);
    CopyCPUToGPU(vertexes, vertexDataSize, m_immediateVBO_TBN);

    // 2. 更新索引缓冲区 m_immediateIBO
    m_immediateIBO->Update(indices, numIndices, m_deviceContext);

    // 3. 绘制索引顶点缓冲区
    DrawVertexIndexed(m_immediateVBO_TBN, m_immediateIBO, numIndices);
}

/**
 * Creates an input layout for the provided shader based on the specified vertex type and vertex shader bytecode.
 *
 * Depending on the specified vertex type, an appropriate configuration of input elements is created
 * and used to generate the input layout. This input layout defines how vertex buffer data is mapped to shader input.
 * If creation fails, an error is logged, and the program terminates.
 *
 * @param shader A pointer to the Shader object where the input layout will be stored.
 * @param vertexShaderByteCode A vector containing the bytecode of the compiled vertex shader.
 * @param vertexType The type of vertex data that this shader is designed to handle, specified as a VertexType enum.
 */
void DX11Renderer::CreateInputLayoutFromShader(Shader* shader, std::vector<uint8_t> vertexShaderByteCode, VertexType vertexType)
{
    if (vertexType == VertexType::Vertex_PCU)
    {
        // Create an input layout and store it on the new shader we created
        D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        UINT    numElements = ARRAYSIZE(inputElementDesc);
        HRESULT hr;
        hr = m_device->CreateInputLayout(
            inputElementDesc, numElements, vertexShaderByteCode.data(), vertexShaderByteCode.size(), &shader->m_inputLayout);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create vertex layout")
        }
        return;
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

        UINT    numElements = ARRAYSIZE(inputElementDesc);
        HRESULT hr;
        hr = m_device->CreateInputLayout(
            inputElementDesc, numElements, vertexShaderByteCode.data(), vertexShaderByteCode.size(), &shader->m_inputLayout);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create vertex layout")
        }

        return;
    }
}
