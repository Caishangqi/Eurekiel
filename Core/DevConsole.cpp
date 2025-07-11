#include "DevConsole.hpp"

#include "Clock.hpp"
#include "EngineCommon.hpp"
#include "ErrorWarningAssert.hpp"
#include "StringUtils.hpp"
#include "Timer.hpp"
#include "VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Game/GameCommon.hpp"
//
DevConsole* g_theDevConsole = nullptr;

// Color that related to Development console
const Rgba8 DevConsole::COLOR_ERROR        = Rgba8(255, 85, 85);
const Rgba8 DevConsole::COLOR_WARNING      = Rgba8(255, 170, 0);
const Rgba8 DevConsole::COLOR_INFO_MAJOR   = Rgba8(85, 255, 255);
const Rgba8 DevConsole::COLOR_INFO_LOG     = Rgba8(252, 252, 252);
const Rgba8 DevConsole::COLOR_INFO_MINOR   = Rgba8(200, 200, 200);
const Rgba8 DevConsole::COLOR_INPUT_NORMAL = Rgba8(168, 168, 168);


bool DevConsole::Event_KeyPressed(EventArgs& args)
{
    /// When receive any input from console while opening we reset the timer
    if (g_theDevConsole->IsOpen())
    {
        g_theDevConsole->m_insertionPointBlinkTimer->Stop();
        g_theDevConsole->m_insertionLineVisible = true;
        g_theDevConsole->m_insertionPointBlinkTimer->Start();
    }
    /// Filter that allow game to receive event when not press console trigger button
    if (!g_theDevConsole->IsOpen() &&
        static_cast<unsigned char>(args.GetValue("KeyCode", -1)) != KEYCODE_TILDE)
    {
        return false;
    }
    /// End of Filter
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_TILDE)
    {
        g_theDevConsole->ToggleOpen();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_BACKSPACE)
    {
        g_theDevConsole->HandleBackSpace();
        return false;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_LEFTARROW)
    {
        g_theDevConsole->HandleInsertionMove(1);
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_RIGHTARROW)
    {
        g_theDevConsole->HandleInsertionMove(-1);
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_DELETE)
    {
        g_theDevConsole->HandleDelete();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_SPACE)
    {
        g_theDevConsole->HandleSpace();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_ESC)
    {
        g_theDevConsole->HandleEscape();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_HOME)
    {
        g_theDevConsole->HandleHome();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_END)
    {
        g_theDevConsole->HandleEnd();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_ENTER)
    {
        g_theDevConsole->HandleEnter();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_UPARROW)
    {
        g_theDevConsole->HandleUpArrow();
        return true;
    }
    if (static_cast<unsigned char>(args.GetValue("KeyCode", -1)) == KEYCODE_DOWNARROW)
    {
        g_theDevConsole->HandleDownArrow();
        return true;
    }
    return true;
}

bool DevConsole::Event_CharInput(EventArgs& args)
{
    if (!g_theDevConsole->IsOpen())
        return false;
    unsigned char inputChar = static_cast<unsigned char>(args.GetValue("KeyCode", -1));
    if (inputChar >= 32 && inputChar <= 126)
    {
        if (inputChar == 96)
        {
            return true;
        }
        if (inputChar == ' ') // Char input do not handle blank, I let Handler to handle it
        {
            return true;
        }
        g_theDevConsole->m_inputText.insert(g_theDevConsole->m_insertionPointPosition, 1, static_cast<char>(inputChar));
        g_theDevConsole->m_insertionPointPosition++;
        return true;
    }
    return false;
}

bool DevConsole::Event_PasteClipboard(EventArgs& args)
{
    UNUSED(args)
    if (!g_theDevConsole->IsOpen())
        return false;

    if (!OpenClipboard(nullptr)) return false;

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr)
    {
        CloseClipboard();
        return false;
    }

    auto pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr)
    {
        CloseClipboard();
        return false;
    }

    std::string clipboardText(pszText);
    GlobalUnlock(hData);
    CloseClipboard();

    g_theDevConsole->m_inputText.insert(g_theDevConsole->m_insertionPointPosition, clipboardText);
    g_theDevConsole->m_insertionPointPosition += static_cast<int>(clipboardText.size());

    return true;
}

