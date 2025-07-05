#include "DX12Renderer.hpp"

#include <d3dcompiler.h>
#include <dxgi1_4.h>

#include "Engine/Window/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/GraphicsError.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "ThirdParty/d3dx12/d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

DX12Renderer::DX12Renderer(const RenderConfig& cfg) : m_config(cfg)
{
}

DX12Renderer::~DX12Renderer()
{
}

void DX12Renderer::Startup()
{
    /// TODO: Fix the meory leak caused by debug layer
#ifdef ENGINE_DEBUG_RENDER
    // Open DX12 Debug Layer (will be eliminated by the compiler under Release)
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
        /// Command queue for Main Renderer Command Allocator
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask                 = 0;
        desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue));

        /// Command queue for Upload Renderer Command Allocator
        D3D12_COMMAND_QUEUE_DESC copyDesc = {};
        copyDesc.Type                     = D3D12_COMMAND_LIST_TYPE_COPY;
        copyDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        m_device->CreateCommandQueue(&copyDesc, IID_PPV_ARGS(&m_copyCommandQueue)) >> chk;
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
        swapChainDesc.BufferCount           = kBackBufferCount;
        swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags                 = 0;
        ComPtr<IDXGISwapChain1> swapChain1;
        dxgiFactory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            static_cast<HWND>(m_config.m_window->GetWindowHandle()),
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
        desc.NumDescriptors             = kBackBufferCount;
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
        // Descriptor handle, means the pointer to the descriptor heap that points to a specific descriptor (GPU side)
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < kBackBufferCount; ++i)
        {
            m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
            m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_renderTargetViewDescriptorSize);
        }
    }

    // depth buffer
    {
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const CD3DX12_RESOURCE_DESC   resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            m_config.m_window->GetClientDimensions().x,
            m_config.m_window->GetClientDimensions().y,
            1, 0, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        );
        // When you create texture, you can give it an optimized clear value
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format            = DXGI_FORMAT_D32_FLOAT;
        // We want to clear the depth stencil to one, one represent the far plane
        clearValue.DepthStencil = D3D12_DEPTH_STENCIL_VALUE{1.0f, 0};
        // We create commit resource
        m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencilBuffer)) >> chk;
    }

    // depth buffer view
    {
        // view also need a heap
        //ComPtr<ID3D12DescriptorHeap> depthStencilViewDescriptorHeap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors             = 1;
        m_device->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&m_depthStencilViewDescriptorHeap)) >> chk;

        // Depth Stencil View and handle
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{m_depthStencilViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart()};
        m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, dsvHandle);

        // We need to bind it to the pipeline in order to use it, but before that, we have to tell the pipeline state
        // object there is a depth stencil vew bound
        m_constantBuffers.resize(14);
    }


    // command allocator
    {
        /// Main Renderer Command Allocator
        /// The Command Allocator that execute main drawing and rendering

        // The memory that backs the command list object
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)) >> chk;; // need to set to Direct command list, submit 3D rendering commands

        // command list
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)) >> chk;
        // we close it after we create it so it can be reset at top of draw loop
        m_commandList->Close(); // because the render loop expect the command list is in close state

        /// Upload Command Allocator
        /// The Command Allocator that execute resource upload that need write the resource when the Main Renderer Command Allocator
        /// was not closing.

        // We create the COPY type allocator which enable Parallel execution
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,IID_PPV_ARGS(&m_uploadCommandAllocator)) >> chk;
        // copy command list
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_uploadCommandAllocator.Get(), nullptr,IID_PPV_ARGS(&m_uploadCommandList)) >> chk;
        // Close in the initial state, and reset when the upload is actually recorded
        m_uploadCommandList->Close();
    }
    // fence
    {
        m_fenceValue = 0; // we keep track this value in CPU side, when it reach we know the Command Lists before fence object is executed by GPU
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        // fence signalling event
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            GetLastError();
            throw std::exception("Failed to create fence event");
        }
    }


    // Buffers
    {
        // Vertex ring buffer
        for (uint32_t i = 0; i < kBackBufferCount; ++i)
        {
            m_frameVertexBuffer[i] = CreateVertexBuffer(kVertexRingSize, sizeof(Vertex_PCUTBN));
        }

        m_currentVertexBuffer = CreateVertexBuffer(sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));

        // Index ring buffer
        for (uint32_t i = 0; i < kBackBufferCount; ++i)
        {
            m_frameIndexBuffer[i] = CreateIndexBuffer(kIndexRingSize);
        }

        m_currentIndexBuffer = CreateIndexBuffer(sizeof(unsigned int));


        // Constant buffers heap that hold view descriptor (for 14 constant buffers)
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors             = kMaxConstantBufferSlot + (kMaxDescriptorSetsPerFrame * kMaxShaderSourceViewSlot); // Need additional space for uploading binding texture
        heapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&m_frameHeap));

        // Allocate and write CBV descriptor to the Heap
        UINT                          incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_frameHeap->GetCPUDescriptorHandleForHeapStart());
        m_constantBuffers.resize(14);
        for (int i = 0; i < (int)m_constantBuffers.size(); ++i)
        {
            // descriptor only when needed
            CD3DX12_CPU_DESCRIPTOR_HANDLE handleOffset(cpuHandle, i, incrementSize);
            // We Create Constant buffer but not specific their view
        }

        /// Conversion Buffer
        m_currentConversionBuffer = &m_conversionBuffers[m_currentBackBufferIndex];
        
    }

    /// Manager heaps
    {
        // Manager heap for ShaderSourceViewManagerHeap
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors             = kMaxTextureCached;
        heapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Do not set shader visible or else it will become read only
        m_device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&m_shaderSourceViewManagerHeap)) >> chk;
    }

    // Default Texture
    {
        Image defaultImage(IntVec2(2, 2), Rgba8::WHITE); // Default 2x2 texture
        m_defaultTexture = CreateTextureFromImage(defaultImage);
        m_defaultTexture->m_dx12Texture->SetName(L"m_defaultTexture");
    }

    // Root Signature
    {
        /// To expose things to our shader we need put them in the root signature, think root signature as a
        /// list of things that are exposed to shaders, there are 3 kings of things you can put in root signature
        /// 1. Put constants directly ( 0 level of indirection ) into there (floats or other values) root signature
        /// can put total 64 DWORD (64 floats)
        /// 2. Put Root descriptors ( 1 level of indirection ) that will point to some resource (can be constant
        /// buffer, texture, can be whatever) 2 DWORD, total 32 Descriptors
        /// 3. Put Descriptor tables ( 2 level of indirection ), pointer to the table that has descriptors,
        /// on of those descriptors will give your buffer where you value lives. 1 DWORD each.
        /// @link https://learn.microsoft.com/en-us/windows/win32/direct3d12/example-root-signatures

        // define root signature with transformation matrix of 16 32-bit floats used by the vertex shader
        CD3DX12_ROOT_PARAMETER rootParameters[2] = {};
        // Divide by 4 to calculate how many DWORD, D3D12_SHADER_VISIBILITY_VERTEX is what shader can see the constants
        //rootParameters[0].InitAsConstants(sizeof(CameraConstants) / 4, 2, 0, D3D12_SHADER_VISIBILITY_VERTEX);

        // define the first descriptor table Descriptor Range that holds constant buffer from b0 to b13
        CD3DX12_DESCRIPTOR_RANGE cbvDescriptorRange;
        cbvDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, kMaxConstantBufferSlot, /*BaseShaderRegister=*/0, /*RegisterSpace=*/0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

        // define the second descriptor table Descriptor Range that holds shader resource view from t0 to t127
        CD3DX12_DESCRIPTOR_RANGE shaderResourceDescriptorRange;
        shaderResourceDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, kMaxShaderSourceViewSlot, 0, 0,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

        // We set the second parameter of root signature to out cBuffer Descriptor Table
        rootParameters[0].InitAsDescriptorTable(1, &cbvDescriptorRange, D3D12_SHADER_VISIBILITY_ALL); // All means it is visible in vertex/pixel/compute shader, set as needed
        rootParameters[1].InitAsDescriptorTable(1, &shaderResourceDescriptorRange, D3D12_SHADER_VISIBILITY_ALL);
        // define root signature with transformation matrix
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc; // always need a descriptor that how GPU will interpolate it
        // Define static sampler
        CD3DX12_STATIC_SAMPLER_DESC staticSamplerDesc = {0, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
        // There are numParameters and array of root parameters this is the things that root signature exposing to the shader
        // You can combined flags, we need allow input layout
        rootSignatureDesc.Init((UINT)std::size(rootParameters), rootParameters, 1, &staticSamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
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
        // default shader
        m_defaultShader                   = CreateOrGetShader(m_config.m_defaultShader.c_str());
        m_psoTemplate.defaultVertexShader = m_defaultShader->m_vertexShaderBlob;
        m_psoTemplate.defaultPixelShader  = m_defaultShader->m_pixelShaderBlob;

        // filling pso structure
        m_psoTemplate.rootSignature      = m_rootSignature;
        m_psoTemplate.depthStencilFormat = DXGI_FORMAT_D32_FLOAT; // need to be same when we create the resource
        m_psoTemplate.renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_psoTemplate.primitiveTopology  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        m_psoTemplate.inputLayout[0]     = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayout[1]     = {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayout[2]     = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayout[3]     = {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayout[4]     = {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayout[5]     = {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        m_psoTemplate.inputLayoutCount   = _countof(m_psoTemplate.inputLayout);

        m_currentPipelineStateObject = CreatePipelineStateForRenderState(m_pendingRenderState);
    }

    // define scissor rect
    m_scissorRect = CD3DX12_RECT(0, 0,LONG_MAX, LONG_MAX);
    // define viewport
    m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(m_config.m_window->GetClientDimensions().x), static_cast<float>(m_config.m_window->GetClientDimensions().y));

    /*m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex(); // Which one is the current one*/
    DebuggerPrintf("DX12Renderer StartUp - OK\n");
}


void DX12Renderer::Shutdown()
{
#ifdef ENGINE_DEBUG_RENDER
    ComPtr<ID3D12DebugDevice> debugDevice;
    if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&debugDevice))))
    {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
    }
#endif

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

    if (m_fenceEvent)
    {
        // Close the fence event handle
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    for (IndexBuffer* indexBuffer : m_frameIndexBuffer)
    {
        POINTER_SAFE_DELETE(indexBuffer)
    }

    m_currentIndexBuffer = nullptr;

    for (VertexBuffer* vertexBuffer : m_frameVertexBuffer)
    {
        POINTER_SAFE_DELETE(vertexBuffer)
    }

    m_currentVertexBuffer = nullptr;

    for (ConstantBuffer* constant_buffer : m_constantBuffers)
    {
        POINTER_SAFE_DELETE(constant_buffer)
    }

    for (Shader* shader : m_shaderCache)
    {
        POINTER_SAFE_DELETE(shader)
    }


    for (Texture* texture : m_textureCache)
    {
        POINTER_SAFE_DELETE(texture)
    }

    POINTER_SAFE_DELETE(m_defaultTexture)
    m_currentShader = nullptr;
    m_defaultShader = nullptr;

#ifdef ENGINE_DEBUG_RENDER
    debugDevice.Reset();
#endif
}

void DX12Renderer::BeginFrame()
{
    // Reset the Description Set
    m_currentDescriptorSet = 0;
    // Initialize the first descriptor set
    if (m_descriptorSets.empty())
    {
        m_descriptorSets.resize(kMaxDescriptorSetsPerFrame);
        for (UINT i = 0; i < kMaxDescriptorSetsPerFrame; ++i)
        {
            m_descriptorSets[i].baseIndex = kMaxConstantBufferSlot + (i * kMaxShaderSourceViewSlot);
            // Clear texture binding
            memset((void*)m_descriptorSets[i].boundTextures, 0, sizeof(m_descriptorSets[i].boundTextures));
        }
    }

    // Clear texture bindings for all descriptor sets of the current frame
    for (auto& set : m_descriptorSets)
    {
        memset((void*)set.boundTextures, 0, sizeof(set.boundTextures));
    }

    // advance buffer
    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    // get current vertex buffer ring's vertex
    m_currentVertexBuffer = m_frameVertexBuffer[m_currentBackBufferIndex];
    m_currentVertexBuffer->ResetCursor();

    // Set up the conversion buffer for the current frame
    m_currentConversionBuffer = &m_conversionBuffers[m_currentBackBufferIndex];
    m_currentConversionBuffer->Reset();

    // get current index buffer ring's vertex
    m_currentIndexBuffer = m_frameIndexBuffer[m_currentBackBufferIndex];
    m_currentIndexBuffer->ResetCursor();

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

    // Set pipeline state
    m_commandList->SetPipelineState(m_currentPipelineStateObject.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    UINT incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Prepare for Constant buffer
    m_commandList->SetDescriptorHeaps(1, m_frameHeap.GetAddressOf());
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_frameHeap->GetGPUDescriptorHandleForHeapStart();
    m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleShaderResource(m_frameHeap->GetGPUDescriptorHandleForHeapStart(), kMaxConstantBufferSlot, incrementSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, gpuHandleShaderResource);

    BindTexture(nullptr);

    // Config the Input assembler
    // you may this we have set the primitive in PSO desc. In PSO we saying that we are rendering triangle
    // this sets it up exactly what kind of triangle data (strip? adjacency? etc...)
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
        m_commandList->Close() >> chk;
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
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), (UINT)m_currentBackBufferIndex, m_renderTargetViewDescriptorSize);
        m_commandList->ClearRenderTargetView(rtvHandle, c, 0, nullptr); // Issue the command clear the render target

        // Bind render target and Depth stencil buffer
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{m_depthStencilViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart()};
        // clear the depth buffer
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &rtvHandle,TRUE, &dsvHandle);
    }
}

