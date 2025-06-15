#include "DX12Renderer.hpp"

#include <d3dcompiler.h>
#include <dxgi1_4.h>

#include "Engine/Window/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/GraphicsError.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "ThirdParty/d3dx12/d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

template <typename T>
constexpr T AlignUp(T v, T a) { return (v + a - 1) & ~(a - 1); }

DX12Renderer::DX12Renderer(const RenderConfig& cfg): m_config(cfg)
{
}

DX12Renderer::~DX12Renderer()
{
    /// Make sure the queue is empty
    /// So we insert an fence signal, when it reach the signal , CPU knows that GPU executed all the Command List
    /// before the fence

    // wait for queue to become completely empty ( 2 seconds max )
    m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue) >> chk;
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent) >> chk;
    if (WaitForSingleObject(m_fenceEvent, 2000) == WAIT_FAILED)
    {
        GetLastError() >> chk;
    }
}

void DX12Renderer::Startup()
{
#ifdef ENGINE_DEBUG_RENDER
    // ① 打开 DX12 Debug Layer（在 Release 下会被编译器剔除）
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
    }
#endif

    // dxgi factory
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT                  factoryFlags = 0;
#ifdef ENGINE_DEBUG_RENDER
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory));
    // device
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateDXGIFactory2 failed")
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&m_device)) >> chk; // 默认适配器
    m_device.As(&m_device2); // Update ID3D12Device interface to ID3D12Device2 

    // command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask                 = 0;
        desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue));
    }

    // swap chain
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width                 = m_config.m_window->GetClientDimensions().x;
        swapChainDesc.Height                = m_config.m_window->GetClientDimensions().y;
        swapChainDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo                = FALSE;
        swapChainDesc.SampleDesc.Count      = 1;
        swapChainDesc.SampleDesc.Quality    = 0;
        swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount           = kBufferCount;
        swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags                 = 0;
        ComPtr<IDXGISwapChain1> swapChain1;
        dxgiFactory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            (HWND)m_config.m_window->GetWindowHandle(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        );
        swapChain1.As(&m_swapChain);
    }

    // render target view heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors             = kBufferCount;
        desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // This heap can only hold render target view
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_renderTargetViewHeap));
        // In order to index the descriptor heap, get the first or the second buffer, we need to know the size of each individual
        // descriptor which it depends on hardware
        m_renderTargetViewDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // we know how much we can move in the heap
    }

    // render target view descriptors and buffer references
    // We need to create the individual descriptor for each of those buffers
    // we also need to cache some reference to each of the buffers
    {
        // Descriptor handle, means the pointer to the descriptor heap that points to a specific descriptor
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < kBufferCount; ++i)
        {
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_renderTargetViewDescriptorSize);
        }
    }

    // command allocator
    // The memory that backs the command list object
    m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)); // need to set to Direct command list, submit 3D rendering commands

    // command list
    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    // we close it after we create it so it can be reset at top of draw loop
    m_commandList->Close(); // because the render loop expect the command list is in close state

    // fence
    m_fenceValue = 0; // we keep track this value in CPU side, when it reach we know the Command Lists before fence object is executed by GPU
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    // fence signalling event
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        GetLastError();
        throw std::exception("Failed to create fence event");
    }

    // Buffers
    m_vertexBuffer = CreateVertexBuffer(sizeof(Vertex_PCU), sizeof(Vertex_PCU));

    // Root Signature
    {
        // define empty root signature
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc; // always need a descriptor that how GPU will interpolate it
        // There are numParameters and array of root parameters this is the things that root signature exposing to the shader
        // You can combined flags, we need allow input layout
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        // serialize root signature
        // We need tp serialize them into a stream format that drivers will understand, like compiling a shader for hlsl
        ComPtr<ID3DBlob> signatureBlob, errorBlob;
        hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                auto errorBufferPtr = errorBlob->GetBufferPointer();
                ERROR_AND_DIE(static_cast<const char*>(errorBufferPtr))
            }
        }
        // Once we get that format drivers know, we create the root signature
        // Node mask is to set the different GPUs, signatureBlob is a hidden implementation for different vendors like NVIDIA
        // Thet have their own optimization and implementation.
        m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)) >> chk;
    }

    // Pipeline state object
    {
        // the way to describe pipeline state object is quite unique, the description of pipeline state object is a stream of bytes.
        // the typical way to make that stream is by creating a struct
        struct PipelineStateStream // static declaration of pso stream structure
        {
            // TODO: learn how windows create the amazing packing template
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE     RootSignature; // PSO need to be tell which root signature it will use
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT       InputLayout; // InputLayout for our vertices
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopology; // Triangle, lines...
            CD3DX12_PIPELINE_STATE_STREAM_VS                 VertexShader;
            CD3DX12_PIPELINE_STATE_STREAM_PS                 PixelShader;
            // this is not specify which render target we are going to using, it says any render target
            // that we bind has to have this format
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerState;
        } pipelineStateStream;

        D3D12_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
            {
                "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            }
        };

        // default shader
        m_defaultShader = CreateOrGetShader(m_config.m_defaultShader.c_str());

        // filling pso structure
        pipelineStateStream.RootSignature     = m_rootSignature.Get();
        pipelineStateStream.InputLayout       = {inputLayoutDesc, _countof(inputLayoutDesc)};
        pipelineStateStream.PrimitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VertexShader      = CD3DX12_SHADER_BYTECODE(m_defaultShader->m_vertexShaderBlob); // Blob is typeless
        pipelineStateStream.PixelShader       = CD3DX12_SHADER_BYTECODE(m_defaultShader->m_pixelShaderBlob);
        // Do not have C++ 20 designated initializer syntax, initialize by aggregate
        DXGI_FORMAT           rtFormats[]       = {DXGI_FORMAT_R8G8B8A8_UNORM};
        D3D12_RT_FORMAT_ARRAY rtFormatArray     = {};
        rtFormatArray.NumRenderTargets          = 1;
        rtFormatArray.RTFormats[0]              = rtFormats[0];
        pipelineStateStream.RenderTargetFormats = CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS(rtFormatArray);
        // Fill rasterization
        CD3DX12_RASTERIZER_DESC rastDesc(D3D12_DEFAULT);
        rastDesc.CullMode                   = D3D12_CULL_MODE_NONE; // TODO: TO Change to correct version
        pipelineStateStream.RasterizerState = rastDesc;

        // build the pipeline state object
        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
            sizeof(PipelineStateStream), &pipelineStateStream
        };
        m_device2->CreatePipelineState(&pipelineStateStreamDesc,IID_PPV_ARGS(&m_pipelineState)) >> chk;
    }

    // define scissor rect
    m_scissorRect = CD3DX12_RECT(0, 0,LONG_MAX, LONG_MAX);
    // define viewport
    m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)m_config.m_window->GetClientDimensions().x, (float)m_config.m_window->GetClientDimensions().y);


    /*m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex(); // Which one is the current one*/
    DebuggerPrintf("DX12Renderer StartUp — OK\n");
}


