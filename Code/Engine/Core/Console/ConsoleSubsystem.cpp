#include "ConsoleSubsystem.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/Console/DevConsole.hpp"
#include <cstdarg>
#include <cstdio>

// Global console instance (defined in global scope, like other engine globals)
enigma::core::ConsoleSubsystem* g_theConsole = nullptr;

namespace enigma::core {

    ConsoleSubsystem::ConsoleSubsystem()
        : ConsoleSubsystem(ConsoleConfig::LoadFromYaml()) {
    }

    ConsoleSubsystem::ConsoleSubsystem(const ConsoleConfig& config)
        : m_config(config) {
        ::g_theConsole = this;
        
        // Initialize external console immediately if needed for early output
        if (m_config.enableExternalConsole) {
            CreateExternalConsole();
        }
    }

    ConsoleSubsystem::~ConsoleSubsystem() {
        Shutdown();
        ::g_theConsole = nullptr;
    }

    void ConsoleSubsystem::Initialize() {
        if (m_initialized) {
            return;
        }

        // External console already created in constructor for early output
        // Just mark as initialized
        m_initialized = true;
    }

    void ConsoleSubsystem::Startup() {
        if (!m_initialized) {
            Initialize();
        }

        RegisterEventHandlers();
        
        if (m_config.enableExternalConsole && m_externalConsole) {
            if (m_config.startupVisible) {
                SetVisible(true);
            }
            
            // Show startup message directly
            WriteLine("Eurekiel Engine External Console", LogLevel::INFO);
            WriteLine("Input is forwarded to DevConsole for command execution", LogLevel::INFO);
        }
    }

    void ConsoleSubsystem::Shutdown() {
        if (!m_initialized) {
            return;
        }

        UnregisterEventHandlers();
        m_externalConsole.reset();
        
        m_initialized = false;
    }
    
    void ConsoleSubsystem::Update(float deltaTime) {
        (void)deltaTime; // Suppress unused parameter warning
        
        // Process direct console input when external console is active
        if (m_externalConsole && m_externalConsole->IsVisible()) {
            // Only process direct input if external console has focus
            if (m_externalConsole->HasFocus()) {
                m_externalConsole->ProcessConsoleInput();
            }
        }
    }

    void ConsoleSubsystem::WriteLine(const std::string& text, LogLevel level) {
        if (!m_initialized || !m_externalConsole) {
            return;
        }

        // Check verbosity level
        if (static_cast<int>(level) > static_cast<int>(m_config.verbosityLevel)) {
            return;
        }

        // Create message with appropriate color and write directly
        Rgba8 color = Rgba8::WHITE;
        switch (level) {
            case LogLevel::ERROR_:   color = Rgba8::RED; break;
            case LogLevel::WARNING: color = Rgba8::YELLOW; break;
            case LogLevel::INFO:    color = Rgba8::WHITE; break;
            case LogLevel::DEBUG:   color = Rgba8::GRAY; break;
            case LogLevel::TRACE:   color = Rgba8::GRAY; break;
        }

        // Direct output - no threading
        if (m_config.enableAnsiColors) {
            m_externalConsole->WriteColored(text, color);
        } else {
            m_externalConsole->WriteLine(text);
        }
    }

    void ConsoleSubsystem::WriteLineColored(const std::string& text, const Rgba8& color) {
        if (!m_initialized || !m_externalConsole) {
            return;
        }

        // Direct output - no threading, add newline for WriteLine
        m_externalConsole->WriteColored(text + "\n", color);
    }

    void ConsoleSubsystem::WriteFormatted(LogLevel level, const char* format, ...) {
        va_list args;
        va_start(args, format);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        
        va_end(args);
        
        WriteLine(std::string(buffer), level);
    }

    // Forward command to DevConsole instead of handling internally
    void ConsoleSubsystem::ForwardCommandToDevConsole(const std::string& command) {
        // Get global DevConsole and execute command there
        // Use global declaration from EngineCommon.hpp
        if (::g_theDevConsole) {
            // Execute in DevConsole (this handles all the actual command processing)  
            // Don't echo here - HandleEnter() already echoed the command
            ::g_theDevConsole->Execute(command, false); // false = don't echo in DevConsole either
        }
    }

