#include "ExternalConsole.hpp"

namespace enigma::core
{
    ExternalConsole::ExternalConsole(const ConsoleConfig& config)
        : m_config(config)
          , m_platformConsole(std::make_unique<WindowsConsole>(config))
    {
    }

    ExternalConsole::~ExternalConsole()
    {
        Shutdown();
    }

    bool ExternalConsole::Initialize()
    {
        if (!m_platformConsole)
        {
            return false;
        }

        return m_platformConsole->Initialize();
    }

    void ExternalConsole::Shutdown()
    {
        if (m_platformConsole)
        {
            m_platformConsole->Shutdown();
        }
    }

    bool ExternalConsole::Show()
    {
        return m_platformConsole ? m_platformConsole->Show() : false;
    }

    bool ExternalConsole::Hide()
    {
        return m_platformConsole ? m_platformConsole->Hide() : false;
    }

    bool ExternalConsole::IsVisible() const
    {
        return m_platformConsole ? m_platformConsole->IsVisible() : false;
    }

    bool ExternalConsole::HasFocus() const
    {
        return m_platformConsole ? m_platformConsole->HasFocus() : false;
    }

    void ExternalConsole::Write(const std::string& text)
    {
        if (m_platformConsole)
        {
            m_platformConsole->Write(text);
        }
    }

    void ExternalConsole::WriteColored(const std::string& text, const Rgba8& color)
    {
        if (m_platformConsole)
        {
            m_platformConsole->WriteColored(text, color);
        }
    }

    void ExternalConsole::WriteLine(const std::string& text)
    {
        if (m_platformConsole)
        {
            m_platformConsole->WriteLine(text);
        }
    }

    void ExternalConsole::Clear()
    {
        if (m_platformConsole)
        {
            m_platformConsole->Clear();
        }
    }

    void ExternalConsole::SetCursorPosition(int x, int y)
    {
        if (m_platformConsole)
        {
            m_platformConsole->SetCursorPosition(x, y);
        }
    }

    void ExternalConsole::ShowCursor(bool show)
    {
        if (m_platformConsole)
        {
            m_platformConsole->ShowCursor(show);
        }
    }

    void ExternalConsole::UpdateInputLine(const std::string& input, int cursorPos)
    {
        if (m_platformConsole)
        {
            m_platformConsole->UpdateInputLine(input, cursorPos);
        }
    }

    void ExternalConsole::SetTitle(const std::string& title)
    {
        if (m_platformConsole)
        {
            m_platformConsole->SetTitle(title);
        }
    }

    bool ExternalConsole::SupportsAnsiColors() const
    {
        return m_platformConsole ? m_platformConsole->SupportsAnsiColors() : false;
    }

    void ExternalConsole::SetSize(int columns, int rows)
    {
        if (m_platformConsole)
        {
            m_platformConsole->SetSize(columns, rows);
        }
    }

    bool ExternalConsole::IsCloseRequested() const
    {
        return m_platformConsole ? m_platformConsole->IsCloseRequested() : false;
    }

    void ExternalConsole::ResetCloseRequest()
    {
        if (m_platformConsole)
        {
            m_platformConsole->ResetCloseRequest();
        }
    }

    void ExternalConsole::ProcessConsoleInput()
    {
        if (m_platformConsole)
        {
            m_platformConsole->ProcessConsoleInput();
        }
    }

    bool ExternalConsole::HasPendingInput() const
    {
        return m_platformConsole ? m_platformConsole->HasPendingInput() : false;
    }
} // namespace enigma::core