void DX12Renderer::BeginCamera(const Camera& cam)
{
    // Configure Rasterize stuff
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // bind the Camera buffer
    CameraConstants cameraConstant         = {};
    cameraConstant.RenderToClipTransform   = cam.GetProjectionMatrix();
    cameraConstant.CameraToRenderTransform = cam.GetCameraToRenderTransform();
    cameraConstant.WorldToCameraTransform  = cam.GetWorldToCameraTransform();

    ConstantBuffer* cameraBuffer = CreateConstantBuffer(sizeof CameraConstants);
    BindConstantBuffer(2, cameraBuffer);
    UploadToCB(cameraBuffer, &cameraConstant, sizeof(CameraConstants));

    SetModelConstants();

    // TODO: Test only! need revert if the constant buffer test failed
    //m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(CameraConstants) / 4, &cameraConstant, 0);
}

void DX12Renderer::EndCamera(const Camera& cam)
{
    UNUSED(cam)
}

BitmapFont* DX12Renderer::CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension)
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

Shader* DX12Renderer::CreateShader(const char* name, const char* src, VertexType t)
{
    ShaderConfig cfg;
    cfg.m_name = name;

    // Compile VS/PS bytecode to vector
    std::vector<uint8_t> vsBytes;
    std::vector<uint8_t> psBytes;

    bool vsOk = CompileShaderToByteCode(vsBytes, name, src, cfg.m_vertexEntryPoint.c_str(), "vs_5_0");
    bool psOk = CompileShaderToByteCode(psBytes, name, src, cfg.m_pixelEntryPoint.c_str(), "ps_5_0");
    GUARANTEE_OR_DIE(vsOk && psOk, "DX12: shader compile failed")

    // Convert vector to ID3DBlob (for direct use by PSO)
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;

    HRESULT hr = D3DCreateBlob(vsBytes.size(), &vsBlob);
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: D3DCreateBlob VS failed");
    memcpy(vsBlob->GetBufferPointer(), vsBytes.data(), vsBytes.size());

    hr = D3DCreateBlob(psBytes.size(), &psBlob);
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: D3DCreateBlob PS failed");
    memcpy(psBlob->GetBufferPointer(), psBytes.data(), psBytes.size());

    // Generate input layout array (matching VertexType)
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

    // Constructing and caching Shader objects
    auto shader                = new Shader(cfg);
    shader->m_vertexShaderBlob = vsBlob.Detach();
    shader->m_pixelShaderBlob  = psBlob.Detach();
    shader->m_dx12InputLayout  = layout;
    m_shaderCache.push_back(shader);
    return shader;
}

