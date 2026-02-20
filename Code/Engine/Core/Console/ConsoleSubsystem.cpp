#include "ConsoleSubsystem.hpp"
#include "Engine/Core/Console/Imgui/ImguiConsole.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/MessageLog/MessageLogSubsystem.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_internal.h"
#include <cstdarg>
#include <cstdio>

#include "Game/GameCommon.hpp"

// Global console instance (defined in global scope, like other engine globals)
enigma::core::ConsoleSubsystem* g_theConsole = nullptr;

namespace enigma::core
{
    ConsoleSubsystem::ConsoleSubsystem()
        : ConsoleSubsystem(ConsoleConfig::LoadFromYaml())
    {
    }

    ConsoleSubsystem::ConsoleSubsystem(const ConsoleConfig& config)
        : m_config(config)
    {
        ::g_theConsole = this;

        // Initialize external console immediately if needed for early output
        if (m_config.enableExternalConsole)
        {
            CreateExternalConsole();
        }
    }

    ConsoleSubsystem::~ConsoleSubsystem()
    {
        Shutdown();
        ::g_theConsole = nullptr;
    }

    void ConsoleSubsystem::Initialize()
    {
        if (m_initialized)
        {
            return;
        }

        // External console already created in constructor for early output
        m_initialized = true;
    }

    void ConsoleSubsystem::Startup()
    {
        if (!m_initialized)
        {
            Initialize();
        }

        // Bind to InputSystem delegates (replaces old SubscribeEventCallbackFunction)
        BindDelegates();

        // Create ImGui Console if enabled
        if (m_config.enableImguiConsole)
        {
            m_imguiConsole = std::make_unique<ImguiConsole>(m_config);
        }

        if (m_config.enableExternalConsole && m_externalConsole)
        {
            if (m_config.startupVisible)
            {
                SetVisible(true);
            }

            WriteLine("Eurekiel Engine Console", LogLevel::INFO);
        }
    }

    void ConsoleSubsystem::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        UnbindDelegates();
        m_imguiConsole.reset();
        m_externalConsole.reset();