void DX12Renderer::Shutdown()
{
    POINTER_SAFE_DELETE(m_vertexBuffer);
}

void DX12Renderer::BeginFrame()
{
    // advance buffer
    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    // reset command list and allocator
    m_commandAllocator->Reset();
    // Every time you reset the command list, it gives you a different command allocator
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    // Prepare Buffer for presentation
    {
        // select current buffer to render to
        auto& backBuffer = m_backBuffers[m_currentBackBufferIndex];
        auto  barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        // PRESENT → RENDER_TARGET so we need to change the state to D3D12_RESOURCE_STATE_RENDER_TARGET
        m_commandList->ResourceBarrier(1, &barrier);
    }
}

void DX12Renderer::EndFrame()
{
    auto& backBuffer = m_backBuffers[m_currentBackBufferIndex];
    auto  barrier    = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    // We put data and buffers in render target and now to switch D3D12_RESOURCE_STATE_PRESENT for presenting
    m_commandList->ResourceBarrier(1, &barrier);

    // submit command list
    {
        // close command list
        m_commandList->Close();
        // submit command list to queue
        ID3D12CommandList* commandLists[] = {m_commandList.Get()};
        m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
        // we submit multiple command list together 
    }
    /// After we submit our command list we want to put a fence in there so we can get information about when it
    /// has complete.
    /// We only have one command allocator so we have to know when the command list is finished executing because
    /// that's when we can reuse our allocator

    // Insert fence to mark command list completion
    m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue);

    // present frame
    // The swap chain backbuffer is automatically placed in the D3D12_RESOURCE_STATE_PRESENT state after Present() ends.
    m_swapChain->Present(1, 0);

    // wait our command list / allocator to become free
    WaitForGPU();
}

