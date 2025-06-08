#include "DX12Renderer.hpp"

#include <d3dcompiler.h>
#include <dxgi1_4.h>

#include "Engine/Window/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "ThirdParty/d3dx12/d3dx12.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

template <typename T>
constexpr T AlignUp(T v, T a) { return (v + a - 1) & ~(a - 1); }

DX12Renderer::DX12Renderer(const RenderConfig& cfg): m_cfg(cfg)
{
}

DX12Renderer::~DX12Renderer()
{
}

void DX12Renderer::Startup()
{
#ifdef ENGINE_DEBUG_RENDER
    // ① 打开 DX12 Debug Layer（在 Release 下会被编译器剔除）
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
    }
#endif

    //----------------------------------------------------------
    // ② 创建设备 & 命令队列
    //----------------------------------------------------------
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    UINT                                  factoryFlags = 0;
#ifdef ENGINE_DEBUG_RENDER
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateDXGIFactory2 failed")
    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&m_device)); // 默认适配器
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateDevice failed")
    // 命令队列（仅 1 条 graphics queue）
    D3D12_COMMAND_QUEUE_DESC qDesc{};
    qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr          = m_device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&m_cmdQueue));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateCommandQueue failed")

    //----------------------------------------------------------
    // ③ 创建交换链（Flip-Discard, 2-buffer）
    //----------------------------------------------------------

    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.BufferCount      = kBackBufferCount;
    scDesc.Width            = m_cfg.m_window->GetClientDimensions().x;
    scDesc.Height           = m_cfg.m_window->GetClientDimensions().y;
    scDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap1;
    hr = factory->CreateSwapChainForHwnd(
        m_cmdQueue, // queue to associate
        (HWND)m_cfg.m_window->GetWindowHandle(), // Win32 HWND
        &scDesc,
        nullptr, // fullscreen desc
        nullptr, // output restrict
        &swap1);
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateSwapChainForHwnd failed")
    factory->MakeWindowAssociation((HWND)m_cfg.m_window->GetWindowHandle(),DXGI_MWA_NO_ALT_ENTER); // 禁止 Alt+Enter 全屏
    IDXGISwapChain3* tmpSwapChain3 = nullptr;
    hr                             = swap1->QueryInterface(IID_PPV_ARGS(&tmpSwapChain3)); // Same as hr = swap1.As(&m_swapChain); 
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: cast to IDXGISwapChain3 failed");
    m_swapChain  = tmpSwapChain3;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    //----------------------------------------------------------
    // ④ RTV 描述符堆 & 后备缓冲区视图
    //----------------------------------------------------------

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = kBackBufferCount;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr                         = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create RTV Heap failed")
    m_rtvSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < kBackBufferCount; ++i)
    {
        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: GetBuffer failed")

        m_device->CreateRenderTargetView(m_backBuffers[i], nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvSize);
    }
    //----------------------------------------------------------
    // ⑤ 深度/模板缓冲 & DSV Heap
    //----------------------------------------------------------
    DXGI_FORMAT                depthFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr                     = m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create DSV Heap failed")

    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        depthFmt,
        scDesc.Width,
        scDesc.Height,
        1, 1); // arraySize, mipLevels
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clearVal{};
    clearVal.Format               = depthFmt;
    clearVal.DepthStencil.Depth   = 1.0f;
    clearVal.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&m_depthStencilBuf));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create depth buffer failed")

    m_device->CreateDepthStencilView(m_depthStencilBuf, nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    //----------------------------------------------------------
    // ⑥  命令分配器 / 列表
    //----------------------------------------------------------

    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAllocator));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateCommandAllocator failed")
    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_cmdAllocator,
        nullptr, // PSO = nullptr 先不绑定
        IID_PPV_ARGS(&m_cmdList));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateCommandList failed")
    m_cmdList->Close(); // main loop 先要求 Closed

    //----------------------------------------------------------
    // ⑦ 同步对象
    //----------------------------------------------------------

    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateFence failed");
    m_fenceVal   = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    //----------------------------------------------------------
    // ⑧ RootSignature (简化版: 1 个 CBV 表 + 1 个描述符表 for heap 1)
    //----------------------------------------------------------

    CD3DX12_DESCRIPTOR_RANGE cbv0;
    cbv0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE cbv1;
    cbv1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, kCBPerDraw, 1);
    CD3DX12_DESCRIPTOR_RANGE srv0;
    srv0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsDescriptorTable(1, &cbv0, D3D12_SHADER_VISIBILITY_ALL);
    params[1].InitAsDescriptorTable(1, &cbv1, D3D12_SHADER_VISIBILITY_ALL);
    params[2].InitAsDescriptorTable(1, &srv0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC samp(
        /*shaderReg*/0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                     D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                     D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                     D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc(
        _countof(params), params,
        1, &samp,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,&sigBlob, &errBlob);
    if (FAILED(hr) && errBlob) DebuggerPrintf("[RootSig] %s\n", (char*)errBlob->GetBufferPointer());
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "SerializeRootSignature failed")

    hr = m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                       IID_PPV_ARGS(&m_rootSignature));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "CreateRootSignature failed")

    //----------------------------------------------------------
    // ⑫ 默认资源（只执行一次）
    //----------------------------------------------------------
    m_defaultShader  = CreateOrGetShader("Default");
    m_defaultTexture = nullptr; // TODO: upload 1×1 RGBA(255)

    //----------------------------------------------------------
    // ⑨ 预分配常量缓冲  (UPLOAD heap, CPU 可写，避免每帧 new)
    // Descriptor Heap 就像 DX11 里的“Bind-slot 数组”，只不过现在要自己分配。
    //
    // kCBPerDraw 表示一次 draw 要用到的 CBV 槽数（这里约定 4 个：model/camera/light/extra），kMaxDraws 是一帧里预估的最大 draw 次数。
    //----------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
    cbvHeapDesc.NumDescriptors = kCBPerDraw * kMaxDraws; // 总共要放多少个 CBV
    cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 设成 SHADER_VISIBLE 才能让 GPU 直接用这个堆里的句柄。
    hr                         = m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));
    GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create CBV Heap failed")

    const UINT cbvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 计算 一个 CB 所需的字节数并做 256-byte 对齐
    // DX12 规范：任何单个常量缓冲区的 SizeInBytes 必须是 256 的整数倍，否则 CreateConstantBufferView 会报错。
    const size_t cbSizeAligned = AlignUp(sizeof(Mat44) * 2 + sizeof(float) * 4, (unsigned long long)256u);
    // 用 UPLOAD 堆 批量创建真正的常量缓冲资源
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   cbResDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSizeAligned);

    CD3DX12_CPU_DESCRIPTOR_HANDLE dst(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < kCBPerDraw * kMaxDraws; ++i)
    {
        m_cbs[i] = new ConstantBuffer(cbSizeAligned);

        hr = m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &cbResDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // Upload 堆默认只能是 READ
            nullptr,
            IID_PPV_ARGS(&m_cbs[i]->m_dx12ConstantBuffer));
        GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create CB resource failed")

        m_cbs[i]->m_constantBufferView->BufferLocation = m_cbs[i]->m_dx12ConstantBuffer->GetGPUVirtualAddress();
        m_cbs[i]->m_constantBufferView->SizeInBytes    = static_cast<UINT>(cbSizeAligned);

        m_device->CreateConstantBufferView(m_cbs[i]->m_constantBufferView, dst);
        dst.Offset(1, cbvStride); // dst 指针向后挪下一个槽
    }


    //----------------------------------------------------------
    // ⑩ Root-Signature → 默认 GraphicsPSO
    //----------------------------------------------------------
    {
        // <RootSig 已创建在前面，此处补 VS/PS 字节码与 PSO>
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

        // (1) 填输入布局
        psoDesc.InputLayout.pInputElementDescs = m_defaultShader->m_dx12InputLayout.data();
        psoDesc.InputLayout.NumElements        = static_cast<UINT>(m_defaultShader->m_dx12InputLayout.size());
        // (2) 多重采样描述
        psoDesc.SampleDesc.Count   = 1; // 不用 MSAA 就设 1
        psoDesc.SampleDesc.Quality = 0;
        // (3) 颜色、深度格式 & RenderTarget 数
        psoDesc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.NumRenderTargets = 1;
        psoDesc.DSVFormat        = DXGI_FORMAT_D24_UNORM_S8_UINT;
        // (4) 必填 SampleMask（缺省 0 会被认为非法）
        psoDesc.SampleMask            = UINT_MAX;
        psoDesc.pRootSignature        = m_rootSignature;
        psoDesc.VS                    = {m_defaultShader->m_vsBytecode->GetBufferPointer(), m_defaultShader->m_vsBytecode->GetBufferSize()};
        psoDesc.PS                    = {m_defaultShader->m_psBytecode->GetBufferPointer(), m_defaultShader->m_psBytecode->GetBufferSize()};
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        // Blend / Raster / DepthStencil desc 可调用  MakeBlendDesc(m_blend) … 
        psoDesc.BlendState        = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState   = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

        hr = m_device->CreateGraphicsPipelineState(&psoDesc,IID_PPV_ARGS(&m_defaultPSO));
        GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: Create default PSO failed")
    }
    //----------------------------------------------------------
    // ⑪ 视口 / 剪裁矩形（仅一次，勿放循环中）
    //----------------------------------------------------------
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width    = static_cast<float>(scDesc.Width);
    m_viewport.Height   = static_cast<float>(scDesc.Height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissor = {0, 0, (LONG)scDesc.Width, (LONG)scDesc.Height};


    //----------------------------------------------------------
    DebuggerPrintf("DX12Renderer StartUp — OK\n");
}


