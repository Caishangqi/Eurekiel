#include "ImGuiSubsystem.hpp"
#include "IImGuiRenderContext.hpp"
#include "../ErrorWarningAssert.hpp"
#include "../StringUtils.hpp"
#include "../../Window/Window.hpp"

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"

// ImGui后端实现
#include <filesystem>

using namespace enigma::core;
ImGuiSubsystem* g_theImGui = nullptr;

namespace enigma::core
{
    //=============================================================================
    // 构造/析构
    //=============================================================================

    ImGuiSubsystem::ImGuiSubsystem(const ImGuiSubsystemConfig& config)
        : m_config(config)
    {
    }

    ImGuiSubsystem::~ImGuiSubsystem()
    {
        if (m_imguiContextInitialized)
        {
            Shutdown();
        }
    }

    //=============================================================================
    // EngineSubsystem接口实现
    //=============================================================================

    void ImGuiSubsystem::Initialize()
    {
        // 1. 验证配置
        if (!ValidateConfig())
        {
            ERROR_AND_DIE("ImGuiSubsystem: Invalid configuration")
        }

        // 2. 初始化ImGui上下文
        if (!InitializeImGuiContext())
        {
            ERROR_AND_DIE("ImGuiSubsystem: Failed to initialize ImGui context")
        }

        // 3. Backend创建延迟到Startup()阶段
        //    此时CommandList尚未创建,延迟到Startup确保资源完整
        DebuggerPrintf("[ImGuiSubsystem] Backend creation deferred to Startup() phase\n");

        // 4. 创建并注册消息预处理器
        m_messagePreprocessor = std::make_unique<ImGuiMessagePreprocessor>();
        if (m_config.targetWindow)
        {
            m_config.targetWindow->RegisterMessagePreprocessor(m_messagePreprocessor.get());
        }

        DebuggerPrintf("[ImGuiSubsystem] Initialized successfully (backend creation deferred)\n");
    }

    void ImGuiSubsystem::Startup()
    {
        DebuggerPrintf("[ImGuiSubsystem] Starting up...\n");

        // 检查RenderContext是否就绪
        if (!m_config.renderContext || !m_config.renderContext->IsReady())
        {
            ERROR_AND_DIE("ImGuiSubsystem: RenderContext is not ready in Startup()")
        }

        // 创建渲染后端（此时RendererSubsystem已经Initialize完成）
        DebuggerPrintf("[ImGuiSubsystem] Creating rendering backend...\n");
        if (!CreateBackend())
        {
            ERROR_AND_DIE("ImGuiSubsystem: Failed to create rendering backend in Startup()")
        }

        DebuggerPrintf("[ImGuiSubsystem] Startup completed with backend: %s\n", GetBackendName());
        g_theImGui = this;
    }

    void ImGuiSubsystem::Shutdown()
    {
        DebuggerPrintf("[ImGuiSubsystem] Shutting down...\n");

        // 0. 注销并销毁消息预处理器
        if (m_messagePreprocessor && m_config.targetWindow)
        {
            m_config.targetWindow->UnregisterMessagePreprocessor(m_messagePreprocessor.get());
        }
        m_messagePreprocessor.reset();

        // 1. 清空窗口注册
        m_windows.clear();

        // 2. 销毁渲染后端
        DestroyBackend();

        // 3. 销毁ImGui上下文
        ShutdownImGuiContext();

        DebuggerPrintf("[ImGuiSubsystem] Shutdown completed\n");
    }

    void ImGuiSubsystem::BeginFrame()
    {
        if (!m_imguiContextInitialized)
        {
            return;
        }

        // 1. Win32平台层NewFrame
        ImGui_ImplWin32_NewFrame();

        // 2. 后端NewFrame
        if (m_backend)
        {
            m_backend->NewFrame();
        }

        // 3. ImGui NewFrame
        ImGui::NewFrame();
    }

    void ImGuiSubsystem::EndFrame()
    {
        if (!m_imguiContextInitialized)
        {
            return;
        }

        // 注意：Viewports功能需要ImGui docking分支
        // 标准版本不支持ViewportsEnable、UpdatePlatformWindows和RenderPlatformWindowsDefault
        // 如果需要这些功能，请使用ImGui docking分支

        // 处理多窗口支持（仅在docking分支中可用）
        // ImGuiIO& io = ImGui::GetIO();
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        // {
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        // }
    }

    void ImGuiSubsystem::Render()
    {
        if (!m_imguiContextInitialized || !m_backend)
        {
            return;
        }

        // 1. 渲染所有注册的窗口
        for (const auto& [name, callback] : m_windows)
        {
            callback();
        }

        // 2. 渲染ImGui
        ImGui::Render();
        m_backend->RenderDrawData(ImGui::GetDrawData());
    }

    //=============================================================================
    // ImGui窗口注册
    //=============================================================================

    void ImGuiSubsystem::RegisterWindow(const std::string& name, ImGuiWindowCallback callback)
    {
        if (name.empty())
        {
            DebuggerPrintf("[ImGuiSubsystem] Warning: Cannot register window with empty name\n");
            return;
        }

        if (!callback)
        {
            DebuggerPrintf("[ImGuiSubsystem] Warning: Cannot register window '%s' with null callback\n", name.c_str());
            return;
        }

        if (m_windows.find(name) != m_windows.end())
        {
            DebuggerPrintf("[ImGuiSubsystem] Warning: Window '%s' is already registered, overwriting\n", name.c_str());
        }

        m_windows[name] = callback;
        DebuggerPrintf("[ImGuiSubsystem] Registered window: %s\n", name.c_str());
    }

