#pragma once
#include <string>
#include <vector>

#include "EventSystem.hpp"
#include "NamedStrings.hpp"
#include "Rgba8.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Renderer/IRenderer.hpp"

class Timer;
class Camera;
struct Vertex_PCU;
class BitmapFont;
class Renderer;


struct DevConsoleLine
{
    Rgba8       m_color;
    std::string m_text;
    int         m_frameNumberPrinted;
    double      m_timePrinted;
};

enum class DevConsoleMode
{
    HIDDEN,
    OPEN_FULL,
    OPEN_PARTIAL,
    COMMAND_PROMPT_ONLY,
    NUM
};

struct DevConsoleConfig
{
    Camera*     m_camera            = nullptr;
    Renderer*   renderer            = nullptr;
    IRenderer*  _renderer           = nullptr;
    std::string m_defaultFontName   = "CaiziiFixedFont";
    float       m_defaultFontAspect = 0.7f;
    float       m_maxLinesDisplay   = 40.5f;
    int         m_maxCommandHistory = 128;
    bool        m_startOpen         = false;
};

// Class for a dev console that allows entering text and executing commands. 
// Can be toggled with tilde ('~') and renders within a transparent box with configurable bounds.
// Other features include specific coloring for different lines of text and a blinking insertion point.
class DevConsole
{
public:
    /// Event
    static bool Event_KeyPressed(EventArgs& args); // Handle char input by appending valid characters to our current input line.
    static bool Event_CharInput(EventArgs& args); // Handle key input.
    static bool Event_PasteClipboard(EventArgs& args);  // Handle Windows paste
    static bool Command_Clear(EventArgs& args); // Clear all lines of text.
    static bool Command_Help(EventArgs& args); // Display all currently registered commands in the event system.
    static bool Command_Quit(EventArgs& args);
    static bool Command_EcoArgs(EventArgs& args);
    /// Handler
    static void HandleBackSpace();
    static void HandleInsertionMove(int direction = 1);
    static void HandleSpace();
    static void HandleDelete();
    static void HandleEscape();
    static void HandleHome();
    static void HandleEnd();
    static void HandleEnter();
    static void HandleUpArrow();
    static void HandleDownArrow();
    /// 
    DevConsole(const DevConsoleConfig& config);
    ~DevConsole();

    // Subscribes to any events needed, prints an initial line of text, and starts the blink timer.
    void Startup();
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    // Parses the current input line and executes it using the event system.
    // Commands and arguments are delimited from each other with space (' ') 
    // and argument names and values are delimited with equals ('='). 
    // Echos the command to the dev console as well as any command output.
    void Execute(const std::string& consoleCommandText, bool echoCommand = true);
    // Adds a line of text to the current list of lines being shown. 
    // Individual lines are delimited with the newline ('\n') character.
    void AddLine(const Rgba8& color, const std::string& text);
    // Renders just visible text lines within the bounds specified.
    // Bounds are in terms of the camera being used to render.
    // The current input line renders at the bottom with all other lines rendered above it,
    // with the most recent lines at the bottom.
    void Render(const AABB2& bounds, Renderer* rendererOverride = nullptr) const;

    void RegisterCommand(const std::string& commandHeader, const std::string& description, EventCallbackFunction functionPtr);

    // Toggles between open and closed.
    void ToggleOpen();
    bool IsOpen();

    DevConsoleMode GetMode() const;
    void           SetMode(DevConsoleMode mode);
    void           ToggleMode(DevConsoleMode mode);

    int GetFrameNumber() const;


    static const Rgba8 COLOR_ERROR;
    static const Rgba8 COLOR_WARNING;
    static const Rgba8 COLOR_INFO_MAJOR;
    static const Rgba8 COLOR_INFO_LOG;
    static const Rgba8 COLOR_INFO_MINOR;
    static const Rgba8 COLOR_INPUT_NORMAL;

protected:
    void Render_OpenFull(const AABB2& bounds, Renderer& renderer, BitmapFont& font, float fontAspect = 1.f) const;
    bool ExecuteSingleCommand(const std::string& consoleCommandText);
    void RenderInputLine(const AABB2& bounds, Renderer& renderer, BitmapFont& font, float fontAspect, float lineHeight) const; // render the input line of the console
    void AdjustInsertionLine();
    void RenderInsertionLine(const AABB2& bounds, Renderer& renderer, BitmapFont& font, float fontAspect, float lineHeight) const;
    /// Command handle
    bool IsCommandRegistered(const std::string& consoleCommandHeader) const;

    DevConsoleConfig            m_config;
    DevConsoleMode              m_mode = DevConsoleMode::HIDDEN;
    std::vector<DevConsoleLine> m_lines; // #ToDo: support a max limited # of lines (e.g. fixed circular buffer)
    int                         m_frameNumber = 0;
    std::string                 m_fontPath;
    bool                        m_isOpen = false; // True if the dev console is currently visible and accepting input.
    std::string                 m_inputText; // Our current line of input text.
    int                         m_insertionPointPosition = 0; // Index of the insertion point in our current input text.
    bool                        m_insertionPointVisible  = true; // True if our insertion point is currently in the visible phase of blinking.
    std::vector<std::string>    m_commandHistory; // History of all commands executed.
    int                         m_historyIndex = -1; // Our current index in our history of commands as we are scrolling.

    Timer* m_insertionPointBlinkTimer = nullptr; // Timer for controlling insertion point visibility.

    /// Functionality Drawing
    /// Including Insertion Line
    AABB2 m_insertionLine;

    /// Command Register 1.0.0
    Strings m_registerCommands;

    bool m_insertionLineVisible = true;
};
