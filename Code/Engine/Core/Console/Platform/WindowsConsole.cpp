#include "WindowsConsole.hpp"
#include "Engine/Core/Event/EventSubsystem.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include <iostream>
#include <io.h>
#include <fcntl.h>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::core
{
    WindowsConsole* WindowsConsole::s_instance = nullptr;

    WindowsConsole::WindowsConsole(const ConsoleConfig& config)
        : m_config(config)
    {
        s_instance = this;
    }

    WindowsConsole::~WindowsConsole()
    {
        Shutdown();
        s_instance = nullptr;
    }

    bool WindowsConsole::Initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        // Check if console already exists 
        m_hadConsole = (GetConsoleWindow() != nullptr);
        OutputDebugStringA("WindowsConsole::Initialize - checking console status\n");
        char statusStr[128];
        sprintf_s(statusStr, sizeof(statusStr), "Had console: %s\n", m_hadConsole ? "true" : "false");
        OutputDebugStringA(statusStr);

        if (!InitializeWindowsConsole())
        {
            return false;
        }

        if (!EnableVirtualTerminalProcessing())
        {
            m_supportsAnsi = false;
        }
        else
        {
            m_supportsAnsi = true;
        }

        ConfigureConsoleMode();
        SetupConsoleCloseHandler();

        if (!m_config.windowTitle.empty())
        {
            SetTitle(m_config.windowTitle);
        }

        SetSize(m_config.windowWidth, m_config.windowHeight);

        // Input handling is done through InputSystem

        m_initialized = true;
        m_isVisible   = m_config.startupVisible;

        if (!m_isVisible)
        {
            Hide();
        }

        return true;
    }

    void WindowsConsole::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        // Input thread not used - input handled by InputSystem

        // Restore original console modes
        if (m_consoleInput != INVALID_HANDLE_VALUE)
        {
            SetConsoleMode(m_consoleInput, m_originalInputMode);
        }
        if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            SetConsoleMode(m_consoleOutput, m_originalOutputMode);
        }

        ShutdownWindowsConsole();
        m_initialized = false;
    }

    bool WindowsConsole::Show()
    {
        if (!m_initialized || !m_consoleWindow)
        {
            return false;
        }

        ShowWindow(m_consoleWindow, SW_SHOW);
        SetForegroundWindow(m_consoleWindow);
        SetFocus(m_consoleWindow);

        // Show welcome message and prompt when first shown
        if (m_ownedConsole)
        {
            WriteLine("=== Eurekiel Engine External Console ===");
            WriteColored("Commands entered here are forwarded to DevConsole for execution", Rgba8(128, 255, 128));
            WriteLine("");
            Write("> "); // Initial prompt
        }

        m_isVisible = true;
        return true;
    }

    bool WindowsConsole::Hide()
    {
        if (!m_initialized || !m_consoleWindow)
        {
            return false;
        }

        ShowWindow(m_consoleWindow, SW_HIDE);
        m_isVisible = false;
        return true;
    }

    bool WindowsConsole::HasFocus() const
    {
        if (!m_consoleWindow) return false;

        HWND foregroundWindow = GetForegroundWindow();
        return foregroundWindow == m_consoleWindow;
    }

    void WindowsConsole::Write(const std::string& text)
    {
        if (!m_initialized) return;

        // Determine where to output based on configuration and runtime environment
        bool outputToIDE      = false;
        bool outputToExternal = false;

        switch (m_config.outputMode)
        {
        case ConsoleOutputMode::AUTO:
            // Auto-detect: Debug build + debugger -> IDE, otherwise -> External
#ifdef _DEBUG
            {
                bool isDebuggerAttached = IsDebuggerPresent();
                outputToIDE             = isDebuggerAttached;
                outputToExternal        = !isDebuggerAttached;
            }
#else
            outputToExternal = true;
#endif
            break;

        case ConsoleOutputMode::IDE_ONLY:
            outputToIDE = true;
            break;

        case ConsoleOutputMode::EXTERNAL_ONLY:
            outputToExternal = true;
            break;

        case ConsoleOutputMode::BOTH:
            outputToIDE = true;
            outputToExternal = true;
            break;
        }

        // Output to IDE Console (Visual Studio Output window)
        if (outputToIDE)
        {
            OutputDebugStringA(text.c_str());
        }

        // Output to External Console
        if (outputToExternal && m_ownedConsole)
        {
            printf("%s", text.c_str());
            fflush(stdout);
        }
    }

    void WindowsConsole::WriteColored(const std::string& text, const Rgba8& color)
    {
        if (!m_initialized) return;

        // Determine output targets (same logic as Write)
        bool outputToIDE      = false;
        bool outputToExternal = false;

        switch (m_config.outputMode)
        {
        case ConsoleOutputMode::AUTO:
#ifdef _DEBUG
            {
                bool isDebuggerAttached = IsDebuggerPresent();
                outputToIDE             = isDebuggerAttached;
                outputToExternal        = !isDebuggerAttached;
            }
#else
            outputToExternal = true;
#endif
            break;
        case ConsoleOutputMode::IDE_ONLY:
            outputToIDE = true;
            break;
        case ConsoleOutputMode::EXTERNAL_ONLY:
            outputToExternal = true;
            break;
        case ConsoleOutputMode::BOTH:
            outputToIDE = true;
            outputToExternal = true;
            break;
        }

        // Output to IDE Console (no colors, just text)
        if (outputToIDE)
        {
            OutputDebugStringA(text.c_str());
        }

        // Output to External Console with colors
        if (outputToExternal && m_ownedConsole)
        {
            if (m_supportsAnsi)
            {
                // Use ANSI color codes
                std::string colorCode = "\033[38;2;" + std::to_string(color.r) + ";" +
                    std::to_string(color.g) + ";" + std::to_string(color.b) + "m";
                printf("%s%s\033[0m", colorCode.c_str(), text.c_str());
                fflush(stdout);
            }
            else if (m_consoleOutput != INVALID_HANDLE_VALUE)
            {
                // Windows console colors using handle
                CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
                GetConsoleScreenBufferInfo(m_consoleOutput, &consoleInfo);
                WORD originalAttributes = consoleInfo.wAttributes;

                WORD attributes = 0;
                if (color.r > 128) attributes |= FOREGROUND_RED;
                if (color.g > 128) attributes |= FOREGROUND_GREEN;
                if (color.b > 128) attributes |= FOREGROUND_BLUE;
                if (attributes == 0) attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

                SetConsoleTextAttribute(m_consoleOutput, attributes);
                printf("%s", text.c_str());
                fflush(stdout);
                SetConsoleTextAttribute(m_consoleOutput, originalAttributes);
            }
            else
            {
                printf("%s", text.c_str());
                fflush(stdout);
            }
        }
    }

    void WindowsConsole::WriteLine(const std::string& text)
    {
        // Use Write with newline - let console mode handle the formatting
        Write(text + "\n");
    }

    void WindowsConsole::Clear()
    {
        if (!m_initialized || !m_ownedConsole)
        {
            return;
        }

        if (m_supportsAnsi)
        {
            printf("\033[2J\033[H");
            fflush(stdout);
        }
        else if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
            GetConsoleScreenBufferInfo(m_consoleOutput, &consoleInfo);

            COORD coordScreen = {0, 0};
            DWORD charsWritten;
            DWORD consoleSize = consoleInfo.dwSize.X * consoleInfo.dwSize.Y;

            FillConsoleOutputCharacterA(m_consoleOutput, ' ', consoleSize, coordScreen, &charsWritten);
            FillConsoleOutputAttribute(m_consoleOutput, consoleInfo.wAttributes, consoleSize, coordScreen, &charsWritten);
            SetConsoleCursorPosition(m_consoleOutput, coordScreen);
        }
        m_currentLine = 0;
    }

    void WindowsConsole::SetCursorPosition(int x, int y)
    {
        if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            COORD coord = {static_cast<SHORT>(x), static_cast<SHORT>(y)};
            SetConsoleCursorPosition(m_consoleOutput, coord);
        }
    }

    void WindowsConsole::ShowCursor(bool show)
    {
        if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            CONSOLE_CURSOR_INFO cursorInfo;
            GetConsoleCursorInfo(m_consoleOutput, &cursorInfo);
            cursorInfo.bVisible = show;
            SetConsoleCursorInfo(m_consoleOutput, &cursorInfo);
        }
    }

    void WindowsConsole::UpdateInputLine(const std::string& input, int cursorPos)
    {
        // Get current cursor position
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        if (GetConsoleScreenBufferInfo(m_consoleOutput, &consoleInfo))
        {
            // Move to beginning of current line
            SetCursorPosition(0, consoleInfo.dwCursorPosition.Y);

            // Clear line and write prompt + input
            Write("> " + input);

            // Clear rest of line
            if (m_supportsAnsi)
            {
                Write("\033[K");
            }

            // Position cursor
            SetCursorPosition(2 + cursorPos, consoleInfo.dwCursorPosition.Y);
        }
    }

    void WindowsConsole::SetTitle(const std::string& title)
    {
        SetConsoleTitleA(title.c_str());
    }

    void WindowsConsole::SetSize(int columns, int rows)
    {
        if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            // Set a much larger buffer size to enable scrolling
            // Buffer should be larger than window to enable scrollback
            int bufferRows = (rows * 10 > 1000) ? (rows * 10) : 1000; // At least 10x window size or 1000 lines

            COORD bufferSize = {static_cast<SHORT>(columns), static_cast<SHORT>(bufferRows)};
            SetConsoleScreenBufferSize(m_consoleOutput, bufferSize);

            // Set window size (visible area)
            SMALL_RECT windowRect = {0, 0, static_cast<SHORT>(columns - 1), static_cast<SHORT>(rows - 1)};
            SetConsoleWindowInfo(m_consoleOutput, TRUE, &windowRect);

            OutputDebugStringA("WindowsConsole: Set console size with scrollback buffer\n");
        }
        m_maxLines = rows;
    }

    bool WindowsConsole::InitializeWindowsConsole()
    {
        // Check if we already have a console window
        m_hadConsole = (GetConsoleWindow() != nullptr);

        // Only allocate new console if requested and we don't already have one
        if (m_config.windows.allocateNewConsole && !m_hadConsole)
        {
            if (!AllocConsole())
            {
                DWORD error = GetLastError();
                OutputDebugStringA("Failed to allocate console. Error: ");
                char errorStr[32];
                sprintf_s(errorStr, sizeof(errorStr), "%lu\n", error);
                OutputDebugStringA(errorStr);
                return false;
            }
            m_ownedConsole = true;
            OutputDebugStringA("Successfully allocated new console\n");
        }
        else if (m_hadConsole)
        {
            OutputDebugStringA("Using existing console window\n");
        }
        else
        {
            OutputDebugStringA("Console allocation skipped (not requested)\n");
        }

        // Get console handles
        m_consoleInput  = GetStdHandle(STD_INPUT_HANDLE);
        m_consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        m_consoleWindow = GetConsoleWindow();

        if (m_consoleInput == INVALID_HANDLE_VALUE ||
            m_consoleOutput == INVALID_HANDLE_VALUE ||
            !m_consoleWindow)
        {
            OutputDebugStringA("Failed to get valid console handles\n");
            return false;
        }

        // Save original modes
        if (!GetConsoleMode(m_consoleInput, &m_originalInputMode))
        {
            DWORD error = GetLastError();
            char  errorStr[128];
            sprintf_s(errorStr, sizeof(errorStr), "Failed to get console input mode. Error: %lu\n", error);
            OutputDebugStringA(errorStr);
            // Don't fail for input mode - we can work without it
            m_originalInputMode = 0;
        }
        if (!GetConsoleMode(m_consoleOutput, &m_originalOutputMode))
        {
            DWORD error = GetLastError();
            char  errorStr[128];
            sprintf_s(errorStr, sizeof(errorStr), "Failed to get console output mode. Error: %lu\n", error);
            OutputDebugStringA(errorStr);
            // Don't fail for output mode - we can work without it
            m_originalOutputMode = 0;
        }

        // Configure stdio redirection based on output mode
        ConfigureStdioRedirection();

        return true;
    }

    void WindowsConsole::ShutdownWindowsConsole()
    {
        if (m_ownedConsole)
        {
            FreeConsole();
            m_ownedConsole = false;
        }

        m_consoleInput  = INVALID_HANDLE_VALUE;
        m_consoleOutput = INVALID_HANDLE_VALUE;
        m_consoleWindow = nullptr;
    }

    bool WindowsConsole::EnableVirtualTerminalProcessing()
    {
        if (m_consoleOutput == INVALID_HANDLE_VALUE)
        {
            OutputDebugStringA("EnableVirtualTerminalProcessing: Invalid console output handle\n");
            return false;
        }

        DWORD mode = 0;
        if (!GetConsoleMode(m_consoleOutput, &mode))
        {
            DWORD error = GetLastError();
            char  errorStr[128];
            sprintf_s(errorStr, sizeof(errorStr), "EnableVirtualTerminalProcessing: Failed to get console mode. Error: %lu\n", error);
            OutputDebugStringA(errorStr);
            return false;
        }

        DWORD newMode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
        if (SetConsoleMode(m_consoleOutput, newMode))
        {
            OutputDebugStringA("EnableVirtualTerminalProcessing: Successfully enabled VT processing\n");
            return true;
        }
        else
        {
            DWORD error = GetLastError();
            char  errorStr[128];
            sprintf_s(errorStr, sizeof(errorStr), "EnableVirtualTerminalProcessing: Failed to set console mode. Error: %lu\n", error);
            OutputDebugStringA(errorStr);
            return false;
        }
    }

    void WindowsConsole::ConfigureConsoleMode()
    {
        // Configure console input mode for External Console functionality
        if (m_consoleInput != INVALID_HANDLE_VALUE)
        {
            DWORD inputMode = 0;
            GetConsoleMode(m_consoleInput, &inputMode);

            // Configure for standard console behavior with InputSystem compatibility
            inputMode |= ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT;

            // Disable automatic echo but keep input processing for direct access
            inputMode &= ~ENABLE_ECHO_INPUT;
            inputMode &= ~ENABLE_LINE_INPUT;

            SetConsoleMode(m_consoleInput, inputMode);
            OutputDebugStringA("ConfigureConsoleMode: Console input configured\n");
        }

        // Configure console output mode for proper text handling
        if (m_consoleOutput != INVALID_HANDLE_VALUE)
        {
            DWORD outputMode = 0;
            GetConsoleMode(m_consoleOutput, &outputMode);

            // Enable proper text processing - this helps with newline handling
            outputMode |= ENABLE_PROCESSED_OUTPUT;
            outputMode |= ENABLE_WRAP_AT_EOL_OUTPUT;

            // Try to enable VT processing if available
            outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

            SetConsoleMode(m_consoleOutput, outputMode);
            OutputDebugStringA("ConfigureConsoleMode: Console output configured\n");
        }
    }

    bool WindowsConsole::SetupConsoleCloseHandler()
    {
        return SetConsoleCtrlHandler(ConsoleCloseHandler, TRUE);
    }

    void WindowsConsole::ConfigureStdioRedirection()
    {
        // Determine where stdout should go based on our intelligent output mode
        bool redirectToIDE = false;

        switch (m_config.outputMode)
        {
        case ConsoleOutputMode::AUTO:
#ifdef _DEBUG
            {
                bool isDebuggerAttached = IsDebuggerPresent();
                redirectToIDE           = isDebuggerAttached;
            }
#else
            redirectToIDE = false;
#endif
            break;
        case ConsoleOutputMode::IDE_ONLY:
            redirectToIDE = true;
            break;
        case ConsoleOutputMode::EXTERNAL_ONLY:
            redirectToIDE = false;
            break;
        case ConsoleOutputMode::BOTH:
            // For "both" mode, redirect to external console and rely on our Write() method
            // to duplicate to IDE via OutputDebugString
            redirectToIDE = false;
            break;
        }

        if (redirectToIDE)
        {
            // Don't redirect stdio - let printf go to IDE via default behavior
            // IDE will capture stdout automatically
            OutputDebugStringA("WindowsConsole: Stdout will go to IDE Console\n");
        }
        else if (m_ownedConsole && m_config.windows.redirectStdio)
        {
            // Redirect stdio to our external console
            FILE* pCout;
            FILE* pCerr;
            FILE* pCin;

            errno_t result;
            result = freopen_s(&pCout, "CONOUT$", "w", stdout);
            if (result == 0)
            {
                OutputDebugStringA("WindowsConsole: Successfully redirected stdout to external console\n");
            }

            result = freopen_s(&pCerr, "CONOUT$", "w", stderr);
            if (result == 0)
            {
                OutputDebugStringA("WindowsConsole: Successfully redirected stderr to external console\n");
            }

            result = freopen_s(&pCin, "CONIN$", "r", stdin);
            if (result == 0)
            {
                OutputDebugStringA("WindowsConsole: Successfully redirected stdin to external console\n");
            }

            std::ios::sync_with_stdio(true);
        }
    }

    bool WindowsConsole::HasPendingInput() const
    {
        if (m_consoleInput == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD numEvents = 0;
        if (GetNumberOfConsoleInputEvents(m_consoleInput, &numEvents))
        {
            return numEvents > 0;
        }
        return false;
    }

    void WindowsConsole::ProcessConsoleInput()
    {
        if (m_consoleInput == INVALID_HANDLE_VALUE)
        {
            return;
        }

        INPUT_RECORD inputRecords[128];
        DWORD        numRead = 0;

        if (!ReadConsoleInput(m_consoleInput, inputRecords, 128, &numRead))
        {
            return;
        }

        for (DWORD i = 0; i < numRead; i++)
        {
            const INPUT_RECORD& record = inputRecords[i];

            if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
            {
                const KEY_EVENT_RECORD& keyEvent = record.Event.KeyEvent;

                // Handle special keys
                if (keyEvent.wVirtualKeyCode == VK_RETURN)
                {
                    // Send enter event to ConsoleSubsystem
                    NamedStrings args;
                    FireEvent("ConsoleDirectEnter", args);
                }
                else if (keyEvent.wVirtualKeyCode == VK_BACK)
                {
                    // Send backspace event to ConsoleSubsystem  
                    NamedStrings args;
                    FireEvent("ConsoleDirectBackspace", args);
                }
                else if (keyEvent.wVirtualKeyCode == VK_UP)
                {
                    // Send up arrow event for history navigation
                    NamedStrings args;
                    FireEvent("ConsoleDirectUpArrow", args);
                }
                else if (keyEvent.wVirtualKeyCode == VK_DOWN)
                {
                    // Send down arrow event for history navigation
                    NamedStrings args;
                    FireEvent("ConsoleDirectDownArrow", args);
                }
                else if (keyEvent.wVirtualKeyCode == 'V' && (keyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)))
                {
                    // Send paste event (Ctrl+V)
                    NamedStrings args;
                    FireEvent("ConsoleDirectPaste", args);
                }
                else if (keyEvent.uChar.AsciiChar >= 32 && keyEvent.uChar.AsciiChar <= 126)
                {
                    // Send character event to ConsoleSubsystem
                    NamedStrings args;
                    args.SetValue("Character", std::to_string(static_cast<int>(keyEvent.uChar.AsciiChar)));
                    FireEvent("ConsoleDirectChar", args);
                }
            }
        }
    }

    BOOL WINAPI WindowsConsole::ConsoleCloseHandler(DWORD dwCtrlType)
    {
        switch (dwCtrlType)
        {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (s_instance)
            {
                s_instance->m_closeRequested.store(true);

                // Fire console close event
                NamedStrings args;
                FireEvent("ConsoleWindowClose", args);

                // If configured to close app, fire quit event
                if (s_instance->m_config.windows.closeAppOnConsoleClose)
                {
                    FireEvent("ApplicationQuitEvent", args);
                }
            }
            return TRUE;

        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            // Handle Ctrl+C/Break if needed
            return TRUE;

        default:
            return FALSE;
        }
    }
} // namespace enigma::core