    bool ConsoleSubsystem::Event_ConsoleKeyPressed(EventArgs& args) {
        auto* console = ::g_theConsole;
        if (!console || !console->m_initialized) {
            return false;
        }
        
        unsigned char keyCode = static_cast<unsigned char>(args.GetValue("KeyCode", -1));
        
        // Handle different toggle keys for different consoles
        if (keyCode == KEYCODE_TILDE) {
            // Tilde toggles DevConsole, don't consume for external console
            return false;  // Let DevConsole handle it
        }
        
        // Backslash toggles external console (ASCII 92 = '\')
        if (keyCode == 92 && console->m_config.enableExternalConsole) {
            // Show and bring to front, don't change input focus
            if (console->m_externalConsole) {
                console->m_externalConsole->Show();
                console->m_isVisible = true;
            }
            return true;  // Consume the event
        }
        
        // ESC should not be consumed by external console - let App handle it
        if (keyCode == KEYCODE_ESC) {
            // If external console is visible, clear input but don't consume ESC
            if (console->IsVisible()) {
                console->HandleEscape();
            }
            return false;  // Don't consume ESC - let App.cpp handle it
        }
        
        // Only handle input if external console is visible AND has focus
        if (!console->IsVisible()) {
            return false;
        }
        
        // Check if external console has focus
        bool hasExternalFocus = console->m_externalConsole && console->m_externalConsole->HasFocus();
        if (!hasExternalFocus) {
            return false;  // Let DevConsole or other systems handle
        }
        
        // Handle input keys for external console when it has focus
        switch (keyCode) {
            case KEYCODE_ENTER:
                console->HandleEnter();
                return true;
                
            case KEYCODE_BACKSPACE:
                console->HandleBackspace();
                return true;
                
            case KEYCODE_UPARROW:
            case KEYCODE_DOWNARROW:
            case KEYCODE_LEFTARROW:
            case KEYCODE_RIGHTARROW:
                console->HandleArrowKeys(keyCode);
                return true;
                
            default:
                return false;  // Let character input event handle
        }
    }

    bool ConsoleSubsystem::Event_ConsoleCharInput(EventArgs& args) {
        auto* console = ::g_theConsole;
        if (!console || !console->IsVisible()) {
            return false;
        }
        
        // Check if external console has focus
        bool hasExternalFocus = console->m_externalConsole && console->m_externalConsole->HasFocus();
        if (!hasExternalFocus) {
            return false;  // Let DevConsole handle
        }
        
        // Window sends character as "KeyCode", not "CharCode"
        unsigned char character = static_cast<unsigned char>(args.GetValue("KeyCode", 0));
        if (character >= 32 && character <= 126) {  // Printable characters
            console->ProcessCharInput(character);
            return true;
        }
        
        return false;
    }

    bool ConsoleSubsystem::Event_ConsoleWindowClose(EventArgs& args) {
        (void)args; // Suppress unused parameter warning
        // Console window close requested, shut down application
        if (::g_theConsole) {
            ::g_theConsole->WriteLine("Console window close requested, shutting down application", LogLevel::INFO);
        }
        FireEvent("ApplicationQuitEvent");
        return true;
    }
    
    bool ConsoleSubsystem::Event_ConsoleDirectChar(EventArgs& args) {
        auto* console = ::g_theConsole;
        if (!console || !console->IsVisible()) {
            return false;
        }
        
        // Only process if external console is visible (direct input only works when focused)
        char character = static_cast<char>(args.GetValue("Character", 0));
        if (character >= 32 && character <= 126) {
            console->ProcessCharInput(static_cast<unsigned char>(character));
            return true;
        }
        
        return false;
    }
    
    bool ConsoleSubsystem::Event_ConsoleDirectEnter(EventArgs& args) {
        (void)args; // Suppress unused parameter warning
        auto* console = ::g_theConsole;
        if (!console || !console->IsVisible()) {
            return false;
        }
        
        console->HandleEnter();
        return true;
    }
    
    bool ConsoleSubsystem::Event_ConsoleDirectBackspace(EventArgs& args) {
        (void)args; // Suppress unused parameter warning
        auto* console = ::g_theConsole;
        if (!console || !console->IsVisible()) {
            return false;
        }
        
        console->HandleBackspace();
        return true;
    }
    
    bool ConsoleSubsystem::Event_DevConsoleOutput(EventArgs& args) {
        auto* console = ::g_theConsole;
        if (!console || !console->IsExternalConsoleActive()) {
            return false; // Don't consume if external console isn't active
        }
        
        // Extract text and color from DevConsole output
        std::string text = args.GetValue("Text", "");
        int r = args.GetValue("ColorR", 255);
        int g = args.GetValue("ColorG", 255);
        int b = args.GetValue("ColorB", 255);
        int a = args.GetValue("ColorA", 255);
        
        Rgba8 color(static_cast<unsigned char>(r), static_cast<unsigned char>(g), 
                   static_cast<unsigned char>(b), static_cast<unsigned char>(a));
        
        // Mirror to External Console
        console->WriteLineColored(text, color);
        
        return false; // Don't consume - let other systems handle it too
    }


