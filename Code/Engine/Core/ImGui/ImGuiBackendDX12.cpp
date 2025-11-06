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

    ImGuiBackendDX12::ImGuiBackendDX12(IImGuiRenderContext* renderContext)
        : m_renderContext(renderContext) // 保存指针
          , m_device(nullptr)
          , m_commandQueue(nullptr)
          , m_srvHeap(nullptr)
          , m_commandList(nullptr)
          , m_rtvFormat(DXGI_FORMAT_UNKNOWN)
          , m_numFramesInFlight(0)
          , m_fontSrvCpuHandle{}
          , m_fontSrvGpuHandle{}
          , m_initialized(false)
          , m_nextDescriptorIndex(0)
    {
        // 从IImGuiRenderContext获取DX12资源（需要类型转换）
        if (renderContext)
        {
            m_device            = static_cast<ID3D12Device*>(renderContext->GetDevice());
            m_commandQueue      = static_cast<ID3D12CommandQueue*>(renderContext->GetCommandQueue());
            m_srvHeap           = static_cast<ID3D12DescriptorHeap*>(renderContext->GetSRVHeap());
            m_commandList       = static_cast<ID3D12GraphicsCommandList*>(renderContext->GetCommandList());
            m_rtvFormat         = renderContext->GetRTVFormat();
            m_numFramesInFlight = renderContext->GetNumFramesInFlight();
        }

        // 检查关键资源（Device, CommandQueue, SRVHeap必须有效）
        if (!m_device || !m_commandQueue || !m_srvHeap)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: Missing critical DX12 resources\n");
            DebuggerPrintf("[ImGuiBackendDX12]   Device: %p\n", m_device);
            DebuggerPrintf("[ImGuiBackendDX12]   CommandQueue: %p\n", m_commandQueue);
            DebuggerPrintf("[ImGuiBackendDX12]   SRV Heap: %p\n", m_srvHeap);
        }
        else
        {
            DebuggerPrintf("[ImGuiBackendDX12] Constructor - Resources retrieved from IImGuiRenderContext\n");
            DebuggerPrintf("[ImGuiBackendDX12]   Device: %p\n", m_device);
            DebuggerPrintf("[ImGuiBackendDX12]   CommandQueue: %p\n", m_commandQueue);
            DebuggerPrintf("[ImGuiBackendDX12]   SRV Heap: %p\n", m_srvHeap);
            DebuggerPrintf("[ImGuiBackendDX12]   RTV Format: %d\n", static_cast<int>(m_rtvFormat));
            DebuggerPrintf("[ImGuiBackendDX12]   Frames in Flight: %u\n", m_numFramesInFlight);
            DebuggerPrintf("[ImGuiBackendDX12]   Reserved SRV Slots: 0-%u (Total: %u)\n",
                           IMGUI_DESCRIPTOR_RESERVE - 1, IMGUI_DESCRIPTOR_RESERVE);
        }

        // CommandList允许在构造时为nullptr（将在NewFrame()时动态获取）
        if (!m_commandList)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Warning: CommandList is nullptr during construction\n");
            DebuggerPrintf("[ImGuiBackendDX12]   This is expected during Initialize phase\n");
            DebuggerPrintf("[ImGuiBackendDX12]   CommandList will be retrieved dynamically in NewFrame()\n");
        }
        else
        {
            DebuggerPrintf("[ImGuiBackendDX12]   Command List: %p\n", m_commandList);
        }
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

        // 1. 验证关键DX12资源（Device, CommandQueue, SRVHeap）
        if (!m_device || !m_commandQueue || !m_srvHeap)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: Missing critical DX12 resources for initialization\n");
            DebuggerPrintf("[ImGuiBackendDX12]   Device: %p\n", m_device);
            DebuggerPrintf("[ImGuiBackendDX12]   CommandQueue: %p\n", m_commandQueue);
            DebuggerPrintf("[ImGuiBackendDX12]   SRV Heap: %p\n", m_srvHeap);
            return false;
        }

        // CommandList允许为nullptr（在NewFrame()时动态获取）
        if (!m_commandList)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Info: CommandList is nullptr, will be retrieved in NewFrame()\n");
        }

        DebuggerPrintf("[ImGuiBackendDX12] Core resources validated successfully\n");

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

        // 动态获取CommandList（每帧刷新）
        // 在Initialize阶段CommandList可能为nullptr,这里确保获取最新的CommandList
        if (m_renderContext)
        {
            m_commandList = static_cast<ID3D12GraphicsCommandList*>(m_renderContext->GetCommandList());
        }

        if (!m_commandList)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Warning: CommandList is nullptr in NewFrame(), skipping ImGui rendering\n");
            return; // 跳过本帧ImGui渲染
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

        // ========================================================================
        // [FIX] (2025-10-22): 动态获取最新的CommandList（与NewFrame()保持一致）
        // ========================================================================
        // 问题原因: NewFrame()动态获取CommandList，但RenderDrawData()使用旧的m_commandList
        // - 如果BeginFrame之前CommandList为nullptr，NewFrame()会跳过
        // - 但RenderDrawData()仍然使用nullptr的m_commandList，导致崩溃
        //
        // 修复: 在RenderDrawData()时重新获取最新的CommandList
        // - 确保每次渲染都使用当前帧的有效CommandList
        // - 与NewFrame()的逻辑保持一致
        // ========================================================================
        if (m_renderContext)
        {
            m_commandList = static_cast<ID3D12GraphicsCommandList*>(m_renderContext->GetCommandList());
        }

        if (!m_commandList)
        {
            DebuggerPrintf("[ImGuiBackendDX12] Error: CommandList is nullptr in RenderDrawData(), skipping\n");
            return;
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
    // 描述符分配回调（静态方法 - 增量分配实现）
    //=============================================================================

    void ImGuiBackendDX12::SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*     info,
                                              D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                                              D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
    {
        // 从InitInfo获取backend实例
        ImGuiBackendDX12* backend = static_cast<ImGuiBackendDX12*>(info->UserData);

        // 检查是否超过预留范围（槽位0-99）
        if (backend->m_nextDescriptorIndex >= IMGUI_DESCRIPTOR_RESERVE)
        {
            ERROR_AND_DIE("ImGui SRV Descriptor pool exhausted! Max reserved slots: 100");
        }

        // 获取描述符大小（用于计算偏移）
        UINT descriptorSize = backend->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // 计算CPU句柄（起始位置 + 索引偏移）
        *out_cpu = backend->m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        out_cpu->ptr += backend->m_nextDescriptorIndex * descriptorSize;

        // 计算GPU句柄（起始位置 + 索引偏移）
        *out_gpu = backend->m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        out_gpu->ptr += backend->m_nextDescriptorIndex * descriptorSize;

        // 保存第一次分配的句柄（字体纹理）
        if (backend->m_nextDescriptorIndex == 0)
        {
            backend->m_fontSrvCpuHandle = *out_cpu;
            backend->m_fontSrvGpuHandle = *out_gpu;
        }

        DebuggerPrintf("[ImGuiBackendDX12] Descriptor allocated at slot %u (CPU: 0x%llx, GPU: 0x%llx)\n",
                       backend->m_nextDescriptorIndex, out_cpu->ptr, out_gpu->ptr);

        // 移动到下一个槽位
        backend->m_nextDescriptorIndex++;
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