void DevConsole::HandleBackSpace()
{
    if (g_theDevConsole->IsOpen())
    {
        if (!g_theDevConsole->m_inputText.empty() && g_theDevConsole->m_insertionPointPosition > 0)
        {
            g_theDevConsole->m_inputText.erase(g_theDevConsole->m_insertionPointPosition - 1, 1);
            g_theDevConsole->m_insertionPointPosition--;
        }
    }
}

void DevConsole::HandleInsertionMove(int direction)
{
    if (g_theDevConsole->IsOpen())
    {
        if (direction > 0)
        {
            if (g_theDevConsole->m_insertionPointPosition > 0)
            {
                g_theDevConsole->m_insertionPointPosition--;
            }
        }
        else if (direction < 0)
        {
            if (g_theDevConsole->m_insertionPointPosition < static_cast<int>(g_theDevConsole->m_inputText.size()))
            {
                g_theDevConsole->m_insertionPointPosition++;
            }
        }
    }
}

void DevConsole::HandleSpace()
{
    if (g_theDevConsole->IsOpen())
    {
        g_theDevConsole->m_inputText.insert(g_theDevConsole->m_insertionPointPosition, 1, ' ');
        g_theDevConsole->m_insertionPointPosition++;
    }
}

void DevConsole::HandleDelete()
{
    if (g_theDevConsole->IsOpen())
    {
        if (!g_theDevConsole->m_inputText.empty() &&
            g_theDevConsole->m_insertionPointPosition < static_cast<int>(g_theDevConsole->m_inputText.size()))
        {
            g_theDevConsole->m_inputText.erase(g_theDevConsole->m_insertionPointPosition, 1);
        }
    }
}

void DevConsole::HandleEscape()
{
    if (g_theDevConsole->IsOpen())
    {
        if (g_theDevConsole->m_inputText.empty() && g_theDevConsole->IsOpen())
        {
            g_theDevConsole->ToggleOpen();
            g_theDevConsole->m_insertionPointPosition = 0;
        }
        else
        {
            g_theDevConsole->m_inputText.clear();
            g_theDevConsole->m_insertionPointPosition = 0;
        }
    }
}

void DevConsole::HandleHome()
{
    if (g_theDevConsole->IsOpen())
        g_theDevConsole->m_insertionPointPosition = 0;
}

void DevConsole::HandleEnd()
{
    if (g_theDevConsole->IsOpen())
    {
        g_theDevConsole->m_insertionPointPosition = static_cast<int>(g_theDevConsole->m_inputText.length());
    }
}

void DevConsole::HandleEnter()
{
    if (g_theDevConsole->IsOpen())
    {
        if (g_theDevConsole->m_inputText.empty())
        {
            g_theDevConsole->m_insertionPointPosition = 0;
            g_theDevConsole->ToggleOpen();
            return;
        }
        g_theDevConsole->Execute(g_theDevConsole->m_inputText);
        g_theDevConsole->m_insertionPointPosition = 0;
        g_theDevConsole->m_historyIndex           = -1;
        g_theDevConsole->m_inputText.clear();
    }
}