Shader* DX12Renderer::CreateShader(const char* name, VertexType t)
{
    std::string shaderSource;
    std::string shaderNameWithExtension = ".enigma/data/Shaders/" + std::string(name) + ".hlsl";
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

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(src, // pSrcData
                            strlen(src), // SrcDataSize
                            name, // pSourceName (用于报错)
                            nullptr, nullptr, // Defines / Include
                            entry, target, // 入口 & 目标 (vs_6_0 / ps_6_0 …)
                            shaderFlags, 0,
                            &shaderBlob, &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
            DebuggerPrintf("%s\n", static_cast<char*>(errorBlob->GetBufferPointer()));

        return false;
    }

    out.resize(shaderBlob->GetBufferSize());
    memcpy(out.data(),
           shaderBlob->GetBufferPointer(),
           shaderBlob->GetBufferSize());

    return true;
}

Texture* DX12Renderer::CreateTextureFromImage(Image& image)
{
    // Create a Texture object and save basic information
    auto newTexture          = new Texture();
    newTexture->m_dimensions = image.GetDimensions();
    newTexture->m_name       = image.GetImageFilePath();

    // Create GPU-side Texture in the DEFAULT heap (initial state is COPY_DEST)
    CD3DX12_RESOURCE_DESC   textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, image.GetDimensions().x, image.GetDimensions().y,/*arraySize=*/1, /*mipLevels=*/1);
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    m_device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&newTexture->m_dx12Texture)) >> chk;

    // Calculate UploadHeap size and create UPLOAD heap
    UINT64 uploadSize = 0;
    m_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

    CD3DX12_RESOURCE_DESC   bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&newTexture->m_textureBufferUploadHeap)) >> chk;

    // Copy pixel data to UPLOAD heap
    {
        uint8_t*      pData = nullptr;
        CD3DX12_RANGE readRange{0, 0};
        newTexture->m_textureBufferUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&pData));
        memcpy(pData, image.GetRawData(), uploadSize);
        newTexture->m_textureBufferUploadHeap->Unmap(0, nullptr);
    }

    // Synchronize the last upload to prevent reset conflicts
    m_copyCommandQueue->Signal(m_fence.Get(), ++m_fenceValue);
    WaitForGPU();

    // Reset and record the Upload list
    m_uploadCommandAllocator->Reset() >> chk;
    m_uploadCommandList->Reset(m_uploadCommandAllocator.Get(), nullptr) >> chk;

    D3D12_SUBRESOURCE_DATA subData = {};
    subData.pData                  = image.GetRawData();
    subData.RowPitch               = (int)image.GetDimensions().x * 4;
    subData.SlicePitch             = subData.RowPitch * image.GetDimensions().y;

    UpdateSubresources(m_uploadCommandList.Get(), newTexture->m_dx12Texture, newTexture->m_textureBufferUploadHeap,/*firstSubresource=*/0, /*numRows=*/0, /*numSlices=*/1, &subData);

    // Submit the Upload list and wait for completion
    m_uploadCommandList->Close() >> chk;
    ID3D12CommandList* lists[] = {m_uploadCommandList.Get()};
    m_copyCommandQueue->ExecuteCommandLists(_countof(lists), lists);
    m_copyCommandQueue->Signal(m_fence.Get(), ++m_fenceValue);
    WaitForGPU();

    // Create an SRV for the texture on the manager heap and cache the CPU handle
    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    UINT srvIndex       = Texture::IncrementInternalID();

    // Create SRV on CPU-only heap (managerHeap)
    CD3DX12_CPU_DESCRIPTOR_HANDLE   cpuHandle(m_shaderSourceViewManagerHeap->GetCPUDescriptorHandleForHeapStart(), (INT)srvIndex, descriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip       = 0;
    srvDesc.Texture2D.MipLevels             = 1;
    m_device->CreateShaderResourceView(newTexture->m_dx12Texture, &srvDesc, cpuHandle);

    newTexture->m_cpuShaderSourceViewHandle = cpuHandle;
    return newTexture;
}