void DX12Renderer::Shutdown()
{
}

void DX12Renderer::BeginFrame()
{
}

void DX12Renderer::EndFrame()
{
}

void DX12Renderer::ClearScreen(const Rgba8& clr)
{
    UNUSED(clr)
}

void DX12Renderer::BeginCamera(const Camera& cam)
{
    UNUSED(cam);
}

void DX12Renderer::EndCamera(const Camera& cam)
{
    UNUSED(cam);
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
    Shader* shader            = new Shader(cfg);
    shader->m_vsBytecode      = vsBlob.Detach();
    shader->m_psBytecode      = psBlob.Detach();
    shader->m_dx12InputLayout = layout;
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

void DX12Renderer::BindShader(Shader* s)
{
    UNUSED(s)
}

VertexBuffer* DX12Renderer::CreateVertexBuffer(size_t size, unsigned stride)
{
    UNUSED(size)
    UNUSED(stride)
    return nullptr;
}

IndexBuffer* DX12Renderer::CreateIndexBuffer(size_t size)
{
    UNUSED(size)
    return nullptr;
}

ConstantBuffer* DX12Renderer::CreateConstantBuffer(size_t size)
{
    UNUSED(size)
    return nullptr;
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, VertexBuffer* vbo, size_t off)
{
    UNUSED(vbo)
    UNUSED(off)
    UNUSED(data)
    UNUSED(sz)
}

void DX12Renderer::CopyCPUToGPU(const void* data, size_t sz, IndexBuffer* ibo)
{
    UNUSED(ibo)
    UNUSED(data)
    UNUSED(sz)
}

void DX12Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
    UNUSED(vbo)
}

