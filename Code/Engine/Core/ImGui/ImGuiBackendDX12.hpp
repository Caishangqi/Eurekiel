#pragma once

#include "IImGuiBackend.hpp"
#include "IImGuiRenderContext.hpp"
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
    // ImGui DirectX 12后端（Phase 2完整实现 + SRV增量分配）
    //=============================================================================
    //
    // 当前状态: Phase 2 - 完整实现 + Reserved Slots 0-99策略
    // - 实现IImGuiBackend接口的所有方法
    // - 使用IImGuiRenderContext接口获取DX12资源
    // - 使用ImGui v1.92.4新API (ImGui_ImplDX12_InitInfo)
    // - 实现SRV增量分配（槽位0-99预留给ImGui）
    // - 添加CommandQueue支持用于纹理上传
    //
    // SRV分配策略（Reserved Slots 0-99）:
    // - 槽位0: 字体纹理（第一次分配）
    // - 槽位1-99: 可用于RenderTarget显示等（增量分配）
    // - 槽位100+: Bindless资源区域（由BindlessIndexAllocator管理）
    //
    // 功能:
    // 1. ImGui_ImplDX12_Init初始化（使用新API）
    // 2. 自动创建字体纹理（通过ImGui后端）
    // 3. 通过回调函数增量分配SRV Descriptor
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
        // 构造函数：接收IImGuiRenderContext引用，从中获取DX12资源
        explicit ImGuiBackendDX12(IImGuiRenderContext* renderContext);
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

    private:
        //-------------------------------------------------------------------------
        // RenderContext引用（用于动态获取资源）
        //-------------------------------------------------------------------------
        IImGuiRenderContext* m_renderContext; // 保存RenderContext指针

        //-------------------------------------------------------------------------
        // DX12资源（不拥有）
        //-------------------------------------------------------------------------
        ID3D12Device*              m_device;
        ID3D12CommandQueue*        m_commandQueue;
        ID3D12DescriptorHeap*      m_srvHeap;
        ID3D12GraphicsCommandList* m_commandList;
        DXGI_FORMAT                m_rtvFormat;
        uint32_t                   m_numFramesInFlight;

        //-------------------------------------------------------------------------
        // 描述符句柄（字体纹理SRV）
        //-------------------------------------------------------------------------
        D3D12_CPU_DESCRIPTOR_HANDLE m_fontSrvCpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE m_fontSrvGpuHandle;
        bool                        m_initialized;

        //-------------------------------------------------------------------------
        // SRV增量分配（Reserved Slots 0-99策略）
        //-------------------------------------------------------------------------
        static constexpr uint32_t IMGUI_DESCRIPTOR_RESERVE = 100; // ImGui预留100个槽位
        uint32_t                  m_nextDescriptorIndex    = 0; // 下一个可用的描述符索引

        //-------------------------------------------------------------------------
        // 内部方法
        //-------------------------------------------------------------------------
        static void SrvDescriptorAlloc(ImGui_ImplDX12_InitInfo*     info,
                                       D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu);
        static void SrvDescriptorFree(ImGui_ImplDX12_InitInfo*    info,
                                      D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                      D3D12_GPU_DESCRIPTOR_HANDLE gpu);
    };
} // namespace enigma::core