Texture* DX12Renderer::CreateOrGetTexture(const char* imageFilePath)
{
    // See if we already have this texture previously loaded
    Texture* existingTexture = GetTextureForFileName(imageFilePath); // You need to write this
    if (existingTexture)
    {
        return existingTexture;
    }
    // Never seen this texture before!  Let's load it.
    Texture* newTexture = CreateTextureFromFile(imageFilePath);
    m_textureCache.push_back(newTexture);
    return newTexture;
}

Texture* DX12Renderer::CreateTextureFromData(const char* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData)
{
    UNUSED(name)
    UNUSED(bytesPerTexel)
    UNUSED(texelData)
    UNUSED(dimensions)
    return nullptr;
}

Texture* DX12Renderer::CreateTextureFromFile(const char* imageFilePath)
{
    Image*   image   = CreateImageFromFile(imageFilePath);
    Texture* texture = CreateTextureFromImage(*image);
    delete image;
    return texture;
}

Texture* DX12Renderer::GetTextureForFileName(const char* imageFilePath)
{
    for (Texture*& m_loaded_texture : m_textureCache)
    {
        if (m_loaded_texture && m_loaded_texture->GetImageFilePath() == imageFilePath)
        {
            return m_loaded_texture;
        }
    }
    return nullptr;
}

