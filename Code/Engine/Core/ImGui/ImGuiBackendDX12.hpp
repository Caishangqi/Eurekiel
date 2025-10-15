#pragma once

#include "IImGuiBackend.hpp"
#include "ImGuiSubsystemConfig.hpp"

// 前置声明DirectX 12类型
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_GPU_DESCRIPTOR_HANDLE;
struct ImGui_ImplDX12_InitInfo;
enum DXGI_FORMAT;

namespace enigma::core
{

//=============================================================================
// ImGui DirectX 12后端（Phase 2完整实现）
//=============================================================================
//
// 当前状态: Phase 2 - 完整实现
// - 实现IImGuiBackend接口的所有方法
// - 使用ImGui v1.92.4新API (ImGui_ImplDX12_InitInfo)
// - 实现描述符分配回调函数
// - 添加CommandQueue支持用于纹理上传
//
// 功能:
// 1. ImGui_ImplDX12_Init初始化（使用新API）
// 2. 自动创建字体纹理（通过ImGui后端）
// 3. 通过回调函数分配SRV Descriptor
// 4. 渲染ImGui绘制数据到CommandList
// 5. 完整的资源清理流程
//
// 参考资源:
// - ImGui官方DX12示例: https://github.com/ocornut/imgui/tree/master/examples/example_win32_directx12
// - imgui_impl_dx12.h API文档（v1.92.4）
//
//=============================================================================
class ImGuiBackendDX12 : public IImGuiBackend
{
public:
    // 构造函数：接收IRenderer引用，从中获取DX12资源
    explicit ImGuiBackendDX12(IRenderer* renderer);
    ~ImGuiBackendDX12() override;

    //-------------------------------------------------------------------------
    // IImGuiBackend接口实现
    //-------------------------------------------------------------------------

    // 初始化后端
    bool Initialize() override;

    // 关闭后端
    void Shutdown() override;

    // 开始新的一帧
    void NewFrame() override;

    // 渲染ImGui绘制数据
    void RenderDrawData(ImDrawData* drawData) override;

    // 处理窗口大小变化
    void OnWindowResize(int width, int height) override;

    //-------------------------------------------------------------------------
    // 后端信息
    //-------------------------------------------------------------------------

    const char* GetBackendName() const override { return "DirectX12"; }
    RendererBackend GetBackendType() const override { return RendererBackend::DirectX12; }

private:
    //-------------------------------------------------------------------------
    // DX12资源（不拥有）
    //-------------------------------------------------------------------------
    ID3D12Device* m_device;
    ID3D12CommandQueue* m_commandQueue;
    ID3D12DescriptorHeap* m_srvHeap;
    ID3D12GraphicsCommandList* m_commandList;
    DXGI_FORMAT m_rtvFormat;
    uint32_t m_numFramesInFlight;

    //-------------------------------------------------------------------------
    // 描述符句柄（字体纹理SRV）
    //-------------------------------------------------------------------------
    D3D12_CPU_DESCRIPTOR_HANDLE m_fontSrvCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_fontSrvGpuHandle;
    bool m_initialized;

    //-------------------------------------------------------------------------
    // 内部方法
    //-------------------------------------------------------------------------
    static void SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo* info,
                                    D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                                    D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu);
    static void SrvDescriptorFree(ImGui_ImplDX12_InitInfo* info,
                                   D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE gpu);
};

} // namespace enigma::core