void DevConsole::HandleUpArrow()
{
    if (!g_theDevConsole->IsOpen())
        return;
    if (g_theDevConsole->m_commandHistory.empty())
        return;
    if (g_theDevConsole->m_historyIndex >= -1)
    {
        g_theDevConsole->m_historyIndex++;
        /// If exceed max then clamp it to the max index of command history
        if (g_theDevConsole->m_historyIndex > static_cast<int>(g_theDevConsole->m_commandHistory.size()) - 1)
        {
            g_theDevConsole->m_historyIndex = static_cast<int>(g_theDevConsole->m_commandHistory.size()) - 1;
        }
    }
    if (g_theDevConsole->m_historyIndex < static_cast<int>(g_theDevConsole->m_commandHistory.size()))
    {
        g_theDevConsole->m_inputText              = g_theDevConsole->m_commandHistory[static_cast<int>(g_theDevConsole->m_commandHistory.size()) - g_theDevConsole->m_historyIndex - 1];
        g_theDevConsole->m_insertionPointPosition = static_cast<int>(g_theDevConsole->m_inputText.length());
    }
}


void DevConsole::HandleDownArrow()
{
    if (!g_theDevConsole->IsOpen())
        return;
    if (g_theDevConsole->m_commandHistory.empty())
        return;
    g_theDevConsole->m_historyIndex--;
    if (g_theDevConsole->m_historyIndex < 0)
    {
        g_theDevConsole->m_historyIndex = -1;
        g_theDevConsole->m_inputText.clear();
        g_theDevConsole->m_insertionPointPosition = 0;
        return;
    }
    if (g_theDevConsole->m_historyIndex < static_cast<int>(g_theDevConsole->m_commandHistory.size()))
    {
        g_theDevConsole->m_inputText              = g_theDevConsole->m_commandHistory[static_cast<int>(g_theDevConsole->m_commandHistory.size()) - g_theDevConsole->m_historyIndex - 1];
        g_theDevConsole->m_insertionPointPosition = static_cast<int>(g_theDevConsole->m_inputText.length());
    }
}

bool DevConsole::Command_Clear(EventArgs& args)
{
    UNUSED(args)
    g_theDevConsole->m_lines.clear();
    return true;
}

bool DevConsole::Command_Help(EventArgs& args)
{
    if (args.GetValue("args", "").length() != 0)
    {
        g_theDevConsole->AddLine(COLOR_WARNING, Stringf("You should not add args after help command!\n"
                                     "Omitted arguments for the command"));
    }
    g_theDevConsole->AddLine(COLOR_INPUT_NORMAL, Stringf("Registered Commands"));
    for (std::string m_register_command : g_theDevConsole->m_registerCommands)
    {
        g_theDevConsole->AddLine(COLOR_INFO_LOG, Stringf("%s", m_register_command.c_str()));
    }
    return true;
}

bool DevConsole::Command_Quit(EventArgs& args)
{
    UNUSED(args)
    g_theEventSystem->FireEvent("WindowCloseEvent");
    return true;
}

bool DevConsole::Command_EcoArgs(EventArgs& args)
{
    g_theDevConsole->AddLine(COLOR_INFO_LOG, Stringf("Arguments > %s", args.GetValue("args", "null").c_str()));
    return true;
}

DevConsole::DevConsole(const DevConsoleConfig& config): m_config(config)
{
}

DevConsole::~DevConsole()
{
}


void DevConsole::Startup()
{
    g_theEventSystem->SubscribeEventCallbackFunction("KeyPressed", Event_KeyPressed);
    g_theEventSystem->SubscribeEventCallbackFunction("CharInput", Event_CharInput);
    g_theEventSystem->SubscribeEventCallbackFunction("PasteClipboard", Event_PasteClipboard);
    g_theEventSystem->SubscribeEventCallbackFunction("quit", Command_Quit);
    g_theEventSystem->SubscribeEventCallbackFunction("clear", Command_Clear);
    g_theEventSystem->SubscribeEventCallbackFunction("help", Command_Help);
    g_theEventSystem->SubscribeEventCallbackFunction("ecoargs", Command_EcoArgs);


    m_fontFullPath = m_config.m_fontPath.append(m_config.m_defaultFontName);
    m_lines.reserve(1000);
    AddLine(COLOR_WARNING, "Welcome to DevConsole v1.0.0\n"
            "Type help for a list of commands");

    /// Add register command
    m_registerCommands.push_back("help");
    m_registerCommands.push_back("quit");
    m_registerCommands.push_back("clear");
    m_registerCommands.push_back("ecoargs");
    m_registerCommands.push_back("debugclear");
    m_registerCommands.push_back("debugtoggle");

    /// Initialize Insertion Timer
    m_insertionPointBlinkTimer = new Timer(0.5f, &Clock::GetSystemClock());
    m_insertionPointBlinkTimer->Start();
    ///

    /// Broadcast the Console initialize event
    g_theEventSystem->FireEvent("Event.Console.Startup");
}

