#pragma once

#include "ImGuiSubsystemConfig.hpp"

// 前置声明ImGui类型
struct ImDrawData;

namespace enigma::core
{

// ImGui后端抽象接口
// 用于封装不同渲染API（DX11/DX12/OpenGL）的ImGui实现
class IImGuiBackend
{
public:
    virtual ~IImGuiBackend() = default;

    //---------------------------------------------------------------------
    // 生命周期管理
    //---------------------------------------------------------------------

    // 初始化后端（设置渲染状态、创建资源等）
    virtual bool Initialize() = 0;

    // 关闭后端（释放资源）
    virtual void Shutdown() = 0;

    //---------------------------------------------------------------------
    // 帧渲染
    //---------------------------------------------------------------------

    // 开始新的一帧（更新渲染状态）
    virtual void NewFrame() = 0;

    // 渲染ImGui绘制数据到当前渲染目标
    virtual void RenderDrawData(ImDrawData* drawData) = 0;

    //---------------------------------------------------------------------
    // 窗口事件
    //---------------------------------------------------------------------

    // 处理窗口大小变化
    virtual void OnWindowResize(int width, int height) = 0;

    //---------------------------------------------------------------------
    // 后端信息
    //---------------------------------------------------------------------

    // 获取后端名称（例如："DirectX11", "DirectX12"）
    virtual const char* GetBackendName() const = 0;

    // 获取后端类型
    virtual RendererBackend GetBackendType() const = 0;
};

} // namespace enigma::core
