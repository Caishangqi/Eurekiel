#pragma once

#include "../SubsystemManager.hpp"
#include "ImGuiSubsystemConfig.hpp"
#include "IImGuiBackend.hpp"
#include "ImGuiMessagePreprocessor.hpp"
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>

namespace enigma::core
{
    // ImGuiSubsystem - ImGui集成子系统
    //
    // 功能：
    // - 管理ImGui上下文生命周期
    // - 根据配置创建和管理渲染后端（DX11/DX12）
    // - 提供ImGui窗口注册机制
    // - 处理ImGui的帧渲染流程
    //
    // 使用示例：
    //   ImGuiSubsystemConfig config;
    //   config.renderContext = m_renderContext;  // 注入IImGuiRenderContext引用
    //   config.targetWindow = m_window;
    //
    //   auto imguiSubsystem = std::make_unique<ImGuiSubsystem>(config);
    //   subsystemManager.RegisterSubsystem(std::move(imguiSubsystem));
    //
    class ImGuiSubsystem : public EngineSubsystem
    {
    public:
        // 使用配置构造ImGuiSubsystem
        explicit ImGuiSubsystem(const ImGuiSubsystemConfig& config);
        ~ImGuiSubsystem() override;

        //---------------------------------------------------------------------
        // EngineSubsystem接口实现
        //---------------------------------------------------------------------

        void Initialize() override; // 早期初始化：创建ImGui上下文、配置、创建后端
        void Startup() override; // 主启动阶段（预留）
        void Shutdown() override; // 关闭：销毁后端、销毁ImGui上下文

        void BeginFrame() override; // 每帧开始：调用后端NewFrame、ImGui::NewFrame
        void EndFrame() override; // 每帧结束：处理多窗口更新

        // Render不在标准生命周期中，需要外部在合适时机调用
        // 通常在主渲染流程的UI阶段调用
        void Render();

        // 子系统信息
        DECLARE_SUBSYSTEM(ImGuiSubsystem, "ImGuiSubsystem", 400);

        // 需要早期初始化（在Startup之前）
        bool RequiresInitialize() const override { return true; }

        // 需要每帧更新（BeginFrame/EndFrame）
        bool RequiresGameLoop() const override { return true; }

        //---------------------------------------------------------------------
        // ImGui窗口注册
        //---------------------------------------------------------------------

        // 窗口回调函数类型
        using ImGuiWindowCallback = std::function<void()>;

        // 注册ImGui窗口
        // name: 窗口唯一标识符
        // callback: 窗口渲染回调（在Render时调用）
        void RegisterWindow(const std::string& name, ImGuiWindowCallback callback);

        // 注销ImGui窗口
        void UnregisterWindow(const std::string& name);

        //---------------------------------------------------------------------
        // 后端信息查询
        //---------------------------------------------------------------------

        // 获取后端名称
        const char* GetBackendName() const;

        // 检查后端是否已初始化
        bool IsBackendInitialized() const { return m_backend != nullptr; }

    private:
        //---------------------------------------------------------------------
        // 内部方法
        //---------------------------------------------------------------------

        // 创建渲染后端（根据config.backendType）
        bool CreateBackend();

        // 销毁渲染后端
        void DestroyBackend();

        // 验证配置是否有效
        bool ValidateConfig() const;

        // 初始化ImGui上下文和配置
        bool InitializeImGuiContext();

        // 销毁ImGui上下文
        void ShutdownImGuiContext();

        //---------------------------------------------------------------------
        // 成员变量
        //---------------------------------------------------------------------

        ImGuiSubsystemConfig                                 m_config; // 配置
        std::unique_ptr<IImGuiBackend>                       m_backend; // 渲染后端
        std::unique_ptr<ImGuiMessagePreprocessor>            m_messagePreprocessor; // Windows消息预处理器
        std::unordered_map<std::string, ImGuiWindowCallback> m_windows; // 注册的窗口回调
        bool                                                 m_imguiContextInitialized = false; // ImGui上下文是否已初始化
        std::string                                          m_iniFilePathStorage; // 存储ini文件路径字符串（ImGui需要持久指针）
    };
} // namespace enigma::core