void DevConsole::Shutdown()
{
    delete m_insertionPointBlinkTimer;
    m_insertionPointBlinkTimer = nullptr;

    if (m_config.m_camera)
    {
        POINTER_SAFE_DELETE(m_config.m_camera)
    }
}

void DevConsole::BeginFrame()
{
    ++m_frameNumber;
    if (m_insertionPointBlinkTimer->DecrementPeriodIfElapsed())
    {
        m_insertionLineVisible = !m_insertionLineVisible;
    }

    if (g_theInput->WasMouseButtonJustPressed(KEYCODE_RIGHT_MOUSE))
    {
        if (IsOpen())
        {
            FireEvent("PasteClipboard");
        }
    }
}

void DevConsole::EndFrame()
{
}

void DevConsole::Execute(const std::string& consoleCommandText, bool echoCommand)
{
    /// Butler's dev console dirrerent from squirrel, working with doc provided by butler this semester
    //Strings multipleLinesCommands = SplitStringOnDelimiter(consoleCommandText, '\n');
    //for (auto const& lineOfCommand : multipleLinesCommands)
    //{
    //    if (!ExecuteSingleCommand(lineOfCommand))
    //    {
    //        AddLine(COLOR_ERROR, "[Error] Command Internal Error");
    //        ERROR_RECOVERABLE("Command Internal Error")
    //        return;
    //    }
    //}
    m_commandHistory.push_back(consoleCommandText);
    if (echoCommand)
    {
        AddLine(Rgba8(221, 221, 221), Stringf("%s", consoleCommandText.c_str()));
    }
    if (!ExecuteSingleCommand(consoleCommandText))
    {
        AddLine(COLOR_ERROR, Stringf("Unknown command: %s", consoleCommandText.c_str()));
    }
}

void DevConsole::AddLine(const Rgba8& color, const std::string& text)
{
    // Split '\n'
    Strings lines = SplitStringOnDelimiter(text, '\n');
    // foreach the split lines
    for (std::string line : lines)
    {
        DevConsoleLine devConsoleLine;
        devConsoleLine.m_color              = color;
        devConsoleLine.m_text               = line;
        devConsoleLine.m_frameNumberPrinted = m_frameNumber;
        // TODO: Add m_timePrinted
        m_lines.push_back(devConsoleLine);
    }
}

void DevConsole::Render(const AABB2& bounds, IRenderer* rendererOverride) const
{
    if (m_mode == DevConsoleMode::HIDDEN)
    {
        return;
    }

    if (rendererOverride == nullptr && m_config.renderer)
    {
        rendererOverride = m_config.renderer;
    }
    else
    {
        ERROR_RECOVERABLE("DevConsole renderer is null")
    }
    if (m_mode == DevConsoleMode::OPEN_FULL)
    {
        m_config.renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
        m_config.renderer->SetBlendMode(BlendMode::ALPHA);
        g_theRenderer->BeginCamera(*m_config.m_camera);
        Render_OpenFull(bounds, *rendererOverride, *rendererOverride->CreateOrGetBitmapFont(m_fontFullPath.c_str()), 1);
        g_theRenderer->EndCamera(*m_config.m_camera);
    }
}