BitmapFont* DX12Renderer::CreateBitmapFont(const char* bitmapFontFilePathWithNoExtension, Texture& fontTexture)
{
    auto bitmapFont = new BitmapFont(bitmapFontFilePathWithNoExtension, fontTexture);
    m_loadedFonts.push_back(bitmapFont);
    return bitmapFont;
}

void DX12Renderer::BindShader(Shader* s)
{
    // If nullptr is passed, the default shader is used.
    Shader* targetShader = (s != nullptr) ? s : m_defaultShader;

    // Update the current shader and pending application status
    m_currentShader             = targetShader;
    m_pendingRenderState.shader = targetShader;

    // @Note: In DX12, we cannot "bind" shaders immediately
    // Shaders will be applied via PSO the next time you draw
}

VertexBuffer* DX12Renderer::CreateVertexBuffer(size_t size, unsigned stride)
{
    auto vb = new VertexBuffer(m_device.Get(), static_cast<unsigned>(size), stride);
    return vb;
}

IndexBuffer* DX12Renderer::CreateIndexBuffer(size_t size)
{
    auto ib = new IndexBuffer(m_device.Get(), static_cast<unsigned>(size));
    return ib;
}

ConstantBuffer* DX12Renderer::CreateConstantBuffer(size_t size)
{
    size_t                  aligned = AlignUp(size, static_cast<size_t>(256));
    auto                    cb      = new ConstantBuffer(static_cast<unsigned>(aligned));
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(aligned);

    m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,IID_PPV_ARGS(&cb->m_dx12ConstantBuffer)) >> chk;

    /// We move the creation of constant buffer view inside the `BindConstantBuffer`
    return cb;
}

void DX12Renderer::CopyCPUToGPU(const Vertex_PCU* data, size_t size, VertexBuffer* v, size_t offset)
{
    /// Due to DX12 limitation of mixture drawing, we need convert data into Vertex_PCUTBN
    size_t numOfVertices = size / sizeof(Vertex_PCU);

    // Allocate space from the conversion buffer
    Vertex_PCUTBN* convertedVerts = m_currentConversionBuffer->Allocate(numOfVertices);
    // Perform efficient conversion
    ConvertPCUArrayToPCUTBN(data, convertedVerts, numOfVertices);
    CopyCPUToGPU(convertedVerts, numOfVertices * sizeof(Vertex_PCUTBN), v, offset);
}

void DX12Renderer::CopyCPUToGPU(const Vertex_PCUTBN* data, size_t size, VertexBuffer* v, size_t offset)
{
    CopyCPUToGPU((void*)data, size, v, offset);
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, VertexBuffer* vbo, size_t off)
{
    if (sz > vbo->GetSize())
    {
        vbo->Resize(static_cast<unsigned int>(sz));
    }

    // copy array of vertex data to buffer
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      mappedVertexData = nullptr;
    // writing the memory of GPU, which is connect via the PCIe bus, you mapped the address of CPU to memory space (virtual)
    // to the PCIe bus hardware that is going to allow you to transfer data over to the GPU memory
    vbo->m_dx12buffer->Map(0, &rng, reinterpret_cast<void**>(&mappedVertexData)) >> chk;
    memcpy(mappedVertexData + off, data, sz);
    vbo->m_dx12buffer->Unmap(0, nullptr);
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, IndexBuffer* ibo)
{
    // @Professor Butler: He would put the reseize logic inside CopyCPUToGPU
    if (sz > ibo->GetSize())
    {
        ibo->Resize(static_cast<unsigned int>(sz));
    }

    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    ibo->m_dx12buffer->Map(0, &rng, reinterpret_cast<void**>(&dst)) >> chk;
    memcpy(dst, data, sz);
    ibo->m_dx12buffer->Unmap(0, nullptr);
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cb)
{
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    cb->m_dx12ConstantBuffer->Map(0, &rng, reinterpret_cast<void**>(&dst));
    memcpy(dst, data, size);
    cb->m_dx12ConstantBuffer->Unmap(0, nullptr);
}

void DX12Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
    m_commandList->IASetVertexBuffers(0, 1, &vbo->m_vertexBufferView);
}

void DX12Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
    m_commandList->IASetIndexBuffer(&ibo->m_indexBufferView);
}

void DX12Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
    if (cbo == nullptr) return;
    if (m_constantBuffers.empty()) return;
    if (m_constantBuffers[slot] == cbo) return; // Same Constant buffer, ignore

    POINTER_SAFE_DELETE(m_constantBuffers[slot])
    m_constantBuffers[slot] = cbo; // Assign to cached constant buffer vector

    // Create or Rewrite the Constant Buffer view
    UINT                          descriptionSize    = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE   cpuSideBaseAddress = m_frameHeap->GetCPUDescriptorHandleForHeapStart();
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(cpuSideBaseAddress, slot, descriptionSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
    desc.BufferLocation                  = cbo->m_dx12ConstantBuffer->GetGPUVirtualAddress();
    desc.SizeInBytes                     = (UINT)cbo->m_size;

    m_device->CreateConstantBufferView(&desc, handle); // Create the view based on current Address
    cbo->m_constantBufferView = desc; // Assign the description view to constant buffer
}

