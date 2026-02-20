#include "Engine/Core/Console/ImguiConsoleConfig.hpp"
#include "ThirdParty/imgui/imgui.h"

// ImguiConsoleConfig - Default values for ImGui Console visual constants
// Definitions - declarations in ImguiConsoleConfig.hpp

//=============================================================================
// General
//=============================================================================
float CONSOLE_FONT_SIZE    = 14.0f;
float CONSOLE_INPUT_HEIGHT = 24.0f;
int   CONSOLE_MAX_MESSAGES = 1024;

//=============================================================================
// Overlay Mode (Bottom Docked)
//=============================================================================
float OVERLAY_WIDTH_RATIO        = 0.6f;
float OVERLAY_HEIGHT_RATIO       = 0.4f;
float OVERLAY_BG_ALPHA           = 0.85f;
int   OVERLAY_MAX_VISIBLE_ITEMS  = 10;
float OVERLAY_POSITION_THRESHOLD = 5.0f;

//=============================================================================
// Console Colors
//=============================================================================
ImVec4 CONSOLE_COLOR_LOG      = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // White
ImVec4 CONSOLE_COLOR_WARNING  = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow
ImVec4 CONSOLE_COLOR_ERROR    = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);    // Red
ImVec4 CONSOLE_COLOR_COMMAND  = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);    // Green
ImVec4 CONSOLE_COLOR_BG       = ImVec4(0.1f, 0.1f, 0.12f, 0.9f);   // Dark background
ImVec4 CONSOLE_COLOR_INPUT_BG = ImVec4(0.15f, 0.15f, 0.18f, 1.0f); // Slightly lighter

//=============================================================================
// Overlay Popup Colors
//=============================================================================
ImVec4 OVERLAY_BG_COLOR          = ImVec4(0.12f, 0.12f, 0.15f, 0.95f); // Dark popup bg
ImVec4 OVERLAY_SELECTED_COLOR    = ImVec4(0.1f, 0.4f, 0.1f, 0.8f);    // Dark green
ImVec4 OVERLAY_HOVER_COLOR       = ImVec4(0.2f, 0.2f, 0.3f, 0.6f);    // Subtle hover
ImVec4 OVERLAY_MATCH_HIGHLIGHT   = ImVec4(0.2f, 0.6f, 0.2f, 0.4f);    // Green tint
ImVec4 OVERLAY_DESCRIPTION_COLOR = ImVec4(0.8f, 0.8f, 0.8f, 0.7f);    // Dimmed gray for descriptions

//=============================================================================
// Docked Mode
//=============================================================================
float       DOCKED_TOOLBAR_HEIGHT    = 28.0f;
float       DOCKED_INPUT_HEIGHT      = 24.0f;
const char* DOCKED_INPUT_PLACEHOLDER = "Enter console command";
const char* DOCKED_INPUT_LABEL       = "Cmd";

//=============================================================================
// Full Mode (centered input + message overlay above)
//=============================================================================
float FULL_MODE_INPUT_Y_RATIO  = 0.65f;
float FULL_MODE_OVERLAY_HEIGHT = 0.4f;