void DevConsole::RegisterCommand(const std::string& commandHeader, const std::string& description, EventCallbackFunction functionPtr)
{
    UNUSED(description)
    m_registerCommands.push_back(commandHeader);
    g_theEventSystem->SubscribeEventCallbackFunction(commandHeader, functionPtr);
}

void DevConsole::ToggleOpen()
{
    m_isOpen = !m_isOpen;
    //m_insertionPointPosition = 0;
    if (m_isOpen)
    {
        ToggleMode(DevConsoleMode::OPEN_FULL);
    }
    else
    {
        ToggleMode(DevConsoleMode::HIDDEN);
        //m_historyIndex = -1;
    }
}

bool DevConsole::IsOpen()
{
    return m_isOpen;
}

DevConsoleMode DevConsole::GetMode() const
{
    return m_mode;
}

void DevConsole::SetMode(DevConsoleMode mode)
{
    m_mode = mode;
}

void DevConsole::ToggleMode(DevConsoleMode mode)
{
    switch (mode)
    {
    case DevConsoleMode::HIDDEN:
        SetMode(mode);
        break;
    case DevConsoleMode::OPEN_FULL:
        SetMode(mode);
        break;
    case DevConsoleMode::OPEN_PARTIAL:
        SetMode(mode);
        ERROR_RECOVERABLE("OPEN_PARTIAL mode is currently unsupported")
        break;
    case DevConsoleMode::COMMAND_PROMPT_ONLY:
        SetMode(mode);
        ERROR_RECOVERABLE("COMMAND_PROMPT_ONLY mode is currently unsupported")
        break;
    case DevConsoleMode::NUM:
        ERROR_AND_DIE("DevConsole::ToggleMode: Invalid Mode")
    }
}

int DevConsole::GetFrameNumber() const
{
    return m_frameNumber;
}

void DevConsole::Render_OpenFull(const AABB2& bounds, IRenderer& renderer, BitmapFont& font, float fontAspect) const
{
    renderer.BindTexture(nullptr);
    std::vector<Vertex_PCU> vertices;
    std::vector<Vertex_PCU> backgroundVerts;
    vertices.reserve(10000);
    backgroundVerts.reserve(10000);

    float lineHeight          = bounds.GetDimensions().y / m_config.m_maxLinesDisplay;
    AABB2 actualBounds        = bounds;
    int   lineIndexFromBottom = 0;
    AddVertsForAABB2D(backgroundVerts, bounds, Rgba8(0, 0, 0, 180));
    renderer.DrawVertexArray(backgroundVerts);
    for (int line = static_cast<int>(m_lines.size()) - 1; line >= 0; --line)
    {
        /// Only render the visible line
        if (lineIndexFromBottom >= static_cast<int>(round(m_config.m_maxLinesDisplay)))
        {
            continue;
        }
        DevConsoleLine devConsoleLine = m_lines[line];
        Vec2           linePos        = actualBounds.m_mins + Vec2(0, lineHeight * static_cast<float>(lineIndexFromBottom));
        linePos.y += lineHeight; /// This is the offset that split inputline and coonsole line
        float lineWidth = font.GetTextWidth(lineHeight, devConsoleLine.m_text, fontAspect);
        if (lineWidth > bounds.GetDimensions().x) // If bigger than console width, we use advance box alignment method
        {
            auto lineBounds = AABB2(linePos, linePos + Vec2(bounds.GetDimensions().x, lineHeight));
            font.AddVertsForTextInBox2D(vertices, devConsoleLine.m_text, lineBounds, lineHeight, devConsoleLine.m_color, fontAspect, Vec2(0, 0.5f));
        }
        else
        {
            font.AddVertsForText2D(vertices, linePos, lineHeight, devConsoleLine.m_text, devConsoleLine.m_color, fontAspect);
        }
        lineIndexFromBottom++;
    }
    renderer.BindTexture(&font.GetTexture());
    renderer.DrawVertexArray(vertices);

    /// Render the Input line
    RenderInputLine(bounds, renderer, font, fontAspect, lineHeight);
}