        m_initialized = false;
    }

    void ConsoleSubsystem::Update(float deltaTime)
    {
        (void)deltaTime;

        if (!m_imguiConsole)
        {
            // External console only - process input
            if (m_externalConsole && m_externalConsole->IsVisible() && m_externalConsole->HasFocus())
            {
                m_externalConsole->ProcessConsoleInput();
            }
            return;
        }

        // Detect whether MessageLogUI is open
        auto* msgLogSub = GEngine ? GEngine->GetSubsystem<MessageLogSubsystem>() : nullptr;
        bool  msgLogOpen = msgLogSub && msgLogSub->GetUI() && msgLogSub->GetUI()->IsWindowOpen();

        if (msgLogOpen)
        {
            // Docked mode: Console docks below MessageLogUI
            m_imguiConsole->SetMode(ConsoleMode::Docked);

            if (!m_dockLayoutInitialized)
            {
                SetupDockLayout();
            }
        }
        else
        {
            // Transition from Docked -> Hidden: reset dock layout
            if (m_imguiConsole->GetMode() == ConsoleMode::Docked)
            {
                m_imguiConsole->SetMode(ConsoleMode::Hidden);
            }
            m_dockLayoutInitialized = false;

            // "/" key cycles: Hidden -> Terminal -> Full -> Hidden
            if (g_theInput)
            {
                unsigned char toggleKey = static_cast<unsigned char>(m_config.imguiToggleKey);
                if (g_theInput->WasKeyJustPressed(toggleKey))
                {
                    CycleTerminalMode();
                }
            }
        }

        // Render ImGui Console
        m_imguiConsole->Render();

        // Process external console input
        if (m_externalConsole && m_externalConsole->IsVisible() && m_externalConsole->HasFocus())
        {
            m_externalConsole->ProcessConsoleInput();
        }
    }

    //=========================================================================
    // Delegate binding (replaces old Subscribe/Unsubscribe pattern)
    //=========================================================================
    void ConsoleSubsystem::BindDelegates()
    {
        if (g_theInput)
        {
            m_keyPressedHandle = g_theInput->OnKeyPressed.Add(this, &ConsoleSubsystem::OnKeyPressed);
            m_charInputHandle  = g_theInput->OnCharInput.Add(this, &ConsoleSubsystem::OnCharInput);
        }
    }

    void ConsoleSubsystem::UnbindDelegates()
    {
        if (g_theInput)
        {
            g_theInput->OnKeyPressed.Remove(m_keyPressedHandle);
            g_theInput->OnCharInput.Remove(m_charInputHandle);
        }
        m_keyPressedHandle = 0;
        m_charInputHandle  = 0;
    }

    //=========================================================================
    // Member function event handlers (replacing old static Event_* functions)
    //=========================================================================
    void ConsoleSubsystem::OnKeyPressed(unsigned char keyCode)
    {
        if (!m_initialized)
        {
            return;
        }

        // Backslash toggles external console visibility
        if (keyCode == 92 && m_config.enableExternalConsole)
        {
            if (m_externalConsole)
            {
                m_externalConsole->Show();
                m_isVisible = true;
            }
            return;
        }

        // ESC clears external console input (does not consume)
        if (keyCode == KEYCODE_ESC)
        {
            if (IsVisible())
            {
                HandleEscape();
            }
            return;
        }

        // Only handle input if external console is visible and has focus
        if (!IsVisible())
        {
            return;
        }

        bool hasExternalFocus = m_externalConsole && m_externalConsole->HasFocus();
        if (!hasExternalFocus)
        {
            return;
        }

        switch (keyCode)
        {
        case KEYCODE_ENTER:
            HandleEnter();
            break;
        case KEYCODE_BACKSPACE:
            HandleBackspace();
            break;
        case KEYCODE_UPARROW:
        case KEYCODE_DOWNARROW:
        case KEYCODE_LEFTARROW:
        case KEYCODE_RIGHTARROW:
            HandleArrowKeys(keyCode);
            break;
        default:
            break;
        }
    }

    void ConsoleSubsystem::OnCharInput(unsigned char character)
    {
        if (!IsVisible())
        {
            return;
        }

        bool hasExternalFocus = m_externalConsole && m_externalConsole->HasFocus();
        if (!hasExternalFocus)
        {
            return;
        }

        if (character >= 32 && character <= 126)
        {
            ProcessCharInput(character);
        }
    }

    //=========================================================================
    // Command execution (sole authority - DevConsole is deprecated)
    //=========================================================================
    void ConsoleSubsystem::Execute(const std::string& command, bool echoCommand)
    {
        if (command.empty())
        {
            return;
        }

        // Echo the command to all backends
        if (echoCommand)
        {
            std::string echoText = m_config.commandPrefix + " " + command;
            ConsoleMessage echoMsg(echoText, LogLevel::INFO, Rgba8::CYAN);
            OnConsoleOutput.Broadcast(echoMsg);

            if (m_externalConsole)
            {
                m_externalConsole->WriteColored(echoText + "\n", Rgba8::CYAN);
            }
            if (m_imguiConsole)
            {
                m_imguiConsole->AddConsoleMessage(echoMsg);
            }
        }

        // Execute command via engine event system
        m_isExecutingCommand = true;
        EventArgs args;
        args.SetValue("Command", command);
        FireEvent("ExecuteConsoleCommand", args);

        // Broadcast to delegate listeners
        OnCommandExecuted.Broadcast(command);
        m_isExecutingCommand = false;
    }

    void ConsoleSubsystem::RegisterCommand(const std::string& name, const std::string& description)
    {
        // Avoid duplicates
        for (const auto& cmd : m_registeredCommands)
        {
            if (cmd == name)
            {
                return;
            }
        }
        m_registeredCommands.push_back(name);
        if (!description.empty())
        {
            m_commandDescriptions[name] = description;
        }
    }

    const std::vector<std::string>& ConsoleSubsystem::GetRegisteredCommands() const
    {
        return m_registeredCommands;
    }

    const std::string& ConsoleSubsystem::GetCommandDescription(const std::string& name) const
    {
        static const std::string s_empty;
        auto it = m_commandDescriptions.find(name);
        if (it != m_commandDescriptions.end())
        {
            return it->second;
        }
        return s_empty;
    }

    //=========================================================================
    // ImGui Console control
    //=========================================================================
    void ConsoleSubsystem::ToggleImguiConsole()
    {
        if (m_imguiConsole)
        {
            CycleTerminalMode();
        }
    }

    bool ConsoleSubsystem::IsImguiConsoleVisible() const
    {
        return m_imguiConsole && m_imguiConsole->IsVisible();
    }

    //=========================================================================
    // Output interface (broadcasts to all backends)
    //=========================================================================
    void ConsoleSubsystem::WriteLine(const std::string& text, LogLevel level)
    {
        if (!m_initialized)
        {
            return;
        }

        // Check verbosity level
        if (static_cast<int>(level) > static_cast<int>(m_config.verbosityLevel))
        {
            return;
        }

        // Determine color from log level
        Rgba8 color = Rgba8::WHITE;
        switch (level)
        {
        case LogLevel::ERROR_: color = Rgba8::RED;
            break;
        case LogLevel::WARNING: color = Rgba8::YELLOW;
            break;
        case LogLevel::INFO: color = Rgba8::WHITE;
            break;
        case LogLevel::DEBUG: color = Rgba8::GRAY;
            break;
        case LogLevel::TRACE: color = Rgba8::GRAY;
            break;
        }

        // Create message for delegate broadcast and ImGui Console
        ConsoleMessage msg(text, level, color);

        // Broadcast to delegate listeners
        OnConsoleOutput.Broadcast(msg);

        // Forward to ImGui Console
        if (m_imguiConsole)
        {
            if (m_isExecutingCommand)
            {
                // During command execution: route to console-specific buffer (+ general)
                m_imguiConsole->AddConsoleMessage(msg);
            }
            else
            {
                // General log output: only to general buffer
                m_imguiConsole->AddMessage(msg);
            }
        }

        // Forward to External Console
        if (m_externalConsole)
        {
            if (m_config.enableAnsiColors)
            {
                m_externalConsole->WriteColored(text, color);
            }
            else
            {
                m_externalConsole->WriteLine(text);
            }
        }

        // Forward to Logger system
        if (m_config.forwardToLogger)
        {
            switch (level)
            {
            case LogLevel::ERROR_:  enigma::core::LogError(LogConsole, text.c_str()); break;
            case LogLevel::WARNING: enigma::core::LogWarn(LogConsole, text.c_str());  break;
            default:                enigma::core::LogInfo(LogConsole, text.c_str());  break;
            }
        }
    }

    void ConsoleSubsystem::WriteLineColored(const std::string& text, const Rgba8& color)
    {
        if (!m_initialized)
        {
            return;
        }

        // Create message for delegate broadcast and ImGui Console
        ConsoleMessage msg(text, LogLevel::INFO, color);
        OnConsoleOutput.Broadcast(msg);

        if (m_imguiConsole)
        {
            m_imguiConsole->AddMessage(msg);
        }

        if (m_externalConsole)
        {
            m_externalConsole->WriteColored(text + "\n", color);
        }
    }

    void ConsoleSubsystem::WriteFormatted(LogLevel level, const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);

        va_end(args);

        WriteLine(std::string(buffer), level);
    }

    //=========================================================================
    // External Console control
    //=========================================================================
    void ConsoleSubsystem::SetVisible(bool visible)
    {
        if (!m_externalConsole)
        {
            return;
        }

        if (visible)
        {
            m_externalConsole->Show();
            Sleep(100);
        }
        else
        {
            m_externalConsole->Hide();
        }

        m_isVisible = visible;
    }

    bool ConsoleSubsystem::IsVisible() const
    {
        return m_isVisible && m_externalConsole && m_externalConsole->IsVisible();
    }

    bool ConsoleSubsystem::IsExternalConsoleActive() const
    {
        return m_externalConsole && m_externalConsole->IsVisible();
    }

    void ConsoleSubsystem::CreateExternalConsole()
    {
        m_externalConsole = std::make_unique<ExternalConsole>(m_config);

        if (!m_externalConsole->Initialize())
        {
            m_externalConsole.reset();
        }
    }

    //=========================================================================
    // DockBuilder layout: split MessageLog node, dock Console below it
    //=========================================================================
    void ConsoleSubsystem::CycleTerminalMode()
    {
        ConsoleMode current = m_imguiConsole->GetMode();
        switch (current)
        {
        case ConsoleMode::Hidden:
            m_imguiConsole->SetMode(ConsoleMode::Terminal);
            break;
        case ConsoleMode::Terminal:
            m_imguiConsole->SetMode(ConsoleMode::Full);
            break;
        case ConsoleMode::Full:
        default:
            m_imguiConsole->SetMode(ConsoleMode::Hidden);
            break;
        }
    }

    void ConsoleSubsystem::SetupDockLayout()
    {
        ImGuiWindow* msgLogWin = ImGui::FindWindowByName("MessageLog");
        if (!msgLogWin)
        {
            return;
        }

        // Get MessageLog's current dock ID, or create a new dock node
        ImGuiID dockId = msgLogWin->DockId;
        if (dockId == 0)
        {
            // MessageLog is not docked yet - create a new dock node
            dockId = ImGui::DockBuilderAddNode(0, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodeSize(dockId, msgLogWin->Size);
            ImGui::DockBuilderSetNodePos(dockId, msgLogWin->Pos);
        }

        // Split: bottom portion for Console (input line height ratio)
        float consoleRatio = 0.07f;
        ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Down, consoleRatio,
                                     &m_dockBottomId, &m_dockTopId);

        // Console node: no undock, no further splitting, auto-hide tab bar, no resize
        ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(m_dockBottomId);
        if (bottomNode)
        {
            bottomNode->LocalFlags |= ImGuiDockNodeFlags_NoUndocking
                                    | ImGuiDockNodeFlags_NoDockingSplit
                                    | ImGuiDockNodeFlags_AutoHideTabBar
                                    | ImGuiDockNodeFlags_NoResize;
        }

        // Dock windows to their respective nodes
        ImGui::DockBuilderDockWindow("MessageLog", m_dockTopId);
        ImGui::DockBuilderDockWindow("Console", m_dockBottomId);
        ImGui::DockBuilderFinish(dockId);

        m_dockLayoutInitialized = true;
    }

    //=========================================================================
    // External console input processing
    //=========================================================================
    void ConsoleSubsystem::ProcessCharInput(unsigned char character)
    {
        if (character >= 32 && character <= 126)
        {
            m_currentInput.insert(m_cursorPosition, 1, static_cast<char>(character));
            m_cursorPosition++;
            UpdateInputDisplay();
        }
    }

    void ConsoleSubsystem::HandleBackspace()
    {
        if (m_cursorPosition > 0)
        {
            m_currentInput.erase(m_cursorPosition - 1, 1);
            m_cursorPosition--;
            UpdateInputDisplay();
        }
    }

    void ConsoleSubsystem::HandleEnter()
    {
        if (!m_currentInput.empty())
        {
            // Add to history
            m_commandHistory.push_back(m_currentInput);
            if (m_commandHistory.size() > 1000)
            {
                m_commandHistory.erase(m_commandHistory.begin());
            }

            // Execute command through ConsoleSubsystem (sole authority)
            Execute(m_currentInput);
        }

        // Reset input state
        m_currentInput.clear();
        m_cursorPosition = 0;
        m_historyIndex   = -1;
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandleEscape()
    {
        m_currentInput.clear();
        m_cursorPosition = 0;
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandleArrowKeys(unsigned char keyCode)
    {
        switch (keyCode)
        {
        case KEYCODE_LEFTARROW:
            if (m_cursorPosition > 0)
            {
                m_cursorPosition--;
                UpdateInputDisplay();
            }
            break;

        case KEYCODE_RIGHTARROW:
            if (m_cursorPosition < static_cast<int>(m_currentInput.length()))
            {
                m_cursorPosition++;
                UpdateInputDisplay();
            }
            break;

        case KEYCODE_UPARROW:
            HandleUpArrow();
            break;

        case KEYCODE_DOWNARROW:
            HandleDownArrow();
            break;
        }
    }

    void ConsoleSubsystem::HandleUpArrow()
    {
        if (m_commandHistory.empty())
        {
            return;
        }

        if (m_historyIndex < 0)
        {
            m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
        }
        else if (m_historyIndex > 0)
        {
            m_historyIndex--;
        }

        m_currentInput   = m_commandHistory[m_historyIndex];
        m_cursorPosition = static_cast<int>(m_currentInput.length());
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandleDownArrow()
    {
        if (m_historyIndex < 0)
        {
            return;
        }

        m_historyIndex++;
        if (m_historyIndex >= static_cast<int>(m_commandHistory.size()))
        {
            m_historyIndex = -1;
            m_currentInput.clear();
            m_cursorPosition = 0;
        }
        else
        {
            m_currentInput   = m_commandHistory[m_historyIndex];
            m_cursorPosition = static_cast<int>(m_currentInput.length());
        }
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandlePaste()
    {
#ifdef _WIN32
        if (!OpenClipboard(nullptr))
        {
            return;
        }

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData)
        {
            const char* pszText = static_cast<const char*>(GlobalLock(hData));
            if (pszText)
            {
                std::string pasteText(pszText);
                GlobalUnlock(hData);

                // Insert at cursor position
                m_currentInput.insert(m_cursorPosition, pasteText);
                m_cursorPosition += static_cast<int>(pasteText.length());
                UpdateInputDisplay();
            }
        }

        CloseClipboard();
#endif
    }

    void ConsoleSubsystem::UpdateInputDisplay()
    {
        if (!m_externalConsole)
        {
            return;
        }

        m_externalConsole->UpdateInputLine(m_currentInput, m_cursorPosition);
    }
} // namespace enigma::core