void DX12Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
    UNUSED(ibo)
}

void DX12Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
    UNUSED(slot)
    UNUSED(cbo)
}

void DX12Renderer::BindTexture(Texture* tex)
{
    UNUSED(tex)
}

void DX12Renderer::SetModelConstants(const Mat44& model, const Rgba8& tint)
{
    UNUSED(model)
    UNUSED(tint)
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
    UNUSED(data)
    UNUSED(slot)
    UNUSED(sz)
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
    UNUSED(n)
    UNUSED(v)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v)
{
    UNUSED(v)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v)
{
    UNUSED(v)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCU>& v, const std::vector<unsigned>& idx)
{
    UNUSED(v)
    UNUSED(idx)
}

void DX12Renderer::DrawVertexArray(const std::vector<Vertex_PCUTBN>& v, const std::vector<unsigned>& idx)
{
    UNUSED(v)
    UNUSED(idx)
}

void DX12Renderer::DrawVertexBuffer(VertexBuffer* vbo, int count, int offset)
{
    UNUSED(vbo)
    UNUSED(count)
    UNUSED(offset)
}

void DX12Renderer::DrawVertexIndexed(VertexBuffer* vbo, IndexBuffer* ibo, int idxCount, int idxOffset)
{
    UNUSED(vbo)
    UNUSED(ibo)
    UNUSED(idxCount)
    UNUSED(idxOffset)
}

uint64_t DX12Renderer::CalcPSOHash() const
{
    return 0;
}

void DX12Renderer::ApplyStatesIfNeeded()
{
}

void DX12Renderer::WaitForPreviousFrame()
{
}