void DX12Renderer::ClearScreen(const Rgba8& clr)
{
    float c[4];
    clr.GetAsFloats(c);

    // select current buffer to render to
    //auto& backBuffer = m_backBuffers[m_currentBackBufferIndex];
    // Clear the render target
    {
        // When you doing a operation on some resource, it has to be a certain state in order to do that operation,
        // and when you want to do a different operation it going to need to be in a different state and so you have
        // to create the transition between those states

        // Basically, you need to know what was the state it was previously in and what is the state that you wanted to now
        // transition buffer resource to render target state, so we need tp draw some thing so we need the render target state not
        // the present state

        // But we have change the state to D3D12_RESOURCE_STATE_RENDER_TARGET at the begin frame, no need for create barrier again.

        //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        //m_commandList->ResourceBarrier(1, &barrier);

        // clear buffer
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), (UINT)(m_currentBackBufferIndex), m_renderTargetViewDescriptorSize);
        m_commandList->ClearRenderTargetView(rtvHandle, c, 0, nullptr); // Issue the command clear the render target
    }
}

void DX12Renderer::BeginCamera(const Camera& cam)
{
    UNUSED(cam)
    
    /*m_curCamCB.WorldToCameraTransform  = cam.GetWorldToCameraTransform();
    m_curCamCB.CameraToRenderTransform = cam.GetCameraToRenderTransform();
    m_curCamCB.RenderToClipTransform   = cam.GetProjectionMatrix();
    // HLSL 在 register(b2) 读 CameraConstants，要上传到 descriptor index 2
    SetModelConstants();
    UploadToCB(m_cbs[m_drawCount * kCBPerDraw + 2], &m_curCamCB, sizeof(CameraConstants));
    // 视口/剪裁
    IntVec2      client = m_config.m_window->GetClientDimensions();
    const AABB2& vpN    = cam.GetNormalizedViewport();

    m_viewport.TopLeftX = vpN.m_mins.x * client.x;
    m_viewport.TopLeftY = (1.0f - vpN.m_maxs.y) * client.y;
    m_viewport.Width    = vpN.GetDimensions().x * client.x;
    m_viewport.Height   = vpN.GetDimensions().y * client.y;
    m_viewport.MinDepth = 0.f;
    m_viewport.MaxDepth = 1.f;

    m_scissorRect.left   = static_cast<LONG>(m_viewport.TopLeftX);
    m_scissorRect.top    = static_cast<LONG>(m_viewport.TopLeftY);
    m_scissorRect.right  = static_cast<LONG>(m_viewport.TopLeftX + m_viewport.Width);
    m_scissorRect.bottom = static_cast<LONG>(m_viewport.TopLeftY + m_viewport.Height);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);*/
}

void DX12Renderer::EndCamera(const Camera& cam)
{
    UNUSED(cam)
}

