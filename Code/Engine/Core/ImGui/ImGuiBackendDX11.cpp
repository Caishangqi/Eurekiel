#include "ImGuiBackendDX11.hpp"
#include "IImGuiRenderContext.hpp"
#include "../ErrorWarningAssert.hpp"

// ImGui头文件
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_dx11.h"

// DirectX 11头文件
#include <d3d11.h>
#include <dxgi.h>

namespace enigma::core
{
    //=============================================================================
    // 构造/析构
    //=============================================================================

    ImGuiBackendDX11::ImGuiBackendDX11(IImGuiRenderContext* renderContext)
        : m_device(nullptr)
          , m_context(nullptr)
          , m_swapChain(nullptr)
          , m_mainRenderTargetView(nullptr)
    {
        // 从IImGuiRenderContext获取DX11资源
        if (renderContext)
        {
            m_device    = static_cast<ID3D11Device*>(renderContext->GetD3D11Device());
            m_context   = static_cast<ID3D11DeviceContext*>(renderContext->GetD3D11DeviceContext());
            m_swapChain = static_cast<IDXGISwapChain*>(renderContext->GetD3D11SwapChain());
        }

        // 不持有设备的所有权，只保存指针
        // 设备生命周期由外部管理（通常是RenderContext）
    }

    ImGuiBackendDX11::~ImGuiBackendDX11()
    {
        // 确保资源被正确释放
        if (m_mainRenderTargetView)
        {
            DebuggerPrintf("[ImGuiBackendDX11] Warning: RenderTargetView not cleaned up before destruction\n");
            CleanupRenderTarget();
        }
    }

    //=============================================================================
    // IImGuiBackend接口实现
    //=============================================================================

    bool ImGuiBackendDX11::Initialize()
    {
        DebuggerPrintf("[ImGuiBackendDX11] Initializing...\n");

        // 1. 验证必需的资源
        if (!m_device || !m_context)
        {
            DebuggerPrintf("[ImGuiBackendDX11] Error: Device or Context is null\n");
            return false;
        }

        // 2. 初始化ImGui DX11后端
        if (!ImGui_ImplDX11_Init(m_device, m_context))
        {
            DebuggerPrintf("[ImGuiBackendDX11] Error: ImGui_ImplDX11_Init failed\n");
            return false;
        }

        // 3. 创建RenderTargetView（如果有SwapChain）
        // 注意：SwapChain可能为nullptr，这是允许的
        // 某些使用场景不需要SwapChain（例如渲染到纹理）
        if (m_swapChain)
        {
            CreateRenderTarget();
        }
        else
        {
            DebuggerPrintf("[ImGuiBackendDX11] Info: No SwapChain provided, RenderTarget will not be created\n");
        }

        DebuggerPrintf("[ImGuiBackendDX11] Initialized successfully\n");
        return true;
    }

    void ImGuiBackendDX11::Shutdown()
    {
        DebuggerPrintf("[ImGuiBackendDX11] Shutting down...\n");

        // 1. 清理RenderTargetView
        CleanupRenderTarget();

        // 2. 关闭ImGui DX11后端
        ImGui_ImplDX11_Shutdown();

        DebuggerPrintf("[ImGuiBackendDX11] Shutdown complete\n");
    }

    void ImGuiBackendDX11::NewFrame()
    {
        // 调用ImGui DX11后端的NewFrame
        // 这会更新渲染状态，准备新的一帧
        ImGui_ImplDX11_NewFrame();
    }

    void ImGuiBackendDX11::RenderDrawData(ImDrawData* drawData)
    {
        if (!drawData)
        {
            DebuggerPrintf("[ImGuiBackendDX11] Warning: RenderDrawData called with null drawData\n");
            return;
        }

        // 调用ImGui DX11后端的渲染函数
        // 这会将ImGui的绘制命令转换为DX11的绘制调用
        ImGui_ImplDX11_RenderDrawData(drawData);
    }

    void ImGuiBackendDX11::OnWindowResize(int width, int height)
    {
        DebuggerPrintf("[ImGuiBackendDX11] Window resized to %dx%d\n", width, height);

        // 如果有SwapChain，需要重新创建RenderTargetView
        if (m_swapChain)
        {
            // 1. 清理旧的RenderTargetView
            CleanupRenderTarget();

            // 2. 调整SwapChain的缓冲区大小
            // 参数：
            //   BufferCount: 0 = 保持不变
            //   Width, Height: 新的大小
            //   Format: DXGI_FORMAT_UNKNOWN = 保持不变
            //   Flags: 0 = 默认
            HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

            if (FAILED(hr))
            {
                DebuggerPrintf("[ImGuiBackendDX11] Error: Failed to resize swap chain buffers (HRESULT: 0x%08X)\n", hr);
                return;
            }

            // 3. 重新创建RenderTargetView
            CreateRenderTarget();
        }
    }

    //=============================================================================
    // 内部方法 - RenderTarget管理
    //=============================================================================

    void ImGuiBackendDX11::CreateRenderTarget()
    {
        if (!m_swapChain)
        {
            DebuggerPrintf("[ImGuiBackendDX11] Warning: Cannot create RenderTarget without SwapChain\n");
            return;
        }

        // 1. 确保旧的RenderTarget已清理
        CleanupRenderTarget();

        // 2. 从SwapChain获取BackBuffer
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT          hr         = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

        if (FAILED(hr) || !backBuffer)
        {
            DebuggerPrintf("[ImGuiBackendDX11] Error: Failed to get back buffer from swap chain (HRESULT: 0x%08X)\n", hr);
            return;
        }

        // 3. 创建RenderTargetView
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_mainRenderTargetView);

        if (FAILED(hr))
        {
            DebuggerPrintf("[ImGuiBackendDX11] Error: Failed to create render target view (HRESULT: 0x%08X)\n", hr);
            backBuffer->Release();
            return;
        }

        // 4. 释放BackBuffer（CreateRenderTargetView会增加引用计数）
        backBuffer->Release();

        DebuggerPrintf("[ImGuiBackendDX11] RenderTarget created successfully\n");
    }

    void ImGuiBackendDX11::CleanupRenderTarget()
    {
        if (m_mainRenderTargetView)
        {
            m_mainRenderTargetView->Release();
            m_mainRenderTargetView = nullptr;
            DebuggerPrintf("[ImGuiBackendDX11] RenderTarget cleaned up\n");
        }
    }
} // namespace enigma::core