void DX12Renderer::BindTexture(Texture* tex, int slot)
{
    const Texture* bindTex = (tex == nullptr) ? m_defaultTexture : tex;

    // Get the current descriptor set
    DescriptorSet& currentSet = m_descriptorSets[m_currentDescriptorSet];

    // Calculate the target location: the base address of the current set + slot
    UINT dstIndex = currentSet.baseIndex + slot;
    UINT incSize  = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Copy descriptor
    CD3DX12_CPU_DESCRIPTOR_HANDLE dst(m_frameHeap->GetCPUDescriptorHandleForHeapStart(), (int)dstIndex, incSize);
    m_device->CopyDescriptorsSimple(1, dst, bindTex->m_cpuShaderSourceViewHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Record the bound texture
    currentSet.boundTextures[slot] = const_cast<Texture*>(bindTex);
}

void DX12Renderer::SetModelConstants(const Mat44& modelToWorldTransform, const Rgba8& tint)
{
    ModelConstants modelConstants        = {};
    modelConstants.ModelToWorldTransform = modelToWorldTransform;
    tint.GetAsFloats(modelConstants.ModelColor);
    ConstantBuffer* modelBuffer = CreateConstantBuffer(sizeof ModelConstants);
    BindConstantBuffer(3, modelBuffer);
    UploadToCB(modelBuffer, &modelConstants, sizeof(CameraConstants));
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
    UNUSED(cbo)
    UNUSED(slot)
    UNUSED(data)
    UNUSED(sz)
}

void DX12Renderer::SetBlendMode(BlendMode mode)
{
    m_currentBlendMode             = mode;
    m_pendingRenderState.blendMode = mode;
    // @note: Do not apply immediately, wait until drawing to apply
}

void DX12Renderer::SetRasterizerMode(RasterizerMode mode)
{
    m_currentRasterizerMode             = mode;
    m_pendingRenderState.rasterizerMode = mode;
}

void DX12Renderer::SetDepthMode(DepthMode mode)
{
    m_currentDepthMode             = mode;
    m_pendingRenderState.depthMode = mode;
}

void DX12Renderer::SetSamplerMode(SamplerMode mode)
{
    m_currentSamplerMode             = mode;
    m_pendingRenderState.samplerMode = mode;
}

void DX12Renderer::DrawVertexArray(int n, const Vertex_PCU* v)
{
    if (n <= 0 || v == nullptr) return;

    // Allocate space from the conversion buffer
    Vertex_PCUTBN* convertedVerts = m_currentConversionBuffer->Allocate(n);
    // Perform efficient conversion
    ConvertPCUArrayToPCUTBN(v, convertedVerts, n);

    // Allocate GPU buffer space
    if (!m_currentVertexBuffer->Allocate(convertedVerts, n * sizeof(Vertex_PCUTBN)))
    {
        // Buffer is full, need to flush
        // Error handling or automatic flush logic can be added here
        return;
    }

    // Directly call the internal drawing function
    DrawVertexBufferInternal(m_currentVertexBuffer, n);
}

void DX12Renderer::DrawVertexArray(int n, const Vertex_PCUTBN* v)
{
    if (n <= 0 || v == nullptr) return;

    size_t dataSize = n * sizeof(Vertex_PCUTBN);

    if (!m_currentVertexBuffer->Allocate(v, dataSize))
    {
        return;
    }

    DrawVertexBufferInternal(m_currentVertexBuffer, n);
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v)
{
    DrawVertexArray(static_cast<int>(v.size()), v.data());
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v)
{
    UNUSED(v)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx)
{
    if (v.empty() || idx.empty()) return;

    // Allocate space from the conversion buffer
    Vertex_PCUTBN* convertedVerts = m_currentConversionBuffer->Allocate(v.size());
    // Perform efficient conversion
    ConvertPCUArrayToPCUTBN(v.data(), convertedVerts, v.size());

    // Allocate vertex space in RingVertexBuffer
    if (!m_currentVertexBuffer->Allocate(convertedVerts, v.size() * sizeof(Vertex_PCUTBN)))
    {
        return;
    }

    // Allocate index space in RingIndexBuffer
    unsigned int indexDataSize = static_cast<unsigned int>(idx.size()) * sizeof(unsigned int);
    if (!m_currentIndexBuffer->Allocate(idx.data(), indexDataSize))
    {
        return;
    }

    // Call internal drawing function
    DrawVertexIndexedInternal(m_currentVertexBuffer, m_currentIndexBuffer, static_cast<unsigned int>(idx.size()));
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx)
{
    UNUSED(v)
    UNUSED(idx)
}

void DX12Renderer::DrawVertexBuffer(VertexBuffer* vbo, int count)
{
    if (!vbo || count <= 0) return;

    // Calculate the size of the data to be copied
    size_t dataSize = (size_t)count * vbo->GetStride();

    // Read data from the user's VertexBuffer and copy it to RingBuffer
    void* srcData = nullptr;
    if (vbo->m_cpuPtr != nullptr)
    {
        // Fast path: persistent mapping
        srcData = vbo->m_cpuPtr;
    }
    else
    {
        // Slow path: Map required
        D3D12_RANGE readRange = {0, dataSize};
        vbo->m_dx12buffer->Map(0, &readRange, &srcData) >> chk;
    }

    // Assign to the current RingBuffer
    if (!m_currentVertexBuffer->Allocate(srcData, dataSize))
    {
        if (vbo->m_cpuPtr == nullptr)
        {
            vbo->m_dx12buffer->Unmap(0, nullptr);
        }
        return;
    }

    // Unmap (if necessary)
    if (vbo->m_cpuPtr == nullptr)
    {
        vbo->m_dx12buffer->Unmap(0, nullptr);
    }

    // Call internal drawing function
    DrawVertexBufferInternal(m_currentVertexBuffer, count);
}

void DX12Renderer::DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount)
{
    BindVertexBuffer(vbo);
    BindIndexBuffer(ibo);
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void DX12Renderer::WaitForGPU()
{
    // wait our command list / allocator to become free
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    if (WaitForSingleObject(m_fenceEvent, INFINITE) == WAIT_FAILED)
    {
        GetLastError() >> chk;
    }
}

void DX12Renderer::CommitCurrentDescriptorSet()
{
    // Set the Root Descriptor Table to point to the current descriptor set
    UINT           incSize    = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    DescriptorSet& currentSet = m_descriptorSets[m_currentDescriptorSet];

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_frameHeap->GetGPUDescriptorHandleForHeapStart(), (int)currentSet.baseIndex, incSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);
}