Shader* DX12Renderer::CreateShader(const char* name, const char* src, VertexType t)
{
    ShaderConfig cfg;
    cfg.m_name = name; // 你的 ShaderConfig 结构保持与 DX11 相同字段

    //--------------------------------------------------------------------
    // 1. 编译 VS/PS 字节码到 vector
    //--------------------------------------------------------------------
    std::vector<uint8_t> vsBytes;
    std::vector<uint8_t> psBytes;

    bool vsOk = CompileShaderToByteCode(vsBytes, name, src, cfg.m_vertexEntryPoint.c_str(), "vs_5_0");
    bool psOk = CompileShaderToByteCode(psBytes, name, src, cfg.m_pixelEntryPoint.c_str(), "ps_5_0");
    GUARANTEE_OR_DIE(vsOk && psOk, "DX12: shader compile failed");

    //--------------------------------------------------------------------
    // 2. 把 vector → ID3DBlob （以便 PSO 直接使用）
    //--------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;

    HRESULT hr = D3DCreateBlob(vsBytes.size(), &vsBlob);
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: D3DCreateBlob VS failed");
    memcpy(vsBlob->GetBufferPointer(), vsBytes.data(), vsBytes.size());

    hr = D3DCreateBlob(psBytes.size(), &psBlob);
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: D3DCreateBlob PS failed");
    memcpy(psBlob->GetBufferPointer(), psBytes.data(), psBytes.size());

    //--------------------------------------------------------------------
    // 3. 生成输入布局数组（与 VertexType 匹配）
    //--------------------------------------------------------------------
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
    if (t == VertexType::Vertex_PCU)
    {
        layout = {
            {
                "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            }
        };
    }
    else if (t == VertexType::Vertex_PCUTBN)
    {
        layout = {
            {
                "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            },
            {
                "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
                D3D12_APPEND_ALIGNED_ELEMENT,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
            }
        };
    }
    else
    {
        ERROR_AND_DIE("DX12Renderer::CreateShader - unhandled VertexType")
    }

    //--------------------------------------------------------------------
    // 4. 构造 Shader 对象并缓存
    //--------------------------------------------------------------------
    Shader* shader             = new Shader(cfg);
    shader->m_vertexShaderBlob = vsBlob.Detach();
    shader->m_pixelShaderBlob  = psBlob.Detach();
    shader->m_dx12InputLayout  = layout;
    m_shaderCache.push_back(shader);
    return shader;
}

Shader* DX12Renderer::CreateShader(const char* name, VertexType t)
{
    std::string shaderSource;
    std::string shaderNameWithExtension = "Data/Shaders/" + std::string(name) + ".hlsl";
    int         readResult              = FileReadToString(shaderSource, shaderNameWithExtension);
    if (readResult == 0)
    {
        ERROR_RECOVERABLE(Stringf( "Could not read shader \"%s\"", name ))
    }
    return CreateShader(name, shaderSource.c_str(), t);
}

Shader* DX12Renderer::CreateOrGetShader(const char* shaderName)
{
    for (auto& shader : m_shaderCache)
    {
        if (shader->GetName() == shaderName)
        {
            return shader;
        }
    }
    return CreateShader(shaderName);
}

bool DX12Renderer::CompileShaderToByteCode(std::vector<uint8_t>& out, const char* name, const char* src, const char* entry, const char* target)
{
    DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(ENGINE_DEBUG_RENDER)
    shaderFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(src, // pSrcData
                            strlen(src), // SrcDataSize
                            name, // pSourceName (用于报错)
                            nullptr, nullptr, // Defines / Include
                            entry, target, // 入口 & 目标 (vs_6_0 / ps_6_0 …)
                            shaderFlags, 0,
                            &shaderBlob, &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob) // 打印 HLSL 错误
            DebuggerPrintf("%s\n", (char*)errorBlob->GetBufferPointer());

        return false;
    }

    // 拷贝到输出 vector
    out.resize(shaderBlob->GetBufferSize());
    memcpy(out.data(),
           shaderBlob->GetBufferPointer(),
           shaderBlob->GetBufferSize());

    return true;
}


Texture* DX12Renderer::CreateTextureFromImage(Image& image)
{
    auto newTexture          = new Texture();
    newTexture->m_dimensions = image.GetDimensions();


    return newTexture;
}

Texture* DX12Renderer::CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData)
{
    return nullptr;
}