void DevConsole::RenderInputLine(const AABB2& bounds, IRenderer& renderer, BitmapFont& font, float fontAspect, float lineHeight) const
{
    std::vector<Vertex_PCU> inputLineVerts;
    inputLineVerts.reserve(10000);
    font.AddVertsForTextInBox2D(inputLineVerts, m_inputText, AABB2(0, 0, bounds.GetDimensions().x, lineHeight), lineHeight, COLOR_INPUT_NORMAL, fontAspect, Vec2(0, 0.5f));
    renderer.BindTexture(&font.GetTexture());
    renderer.DrawVertexArray(inputLineVerts);

    /// Insertion Line
    RenderInsertionLine(bounds, renderer, font, fontAspect, lineHeight);
}

void DevConsole::AdjustInsertionLine()
{
}

void DevConsole::RenderInsertionLine(const AABB2& bounds, IRenderer& renderer, BitmapFont& font, float fontAspect, float lineHeight) const
{
    UNUSED(font)
    UNUSED(fontAspect)
    if (!m_insertionLineVisible)
        return;
    renderer.BindTexture(nullptr);
    std::vector<Vertex_PCU> insertionLineVerts;
    insertionLineVerts.reserve(100);
    AABB2 insertionLine;
    int   maxCharsX       = static_cast<int>(bounds.GetDimensions().x / lineHeight * fontAspect);
    float scaleMultiplier = GetClamped(static_cast<float>(maxCharsX) / static_cast<float>(m_inputText.size()), 0.f, 1.f);
    insertionLine.SetDimensions(Vec2(4.0f * scaleMultiplier, lineHeight));
    insertionLine.SetCenter(Vec2(static_cast<float>(m_insertionPointPosition) * lineHeight * scaleMultiplier, lineHeight / 2.0f));
    AddVertsForAABB2D(insertionLineVerts, insertionLine, Rgba8(221, 221, 221));
    renderer.DrawVertexArray(insertionLineVerts);
}

bool DevConsole::IsCommandRegistered(const std::string& consoleCommandHeader) const
{
    for (std::string m_register_command : m_registerCommands)
    {
        if (m_register_command == consoleCommandHeader)
            return true;
    }
    return false;
}

bool DevConsole::ExecuteSingleCommand(const std::string& consoleCommandText)
{
    Strings commandSegment = SplitStringOnDelimiter(consoleCommandText, ' ');
    if (commandSegment.size() < 1)
    {
        AddLine(COLOR_ERROR, "[Error] Command Header is not provided");
        ERROR_RECOVERABLE("Command Header is Empty")
        return false;
    }
    if (commandSegment.size() == 1)
    {
        if (IsCommandRegistered(consoleCommandText))
        {
            g_theEventSystem->FireEvent(commandSegment[0]); // Fire no args event
            return true;
        }
        return false;
    }
    if (commandSegment.size() > 1)
    {
        std::string commandHeader = commandSegment[0];
        EventArgs   args;
        if (IsCommandRegistered(commandHeader))
        {
            /*for (int i = 1; i < (int)commandSegment.size(); ++i)
            {
                Strings keyValuePairs = SplitStringOnDelimiter(commandSegment[i], '=');
                if (keyValuePairs.size() != 2)
                {
                    AddLine(COLOR_ERROR, "Command value is not valid");
                    return false;
                }
                args.SetValue(keyValuePairs[0], keyValuePairs[1]);
            }*/
            /// We append args and put into one big string and let Command handler to parse it
            std::string commandArg;
            for (int i = 1; i < static_cast<int>(commandSegment.size()); i++)
            {
                if (i == 1)
                {
                    commandArg.append(commandSegment[i]);
                    continue;
                }
                commandArg.append(" " + commandSegment[i]);
            }
            args.SetValue("args", commandArg);
            g_theEventSystem->FireEvent(commandHeader, args);
            return true;
        }
        return false;
    }
    return false;
}