    void ConsoleSubsystem::SetVisible(bool visible) {
        if (!m_externalConsole) {
            return;
        }

        if (visible) {
            m_externalConsole->Show();
            // Give console time to appear and gain focus
            Sleep(100);
            // UpdateFocusState(); // Removed this method
            // Focus state tracking removed
        } else {
            m_externalConsole->Hide();
            // Focus state tracking removed
        }
        
        m_isVisible = visible;
    }

    bool ConsoleSubsystem::IsVisible() const {
        return m_isVisible && m_externalConsole && m_externalConsole->IsVisible();
    }

    bool ConsoleSubsystem::IsExternalConsoleActive() const {
        return m_externalConsole && m_externalConsole->IsVisible();
    }

    void ConsoleSubsystem::CreateExternalConsole() {
        m_externalConsole = std::make_unique<ExternalConsole>(m_config);
        
        if (!m_externalConsole->Initialize()) {
            m_externalConsole.reset();
        }
    }

    void ConsoleSubsystem::RegisterEventHandlers() {
        // Register to InputSystem events
        SubscribeEventCallbackFunction("KeyPressed", Event_ConsoleKeyPressed);
        SubscribeEventCallbackFunction("CharInput", Event_ConsoleCharInput);
        
        // Register Console specific events
        SubscribeEventCallbackFunction("ConsoleWindowClose", Event_ConsoleWindowClose);
        
        // Register direct console input events (for when InputSystem can't reach us)
        SubscribeEventCallbackFunction("ConsoleDirectChar", Event_ConsoleDirectChar);
        SubscribeEventCallbackFunction("ConsoleDirectEnter", Event_ConsoleDirectEnter);
        SubscribeEventCallbackFunction("ConsoleDirectBackspace", Event_ConsoleDirectBackspace);
        
        // Register DevConsole output event for mirroring output to External Console
        SubscribeEventCallbackFunction("DevConsoleOutput", Event_DevConsoleOutput);
    }

    void ConsoleSubsystem::UnregisterEventHandlers() {
        // Unregister event subscriptions
        UnsubscribeEventCallbackFunction("KeyPressed", Event_ConsoleKeyPressed);
        UnsubscribeEventCallbackFunction("CharInput", Event_ConsoleCharInput);
        UnsubscribeEventCallbackFunction("ConsoleWindowClose", Event_ConsoleWindowClose);
        
        // Unregister direct console input events
        UnsubscribeEventCallbackFunction("ConsoleDirectChar", Event_ConsoleDirectChar);
        UnsubscribeEventCallbackFunction("ConsoleDirectEnter", Event_ConsoleDirectEnter);
        UnsubscribeEventCallbackFunction("ConsoleDirectBackspace", Event_ConsoleDirectBackspace);
        
        // Unregister DevConsole output event
        UnsubscribeEventCallbackFunction("DevConsoleOutput", Event_DevConsoleOutput);
    }


    void ConsoleSubsystem::ProcessCharInput(unsigned char character) {
        if (character >= 32 && character <= 126) {
            // Insert character at cursor position
            m_currentInput.insert(m_cursorPosition, 1, static_cast<char>(character));
            m_cursorPosition++;
            UpdateInputDisplay();
        }
    }

    void ConsoleSubsystem::HandleBackspace() {
        if (m_cursorPosition > 0) {
            m_currentInput.erase(m_cursorPosition - 1, 1);
            m_cursorPosition--;
            UpdateInputDisplay();
        }
    }

    void ConsoleSubsystem::HandleEnter() {
        if (!m_currentInput.empty()) {
            // Echo the command in external console
            WriteLineColored("> " + m_currentInput, Rgba8::CYAN);
            
            // Forward command to DevConsole for execution
            ForwardCommandToDevConsole(m_currentInput);
        }
        
        m_currentInput.clear();
        m_cursorPosition = 0;
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandleEscape() {
        m_currentInput.clear();
        m_cursorPosition = 0;
        UpdateInputDisplay();
    }

    void ConsoleSubsystem::HandleArrowKeys(unsigned char keyCode) {
        switch (keyCode) {
            case KEYCODE_LEFTARROW:
                if (m_cursorPosition > 0) {
                    m_cursorPosition--;
                    UpdateInputDisplay();
                }
                break;
                
            case KEYCODE_RIGHTARROW:
                if (m_cursorPosition < static_cast<int>(m_currentInput.length())) {
                    m_cursorPosition++;
                    UpdateInputDisplay();
                }
                break;
                
            case KEYCODE_UPARROW:
            case KEYCODE_DOWNARROW:
                // History is handled by DevConsole, not here
                break;
        }
    }



    void ConsoleSubsystem::UpdateInputDisplay() {
        if (m_externalConsole) {
            m_externalConsole->UpdateInputLine(m_currentInput, m_cursorPosition);
        }
    }

} // namespace enigma::core