Texture* DX12Renderer::CreateTextureFromFile(const char* imageFilePath)
{
    return nullptr;
}

Texture* DX12Renderer::GetTextureForFileName(const char* imageFilePath)
{
    return nullptr;
}

BitmapFont* DX12Renderer::CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture)
{
    return nullptr;
}

void DX12Renderer::BindShader(Shader* s)
{
    UNUSED(s)
}

VertexBuffer* DX12Renderer::CreateVertexBuffer(size_t size, unsigned stride)
{
    VertexBuffer* vb = new VertexBuffer(m_device.Get(), static_cast<unsigned>(size), stride);
    return vb;
}

IndexBuffer* DX12Renderer::CreateIndexBuffer(size_t size)
{
    //IndexBuffer* ib = new IndexBuffer(m_device, static_cast<unsigned>(size));
    //return ib;
    return nullptr;
}

ConstantBuffer* DX12Renderer::CreateConstantBuffer(size_t size)
{
    size_t                  aligned = AlignUp(size, (size_t)256);
    ConstantBuffer*         cb      = new ConstantBuffer(static_cast<unsigned>(aligned));
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(aligned);

    HRESULT hr = m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   IID_PPV_ARGS(&cb->m_dx12ConstantBuffer));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateConstantBuffer failed");
    return cb;
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, VertexBuffer* vbo, size_t off)
{
    if (sz > vbo->GetSize())
    {
        vbo->Resize((unsigned int)sz);
    }

    // copy array of vertex data to buffer
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      mappedVertexData = nullptr;
    // writing the memory of GPU which is connect via the PCIe bus, you mapped the address of CPU to memory space (virtual)
    // to the PCIe bus hardware that is going to allow you to transfer data over to the GPU memory
    vbo->m_dx12buffer->Map(0, &rng, reinterpret_cast<void**>(&mappedVertexData)) >> chk;
    memcpy(mappedVertexData + off, data, sz);
    vbo->m_dx12buffer->Unmap(0, nullptr);
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, IndexBuffer* ibo)
{
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    ibo->m_dx12buffer->Map(0, &rng, reinterpret_cast<void**>(&dst));
    memcpy(dst, data, sz);
    ibo->m_dx12buffer->Unmap(0, nullptr);
}

void DX12Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
    D3D12_VERTEX_BUFFER_VIEW vbv = *vbo->m_vertexBufferView;
    m_commandList->IASetVertexBuffers(0, 1, &vbv);
}

void DX12Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = ibo->m_dx12buffer->GetGPUVirtualAddress();
    ibv.SizeInBytes    = static_cast<UINT>(ibo->GetSize());
    ibv.Format         = DXGI_FORMAT_R32_UINT;
    m_commandList->IASetIndexBuffer(&ibv);
}

void DX12Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
}

void DX12Renderer::BindTexture(Texture* tex, int slot)
{
    UNUSED(slot)
    UNUSED(tex)
}

void DX12Renderer::SetModelConstants(const Mat44& model, const Rgba8& tint)
{
    m_curModelCB.ModelToWorldTransform = model;
    tint.GetAsFloats(m_curModelCB.ModelColor);
    UploadToCB(m_cbs[m_drawCount * kCBPerDraw + 3], &m_curModelCB, sizeof(ModelConstants));
}

void DX12Renderer::SetDirectionalLightConstants(const DirectionalLightConstants&)
{
}

void DX12Renderer::SetLightConstants(Vec3 lightPos, float ambient, const Mat44& view, const Mat44& proj)
{
    UNUSED(lightPos)
    UNUSED(ambient)
    UNUSED(view)
    UNUSED(proj)
}

void DX12Renderer::SetCustomConstantBuffer(ConstantBuffer*& cbo, void* data, size_t sz, int slot)
{
    UploadToCB(m_cbs[m_drawCount * kCBPerDraw + slot], data, sz);
}

void DX12Renderer::SetBlendMode(BlendMode)
{
}

