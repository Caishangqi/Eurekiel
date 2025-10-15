#include "ImGuiBackendDX12.hpp"
#include "../ErrorWarningAssert.hpp"

// ImGui头文件
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx12.h"

// DirectX 12头文件
#include <d3d12.h>
#include <dxgi.h>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::core
{
    //=============================================================================
    // 构造/析构
    //=============================================================================

    ImGuiBackendDX12::ImGuiBackendDX12(IRenderer* renderer)
        : m_device(nullptr)
          , m_commandQueue(nullptr)
          , m_srvHeap(nullptr)
          , m_commandList(nullptr)
          , m_rtvFormat(DXGI_FORMAT_UNKNOWN)
          , m_numFramesInFlight(0)
          , m_fontSrvCpuHandle{}
          , m_fontSrvGpuHandle{}
          , m_initialized(false)
    {
        // 从IRenderer获取DX12资源
        if (renderer)
        {
            m_device            = renderer->GetD3D12Device();
            m_commandQueue      = renderer->GetD3D12CommandQueue();
            m_srvHeap           = renderer->GetD3D12SRVHeap();
            m_commandList       = renderer->GetD3D12CommandList();
            m_rtvFormat         = renderer->GetRTVFormat();
            m_numFramesInFlight = renderer->GetNumFramesInFlight();
        }

        DebuggerPrintf("[ImGuiBackendDX12] Constructor - Resources retrieved from IRenderer\n");
        DebuggerPrintf("[ImGuiBackendDX12]   Device: %p\n", m_device);
        DebuggerPrintf("[ImGuiBackendDX12]   CommandQueue: %p\n", m_commandQueue);
        DebuggerPrintf("[ImGuiBackendDX12]   SRV Heap: %p\n", m_srvHeap);
        DebuggerPrintf("[ImGuiBackendDX12]   Command List: %p\n", m_commandList);
        DebuggerPrintf("[ImGuiBackendDX12]   RTV Format: %d\n", static_cast<int>(m_rtvFormat));
        DebuggerPrintf("[ImGuiBackendDX12]   Frames in Flight: %u\n", m_numFramesInFlight);
    }

    ImGuiBackendDX12::~ImGuiBackendDX12()
    {
        if (m_initialized)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Warning: Not shutdown before destruction\n");
            Shutdown();
        }
    }

    //=============================================================================
    // IImGuiBackend接口实现
    //=============================================================================

    bool ImGuiBackendDX12::Initialize()
    {
        DebuggerPrintf("[ImGuiBackendDX12] Initializing...\n");

        // 1. 验证必需的资源
        if (!m_device || !m_commandQueue || !m_srvHeap || !m_commandList)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: Invalid DX12 resources\n");
            DebuggerPrintf("[ImGuiBackendDX12]   Device: %p\n", m_device);
            DebuggerPrintf("[ImGuiBackendDX12]   CommandQueue: %p\n", m_commandQueue);
            DebuggerPrintf("[ImGuiBackendDX12]   SRV Heap: %p\n", m_srvHeap);
            DebuggerPrintf("[ImGuiBackendDX12]   Command List: %p\n", m_commandList);
            return false;
        }

        // 2. 验证SRV Heap容量
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = m_srvHeap->GetDesc();
        if (heapDesc.NumDescriptors < 1)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: SRV Heap too small (need at least 1 descriptor)\n");
            return false;
        }

        // 3. 准备ImGui_ImplDX12_InitInfo（新API）
        ImGui_ImplDX12_InitInfo initInfo;
        initInfo.Device               = m_device;
        initInfo.CommandQueue         = m_commandQueue;
        initInfo.NumFramesInFlight    = static_cast<int>(m_numFramesInFlight);
        initInfo.RTVFormat            = m_rtvFormat;
        initInfo.DSVFormat            = DXGI_FORMAT_UNKNOWN; // 不使用深度
        initInfo.SrvDescriptorHeap    = m_srvHeap;
        initInfo.SrvDescriptorAllocFn = &ImGuiBackendDX12::SrvDescriptorAlloc;
        initInfo.SrvDescriptorFreeFn  = &ImGuiBackendDX12::SrvDescriptorFree;
        initInfo.UserData             = this; // 将this指针传递给回调函数

        // 4. 初始化ImGui DX12后端
        if (!ImGui_ImplDX12_Init(&initInfo))
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: ImGui_ImplDX12_Init failed\n");
            return false;
        }

        m_initialized = true;
        DebuggerPrintf("[ImGuiBackendDX12] Initialized successfully\n");
        DebuggerPrintf("[ImGuiBackendDX12]   Frames in Flight: %u\n", m_numFramesInFlight);
        DebuggerPrintf("[ImGuiBackendDX12]   RTV Format: %d\n", m_rtvFormat);
        return true;
    }

    void ImGuiBackendDX12::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        DebuggerPrintf("[ImGuiBackendDX12] Shutting down...\n");

        // 关闭ImGui DX12后端（会自动释放所有资源）
        ImGui_ImplDX12_Shutdown();

        // 清空句柄
        m_fontSrvCpuHandle = {};
        m_fontSrvGpuHandle = {};
        m_initialized      = false;

        DebuggerPrintf("[ImGuiBackendDX12] Shutdown complete\n");
    }

    void ImGuiBackendDX12::NewFrame()
    {
        if (!m_initialized)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Warning: NewFrame called before initialization\n");
            return;
        }

        // ImGui DX12后端NewFrame
        ImGui_ImplDX12_NewFrame();
    }

    void ImGuiBackendDX12::RenderDrawData(ImDrawData* drawData)
    {
        if (!m_initialized)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Warning: RenderDrawData called before initialization\n");
            return;
        }

        // 1. 验证数据
        if (!drawData || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
        {
            return; // 无效的绘制数据
        }

        // 2. 设置Descriptor Heaps
        ID3D12DescriptorHeap* heaps[] = {m_srvHeap};
        m_commandList->SetDescriptorHeaps(1, heaps);

        // 3. 渲染ImGui绘制数据
        ImGui_ImplDX12_RenderDrawData(drawData, m_commandList);
    }

    void ImGuiBackendDX12::OnWindowResize(int width, int height)
    {
        // DX12后端不需要特殊的Resize处理
        // SwapChain的Resize由渲染系统负责
        // ImGui的DisplaySize会在NewFrame时自动更新
        DebuggerPrintf("[ImGuiBackendDX12] Window resized to %dx%d (no action needed)\n", width, height);
    }

    //=============================================================================
    // 描述符分配回调（静态方法）
    //=============================================================================

    void ImGuiBackendDX12::SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*     info,
                                              D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                                              D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
    {
        // 从InitInfo获取backend实例
        ImGuiBackendDX12* backend = static_cast<ImGuiBackendDX12*>(info->UserData);

        // 使用SRV Heap的起始位置作为字体纹理的Descriptor
        // 注意：实际应用中可能需要更复杂的Descriptor分配策略
        *out_cpu = backend->m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        *out_gpu = backend->m_srvHeap->GetGPUDescriptorHandleForHeapStart();

        // 保存句柄供后续使用
        backend->m_fontSrvCpuHandle = *out_cpu;
        backend->m_fontSrvGpuHandle = *out_gpu;

        DebuggerPrintf("[ImGuiBackendDX12] Descriptor allocated for font texture\n");
    }

    void ImGuiBackendDX12::SrvDescriptorFree(ImGui_ImplDX12_InitInfo*    info,
                                             D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                             D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        UNUSED(info)
        UNUSED(cpu)
        UNUSED(gpu)
        // 在简单的实现中不需要特殊处理
        // 实际应用中可能需要将Descriptor返回到分配器
        DebuggerPrintf("[ImGuiBackendDX12] Descriptor freed\n");
    }
} // namespace enigma::core