    void ImGuiSubsystem::UnregisterWindow(const std::string& name)
    {
        auto it = m_windows.find(name);
        if (it != m_windows.end())
        {
            m_windows.erase(it);
            DebuggerPrintf("[ImGuiSubsystem] Unregistered window: %s\n", name.c_str());
        }
        else
        {
            DebuggerPrintf("[ImGuiSubsystem] Warning: Cannot unregister window '%s', not found\n", name.c_str());
        }
    }

    //=============================================================================
    // 后端信息查询
    //=============================================================================

    const char* ImGuiSubsystem::GetBackendName() const
    {
        if (m_backend)
        {
            return m_backend->GetBackendName();
        }
        return "None";
    }

    //=============================================================================
    // 内部方法 - 配置验证
    //=============================================================================

    bool ImGuiSubsystem::ValidateConfig() const
    {
        // 1. 检查RenderContext引用
        if (!m_config.renderContext)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: RenderContext not specified\n");
            return false;
        }

        // 2. 检查目标窗口
        if (!m_config.targetWindow)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: Target window not specified\n");
            return false;
        }

        // 3. IsReady()检查推迟到Startup()阶段
        //    Initialize()阶段只验证指针有效性,不检查资源就绪状态
        //    (RenderContext在Initialize阶段可能尚未完成资源创建)
        DebuggerPrintf("[ImGuiSubsystem] RenderContext pointer validated (readiness check deferred to Startup)\n");

        return true;
    }

    //=============================================================================
    // 内部方法 - ImGui上下文管理
    //=============================================================================

    bool ImGuiSubsystem::InitializeImGuiContext()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // 配置ini文件路径
        if (!m_config.iniFilePath.empty())
        {
            // 注意：ImGui需要持久的字符串指针，不能使用临时string的c_str()
            // 我们在m_iniFilePathStorage中保存这个字符串
            m_iniFilePathStorage = m_config.iniFilePath;
            io.IniFilename       = m_iniFilePathStorage.c_str();

            // 确保ini文件目录存在
            std::string dirPath = m_iniFilePathStorage.substr(0, m_iniFilePathStorage.find_last_of("/\\"));
            if (!dirPath.empty())
            {
                // 使用std::filesystem创建目录（C++17特性）
                try
                {
                    std::filesystem::create_directories(dirPath);
                    DebuggerPrintf("[ImGuiSubsystem] Created ini file directory: %s\n", dirPath.c_str());
                }
                catch (const std::exception& e)
                {
                    DebuggerPrintf("[ImGuiSubsystem] Warning: Failed to create ini file directory: %s\n", e.what());
                }
            }

            DebuggerPrintf("[ImGuiSubsystem] ImGui ini file path set to: %s\n", io.IniFilename);
        }
        else
        {
            io.IniFilename = nullptr; // 禁用ini文件
            DebuggerPrintf("[ImGuiSubsystem] ImGui ini file disabled\n");
        }

        // 注意：Docking和Viewports功能需要ImGui docking分支
        // 标准版本不支持DockingEnable和ViewportsEnable标志
        // 如果需要这些功能，请使用ImGui docking分支

        // 配置ImGui标志
        // if (m_config.enableDocking)
        // {
        //     io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // }

        // if (m_config.enableViewports)
        // {
        //     io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        // }

        if (m_config.enableKeyboardNav)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }

        if (m_config.enableGamepadNav)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        // 设置样式
        ImGui::StyleColorsDark();

        // 加载字体
        if (!m_config.defaultFontPath.empty())
        {
            io.Fonts->AddFontFromFileTTF(
                m_config.defaultFontPath.c_str(),
                m_config.defaultFontSize
            );
        }

        // 初始化Win32平台层
        if (m_config.targetWindow)
        {
            ImGui_ImplWin32_Init(m_config.targetWindow->GetWindowHandle());
        }

        m_imguiContextInitialized = true;
        DebuggerPrintf("[ImGuiSubsystem] ImGui context initialized successfully\n");
        return true;
    }

    void ImGuiSubsystem::ShutdownImGuiContext()
    {
        if (!m_imguiContextInitialized)
        {
            return;
        }

        // 关闭Win32平台层
        ImGui_ImplWin32_Shutdown();

        // 销毁ImGui上下文
        ImGui::DestroyContext();

        m_imguiContextInitialized = false;
        DebuggerPrintf("[ImGuiSubsystem] ImGui context shutdown\n");
    }

    //=============================================================================
    // 内部方法 - 后端管理
    //=============================================================================

    bool ImGuiSubsystem::CreateBackend()
    {
        DebuggerPrintf("[ImGuiSubsystem] Creating backend via factory method...\n");

        // 使用Context的工厂方法创建Backend（工厂方法模式）
        m_backend = m_config.renderContext->CreateBackend();

        if (!m_backend)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: Failed to create backend\n");
            return false;
        }

        // 初始化后端
        if (!m_backend->Initialize())
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: Backend initialization failed\n");
            m_backend.reset();
            return false;
        }

        DebuggerPrintf("[ImGuiSubsystem] Backend created and initialized successfully: %s\n",
                       m_backend->GetBackendName());
        return true;
    }

    void ImGuiSubsystem::DestroyBackend()
    {
        if (m_backend)
        {
            DebuggerPrintf("[ImGuiSubsystem] Destroying backend: %s\n", m_backend->GetBackendName());
            m_backend->Shutdown();
            m_backend.reset();
        }
    }
} // namespace enigma::core