void DX12Renderer::PrepareNextDescriptorSet()
{
    // Move to the next descriptor set
    m_currentDescriptorSet++;

    if (m_currentDescriptorSet >= kMaxDescriptorSetsPerFrame)
    {
        ERROR_AND_DIE("Exceeded maximum descriptor sets per frame")
    }

    // Copy the texture from the previous set to the new set (keeping state)
    if (m_currentDescriptorSet > 0)
    {
        DescriptorSet& prevSet = m_descriptorSets[m_currentDescriptorSet - 1];
        DescriptorSet& currSet = m_descriptorSets[m_currentDescriptorSet];

        UINT incSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Copy all bound textures
        for (int i = 0; i < (int)kMaxShaderSourceViewSlot; ++i)
        {
            if (prevSet.boundTextures[i])
            {
                CD3DX12_CPU_DESCRIPTOR_HANDLE dst(m_frameHeap->GetCPUDescriptorHandleForHeapStart(), (int)currSet.baseIndex + i, incSize);

                m_device->CopyDescriptorsSimple(1, dst, prevSet.boundTextures[i]->m_cpuShaderSourceViewHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                currSet.boundTextures[i] = prevSet.boundTextures[i];
            }
        }
    }
}

ID3D12PipelineState* DX12Renderer::GetOrCreatePipelineState(const RenderState& state)
{
    // Check the cache first
    auto it = m_pipelineStateCache.find(state);
    if (it != m_pipelineStateCache.end())
    {
        return it->second.Get();
    }
    // If not in cache, create a new PSO
    ComPtr<ID3D12PipelineState> newPSO = CreatePipelineStateForRenderState(state);
    m_pipelineStateCache[state]        = newPSO;
    return newPSO.Get();
}

ComPtr<ID3D12PipelineState> DX12Renderer::CreatePipelineStateForRenderState(const RenderState& state)
{
    // Create the corresponding PSO description according to the state

    /// the way to describe pipeline state object is quite unique, the description of pipeline state object is a stream of bytes.
    /// the typical way to make that stream is by creating a struct
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        RootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VertexShader;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    PixelShader;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DepthStencilViewFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerState;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendState;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencilState;
    } pipelineStateStream;

    // Set the fixed part (copy from template)
    pipelineStateStream.RootSignature     = m_psoTemplate.rootSignature.Get();
    pipelineStateStream.InputLayout       = {m_psoTemplate.inputLayout, m_psoTemplate.inputLayoutCount};
    pipelineStateStream.PrimitiveTopology = m_psoTemplate.primitiveTopology;
    // Set up a shader (either default or user-specified)
    Shader* targetShader             = state.shader ? state.shader : m_defaultShader;
    pipelineStateStream.VertexShader = CD3DX12_SHADER_BYTECODE(targetShader->m_vertexShaderBlob);
    pipelineStateStream.PixelShader  = CD3DX12_SHADER_BYTECODE(targetShader->m_pixelShaderBlob);
    // Set the render target format (fixed part)
    DXGI_FORMAT           rtFormats[]          = {m_psoTemplate.renderTargetFormat};
    D3D12_RT_FORMAT_ARRAY rtFormatArray        = {};
    rtFormatArray.NumRenderTargets             = 1;
    rtFormatArray.RTFormats[0]                 = rtFormats[0];
    pipelineStateStream.RenderTargetFormats    = CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS(rtFormatArray);
    pipelineStateStream.DepthStencilViewFormat = m_psoTemplate.depthStencilFormat;


    // Set the rasterization state according to state.rasterizerMode
    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    switch (state.rasterizerMode)
    {
    case RasterizerMode::SOLID_CULL_NONE:
        rasterizerDesc.FrontCounterClockwise = false;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    case RasterizerMode::SOLID_CULL_BACK:
        rasterizerDesc.FrontCounterClockwise = true;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        break;
    case RasterizerMode::WIREFRAME_CULL_BACK:
        rasterizerDesc.FrontCounterClockwise = true;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        break;
    case RasterizerMode::WIREFRAME_CULL_NONE:
        rasterizerDesc.FrontCounterClockwise = false;
        rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    default:
        ERROR_AND_DIE("Can not use COUNT as RasterizerMode settings")
    }
    pipelineStateStream.RasterizerState = rasterizerDesc;

    // Set the blending state according to state.blendMode
    CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
    switch (state.blendMode)
    {
    /// Use the alpha value of the source pixel to determine the degree of blending. The formula is: FinalColor = SrcColor * SrcAlpha + DestColor * (1 - SrcAlpha)
    case BlendMode::ALPHA:
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend  = D3D12_BLEND_SRC_ALPHA; // Multiply the source color by the source alpha
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // Multiply the destination color by (1-source Alpha)
        blendDesc.RenderTarget[0].BlendOp   = D3D12_BLEND_OP_ADD; // Add
        break;
    /// This mode makes the color brighter and is often used for light effects, flames, explosions, etc. The formula is: FinalColor = SrcColor * SrcAlpha + DestColor * 1
    case BlendMode::ADDITIVE:
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend  = D3D12_BLEND_SRC_ALPHA; // Multiply the source color by the source alpha
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; // Multiply the destination color by 1 (keep same)
        blendDesc.RenderTarget[0].BlendOp   = D3D12_BLEND_OP_ADD; // Add
        break;
    case BlendMode::OPAQUE:
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        break;
    default:
        ERROR_AND_DIE("Can not use COUNT as BlendMode settings")
    }
    pipelineStateStream.BlendState = blendDesc;

    // Set the depth state according to state.depthMode
    CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
    switch (state.depthMode)
    {
    // Disable depth testing completely
    case DepthMode::DISABLED:
        depthStencilDesc.DepthEnable = FALSE; // Do not write depth
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Not write depth
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS; // Always pass (even if disabled)
        break;
    // Enable depth test, but don't write, always pass
    case DepthMode::READ_ONLY_ALWAYS:
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS; // Always pass
        break;
    // Enable depth test, do not write, pass when less than or equal to
    case DepthMode::READ_ONLY_LESS_EQUAL:
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Not write depth
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Pass if less than or equal to
        break;
    // Standard depth test: read and write, pass if less than or equal
    case DepthMode::READ_WRITE_LESS_EQUAL:
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // Write Depth
        depthStencilDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Pass if less than or equal to
        break;
    default:
        ERROR_AND_DIE("Unhandled DepthMode in CreatePipelineStateForRenderState")
    }
    depthStencilDesc.StencilEnable        = FALSE;
    pipelineStateStream.DepthStencilState = depthStencilDesc;

    ComPtr<ID3D12PipelineState>      pso;
    D3D12_PIPELINE_STATE_STREAM_DESC desc = {sizeof(PipelineStateStream), &pipelineStateStream};

    // build the pipeline state object
    m_device2->CreatePipelineState(&desc, IID_PPV_ARGS(&pso)) >> chk;
    return pso;
}

