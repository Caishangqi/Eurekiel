#include "ImGuiSubsystem.hpp"
#include "IImGuiRenderContext.hpp"
#include "../ErrorWarningAssert.hpp"
#include "../StringUtils.hpp"
#include "../../Window/Window.hpp"

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include "ThirdParty/imgui/backends/imgui_impl_win32.h"

// ImGui backend implementation
#include <filesystem>

using namespace enigma::core;
ImGuiSubsystem* g_theImGui = nullptr;

namespace enigma::core
{
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

    //==============================================================================
    // EngineSubsystem interface implementation
    //==============================================================================
    void ImGuiSubsystem::Initialize()
    {
        // Verify configuration
        if (!ValidateConfig())
        {
            ERROR_AND_DIE("ImGuiSubsystem: Invalid configuration")
        }

        // Initialize ImGui context
        if (!InitializeImGuiContext())
        {
            ERROR_AND_DIE("ImGuiSubsystem: Failed to initialize ImGui context")
        }

        // Backend creation is delayed until the Startup() phase
        // At this time, the CommandList has not been created yet, and it is delayed until Startup to ensure that the resources are complete.
        DebuggerPrintf("[ImGuiSubsystem] Backend creation deferred to Startup() phase\n");

        // Create and register message preprocessor
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

        // Check if RenderContext is ready
        if (!m_config.renderContext || !m_config.renderContext->IsReady())
        {
            ERROR_AND_DIE("ImGuiSubsystem: RenderContext is not ready in Startup()")
        }

        // Create a rendering backend (RendererSubsystem has already been Initialized at this time)
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

        // Unregister and destroy the message preprocessor
        if (m_messagePreprocessor && m_config.targetWindow)
        {
            m_config.targetWindow->UnregisterMessagePreprocessor(m_messagePreprocessor.get());
        }
        m_messagePreprocessor.reset();

        // Clear window registration
        m_windows.clear();

        // Destroy the rendering backend
        DestroyBackend();

        // Destroy ImGui context
        ShutdownImGuiContext();

        DebuggerPrintf("[ImGuiSubsystem] Shutdown completed\n");
    }

    void ImGuiSubsystem::BeginFrame()
    {
        if (!m_imguiContextInitialized)
        {
            return;
        }

        // Win32 platform layer NewFrame
        ImGui_ImplWin32_NewFrame();

        // Backend NewFrame
        if (m_backend)
        {
            m_backend->NewFrame();
        }

        // ImGui NewFrame
        ImGui::NewFrame();
    }

    void ImGuiSubsystem::EndFrame()
    {
        if (!m_imguiContextInitialized)
        {
            return;
        }

        // Handle multi-viewport support (Docking branch function)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiSubsystem::Render()
    {
        if (!m_imguiContextInitialized || !m_backend)
        {
            return;
        }

        // Render all registered windows
        for (const auto& [name, callback] : m_windows)
        {
            callback();
        }

        // Render ImGui
        ImGui::Render();
        m_backend->RenderDrawData(ImGui::GetDrawData());
    }

    //==============================================================================
    // ImGui window registration
    //==============================================================================
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

    //==============================================================================
    // Backend information query
    //==============================================================================
    const char* ImGuiSubsystem::GetBackendName() const
    {
        if (m_backend)
        {
            return m_backend->GetBackendName();
        }
        return "None";
    }

    //==============================================================================
    // Internal method - configuration validation
    //==============================================================================
    bool ImGuiSubsystem::ValidateConfig() const
    {
        // Check the RenderContext reference
        if (!m_config.renderContext)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: RenderContext not specified\n");
            return false;
        }

        // Check the target window
        if (!m_config.targetWindow)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: Target window not specified\n");
            return false;
        }

        //IsReady() check is postponed to Startup() phase
        //The Initialize() phase only verifies the validity of the pointer and does not check the resource readiness status.
        // (RenderContext may not have completed resource creation during the Initialize phase)
        DebuggerPrintf("[ImGuiSubsystem] RenderContext pointer validated (readiness check deferred to Startup)\n");

        return true;
    }

    //==============================================================================
    // Internal method - ImGui context management
    //==============================================================================
    bool ImGuiSubsystem::InitializeImGuiContext()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // Configure ini file path
        if (!m_config.iniFilePath.empty())
        {
            // Note: ImGui requires a persistent string pointer and cannot use c_str() of temporary string.
            // We save this string in m_iniFilePathStorage
            m_iniFilePathStorage = m_config.iniFilePath;
            io.IniFilename       = m_iniFilePathStorage.c_str();

            // Make sure the ini file directory exists
            std::string dirPath = m_iniFilePathStorage.substr(0, m_iniFilePathStorage.find_last_of("/\\"));
            if (!dirPath.empty())
            {
                // Use std::filesystem to create a directory (C++17 feature)
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
            io.IniFilename = nullptr; // Disable ini file
            DebuggerPrintf("[ImGuiSubsystem] ImGui ini file disabled\n");
        }

        // Configure Docking and Viewports flags (requires ImGui docking branch)
        if (m_config.enableDocking)
        {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }

        if (m_config.enableViewports)
        {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        if (m_config.enableKeyboardNav)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }

        if (m_config.enableGamepadNav)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        // Set style
        ImGui::StyleColorsDark();

        // Load font
        if (!m_config.defaultFontPath.empty())
        {
            io.Fonts->AddFontFromFileTTF(m_config.defaultFontPath.c_str(), m_config.defaultFontSize);
        }

        // Initialize the Win32 platform layer
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

        // Close the Win32 platform layer
        ImGui_ImplWin32_Shutdown();

        // Destroy ImGui context
        ImGui::DestroyContext();

        m_imguiContextInitialized = false;
        DebuggerPrintf("[ImGuiSubsystem] ImGui context shutdown\n");
    }

    //==============================================================================
    // Internal method - backend management
    //==============================================================================

    bool ImGuiSubsystem::CreateBackend()
    {
        DebuggerPrintf("[ImGuiSubsystem] Creating backend via factory method...\n");

        // Use Context's factory method to create Backend (factory method pattern)
        m_backend = m_config.renderContext->CreateBackend();

        if (!m_backend)
        {
            DebuggerPrintf("[ImGuiSubsystem] Error: Failed to create backend\n");
            return false;
        }

        // Initialize backend
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