void DX12Renderer::SetRasterizerMode(RasterizerMode)
{
}

void DX12Renderer::SetDepthMode(DepthMode)
{
}

void DX12Renderer::SetSamplerMode(SamplerMode)
{
}

void DX12Renderer::DrawVertexArray(int n, const Vertex_PCU* v)
{
    if (n <= 0 || v == nullptr) return;
    size_t sz = sizeof(Vertex_PCU) * (size_t)n;
    CopyCPUToGPU(v, sz, m_vertexBuffer);
    //m_tmpVB.push_back(m_vertexBuffer);
    DrawVertexBuffer(m_vertexBuffer, n);
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v)
{
    DrawVertexArray((int)v.size(), v.data());
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v)
{
    UNUSED(v)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx)
{
    if (v.empty() || idx.empty()) return;
    VertexBuffer* vb = CreateVertexBuffer(v.size() * sizeof(Vertex_PCU), sizeof(Vertex_PCU));
    CopyCPUToGPU(v.data(), v.size() * sizeof(Vertex_PCU), vb);
    IndexBuffer* ib = CreateIndexBuffer(idx.size() * sizeof(unsigned));
    CopyCPUToGPU(idx.data(), idx.size() * sizeof(unsigned), ib);
    m_tmpVB.push_back(vb);
    m_tmpIB.push_back(ib);
    DrawVertexIndexed(vb, ib, (int)idx.size());
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx)
{
    UNUSED(v)
    UNUSED(idx)
}

void DX12Renderer::DrawVertexBuffer(VertexBuffer* vbo, int count, int offset)
{
    // Set pipeline state
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    // Config the Input assembler
    // you may this we have set the primitive in PSO desc. In PSO we saying that we are rendering triangle
    // this sets it up exactly what kind of triangle data (strip? adjacency? etc...)
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, vbo->m_vertexBufferView);
    // Configure Rasterize stuff
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    // Bind render target
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), (UINT)(m_currentBackBufferIndex), m_renderTargetViewDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle,TRUE, nullptr);
    // draw the geometry
    m_commandList->DrawInstanced(count, 1, 0, 0);
}

void DX12Renderer::DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, int idxCount, int idxOffset)
{
    /*if (m_drawCount >= kMaxDraws) return;

    // 上传 Model CB（如有）或 Camera CB
    UploadToCB(m_cbs[m_drawCount * kCBPerDraw + 3], &m_curModelCB, sizeof(ModelConstants));
    // —— 1) RootSignature + PSO ——
    m_commandList->SetGraphicsRootSignature(m_rootSignature);
    m_commandList->SetPipelineState(m_curPSO ? m_curPSO : m_defaultPSO);
    // —— 2) DescriptorHeap & CBV Table ——
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbStart(
        m_cbvHeap->GetGPUDescriptorHandleForHeapStart(),
        m_drawCount * kCBPerDraw,
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    );
    ID3D12DescriptorHeap* heaps[] = {m_cbvHeap};
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, cbStart);
    // IA + DrawIndexed
    // 先绑定 VB / IB，再设置拓扑
    BindVertexBuffer(vbo);
    BindIndexBuffer(ibo);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->DrawIndexedInstanced(idxCount, 1, idxOffset, 0, 0);
    ++m_drawCount;*/
}

void DX12Renderer::WaitForGPU()
{
    // wait our command list / allocator to become free
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    if (::WaitForSingleObject(m_fenceEvent, INFINITE) == WAIT_FAILED)
    {
        GetLastError();
    }
}

uint64_t DX12Renderer::CalcPSOHash() const
{
    return 0;
}

void DX12Renderer::ApplyStatesIfNeeded()
{
}

void DX12Renderer::UploadToCB(ConstantBuffer* cb, const void* data, size_t size)
{
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    cb->m_dx12ConstantBuffer->Map(0, &rng, reinterpret_cast<void**>(&dst));
    memcpy(dst, data, size);
    cb->m_dx12ConstantBuffer->Unmap(0, nullptr);
}
