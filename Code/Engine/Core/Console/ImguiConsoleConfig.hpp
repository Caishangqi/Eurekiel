#pragma once

// Forward declare ImVec4 to avoid pulling in full imgui header
struct ImVec4;

// ImguiConsoleConfig - Visual constants for ImGui Console
// Declarations only - definitions in ImguiConsoleConfig.cpp
// Follows the extern global variable pattern from ConvexSceneConfig.hpp

//=============================================================================
// General
//=============================================================================
extern float CONSOLE_FONT_SIZE;
extern float CONSOLE_INPUT_HEIGHT;
extern int   CONSOLE_MAX_MESSAGES;

//=============================================================================
// Overlay Mode (Bottom Docked)
//=============================================================================
extern float OVERLAY_WIDTH_RATIO;         // Ratio of screen width (default 0.6)
extern float OVERLAY_HEIGHT_RATIO;        // Ratio of screen height (default 0.4)
extern float OVERLAY_BG_ALPHA;            // Background opacity
extern int   OVERLAY_MAX_VISIBLE_ITEMS;   // Max visible items in overlay popup
extern float OVERLAY_POSITION_THRESHOLD;  // Position deviation threshold for snap-back (default 5.0)

//=============================================================================
// Console Colors
//=============================================================================
extern ImVec4 CONSOLE_COLOR_LOG;          // White - normal log messages
extern ImVec4 CONSOLE_COLOR_WARNING;      // Yellow - warnings
extern ImVec4 CONSOLE_COLOR_ERROR;        // Red - errors
extern ImVec4 CONSOLE_COLOR_COMMAND;      // Green - user commands
extern ImVec4 CONSOLE_COLOR_BG;           // Console background color
extern ImVec4 CONSOLE_COLOR_INPUT_BG;     // Input line background color

//=============================================================================
// Overlay Popup Colors
//=============================================================================
extern ImVec4 OVERLAY_BG_COLOR;           // Popup background color
extern ImVec4 OVERLAY_SELECTED_COLOR;     // Selected item background (dark green)
extern ImVec4 OVERLAY_HOVER_COLOR;        // Hovered item background
extern ImVec4 OVERLAY_MATCH_HIGHLIGHT;    // Fuzzy match highlight color (green tint)

//=============================================================================
// Docked Mode
//=============================================================================
extern float       DOCKED_TOOLBAR_HEIGHT;
extern float       DOCKED_INPUT_HEIGHT;
extern const char* DOCKED_INPUT_PLACEHOLDER;  // "Enter console command"
extern const char* DOCKED_INPUT_LABEL;        // "Cmd"

//=============================================================================
// Full Mode (centered input + message overlay above)
//=============================================================================
extern float FULL_MODE_INPUT_Y_RATIO;    // Input Y position (screen height ratio, default 0.65)
extern float FULL_MODE_OVERLAY_HEIGHT;   // Message overlay height (screen height ratio, default 0.4)