void DX12Renderer::DrawVertexBufferInternal(VertexBuffer* vbo, int count)
{
    DescriptorSet& currentSet = m_descriptorSets[m_currentDescriptorSet];
    {
        bool stateChanged = !(currentSet.renderState == m_pendingRenderState);
        if (stateChanged)
        {
            PrepareNextDescriptorSet();
            DescriptorSet& newSet    = m_descriptorSets[m_currentDescriptorSet];
            newSet.renderState       = m_pendingRenderState;
            ID3D12PipelineState* pso = GetOrCreatePipelineState(m_pendingRenderState);
            m_commandList->SetPipelineState(pso);
            m_currentPipelineStateObject = pso;
        }
    }

    // Submit the current descriptor set
    CommitCurrentDescriptorSet();

    // Draw
    m_commandList->IASetVertexBuffers(0, 1, &vbo->m_vertexBufferView);
    m_commandList->DrawInstanced(count, 1, 0, 0);

    // Prepare the next descriptor set
    PrepareNextDescriptorSet();
}

void DX12Renderer::DrawVertexIndexedInternal(VertexBuffer* vbo, IndexBuffer* ibo, unsigned int indexCount)
{
    // Check and apply state changes
    DescriptorSet& currentSet = m_descriptorSets[m_currentDescriptorSet];
    {
        bool stateChanged = !(currentSet.renderState == m_pendingRenderState);
        if (stateChanged)
        {
            PrepareNextDescriptorSet();
            DescriptorSet& newSet    = m_descriptorSets[m_currentDescriptorSet];
            newSet.renderState       = m_pendingRenderState;
            ID3D12PipelineState* pso = GetOrCreatePipelineState(m_pendingRenderState);
            m_commandList->SetPipelineState(pso);
            m_currentPipelineStateObject = pso;
        }
    }

    // Submit the current descriptor set
    CommitCurrentDescriptorSet();

    // Draw
    m_commandList->IASetVertexBuffers(0, 1, &vbo->m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&ibo->m_indexBufferView);
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);

    // Prepare the next descriptor set
    PrepareNextDescriptorSet();
}

void DX12Renderer::UploadToCB(ConstantBuffer* cb, const void* data, size_t size)
{
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    cb->m_dx12ConstantBuffer->Map(0, &rng, reinterpret_cast<void**>(&dst));
    memcpy(dst, data, size);
    cb->m_dx12ConstantBuffer->Unmap(0, nullptr);
}
