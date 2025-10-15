#pragma once

#include "IImGuiBackend.hpp"
#include "ImGuiSubsystemConfig.hpp"

// 前置声明DirectX类型
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

namespace enigma::core
{

// DirectX 11 ImGui后端实现
// 封装imgui_impl_dx11的功能，管理DX11渲染状态
class ImGuiBackendDX11 : public IImGuiBackend
{
public:
    // 构造函数：接收IRenderer引用，从中获取DX11资源
    // 注意：本类不拥有这些资源的所有权，只保存指针
    explicit ImGuiBackendDX11(IRenderer* renderer);
    ~ImGuiBackendDX11() override;

    //---------------------------------------------------------------------
    // IImGuiBackend接口实现
    //---------------------------------------------------------------------

    // 初始化DX11后端（调用ImGui_ImplDX11_Init）
    bool Initialize() override;

    // 关闭DX11后端（释放RenderTargetView，调用ImGui_ImplDX11_Shutdown）
    void Shutdown() override;

    // 开始新的一帧（调用ImGui_ImplDX11_NewFrame）
    void NewFrame() override;

    // 渲染ImGui绘制数据（调用ImGui_ImplDX11_RenderDrawData）
    void RenderDrawData(ImDrawData* drawData) override;

    // 处理窗口大小变化（重新创建RenderTargetView）
    void OnWindowResize(int width, int height) override;

    // 获取后端名称
    const char* GetBackendName() const override { return "DirectX11"; }

    // 获取后端类型
    RendererBackend GetBackendType() const override { return RendererBackend::DirectX11; }

private:
    //---------------------------------------------------------------------
    // 内部方法
    //---------------------------------------------------------------------

    // 创建RenderTargetView（从SwapChain的BackBuffer）
    void CreateRenderTarget();

    // 清理RenderTargetView
    void CleanupRenderTarget();

    //---------------------------------------------------------------------
    // 成员变量
    //---------------------------------------------------------------------

    // DX11设备和上下文（不拥有所有权）
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGISwapChain* m_swapChain;  // 可选，某些使用场景不需要

    // RenderTargetView（本类拥有所有权）
    ID3D11RenderTargetView* m_mainRenderTargetView;
};

} // namespace enigma::core
