#pragma once
#include "Engine/Core/Rgba8.hpp"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>

#include "ThirdParty/imgui/imgui.h"

namespace enigma::core
{
    // Log message structure (simplified, for display)
    struct DisplayMessage
    {
        std::string timestamp;      // Timestamp HH:MM:SS
        std::string category;       // Category (Game, Render, Audio, etc.)
        std::string level;          // Level (Verbose, Info, Warning, Error, Fatal)
        std::string message;        // Message content

        // Lowercase text for filtering
        std::string searchableText; // All lowercase searchable text (category + message)

        DisplayMessage() = default;
        DisplayMessage(const std::string& ts, const std::string& cat,
                       const std::string& lvl, const std::string& msg)
            : timestamp(ts), category(cat), level(lvl), message(msg)
        {
            // Construct searchable text (lowercase)
            searchableText = category + " " + message;
            for (char& c : searchableText)
            {
                c = static_cast<char>(tolower(c));
            }
        }
    };

    // MessageLog UI configuration
    struct MessageLogUIConfig
    {
        bool showWindow = false;         // Whether the window is visible
        int toggleKey = 0xC0;            // Toggle key (default ~ key, VK_OEM_3)
        size_t maxMessages = 10000;      // Maximum number of messages
        bool autoScroll = true;          // Auto-scroll to bottom
        float windowWidth = 1200.0f;     // Window width
        float windowHeight = 700.0f;     // Window height
    };

    // MessageLog UI class (refactored - top toolbar + fullscreen log panel layout)
    class MessageLogUI
    {
    public:
        MessageLogUI();
        ~MessageLogUI();

        // Render UI
        void Render();

        // Add message (API: AddMessage("LogGame", "Info", "Game initialized successfully"))
        void AddMessage(const std::string& category, const std::string& level, const std::string& message);

        // Toggle window
        void ToggleWindow();
        bool IsWindowOpen() const { return m_config.showWindow; }
        void SetWindowOpen(bool open) { m_config.showWindow = open; }

        // Configuration
        MessageLogUIConfig& GetConfig() { return m_config; }
        const MessageLogUIConfig& GetConfig() const { return m_config; }

        // Clear messages
        void Clear();

        // Export messages to file
        void ExportToFile(const std::string& filepath);

    private:
        // Render sections
        void RenderTopToolbar();              // Render top toolbar (search + Filter button)
        void RenderMainPanel();               // Render main panel (fullscreen log display)
        void RenderVerbosityFilter();        // Render Verbosity filter
        void RenderCategoryFilter();         // Render Category filter
        void RenderCategoryPopup();          // Render Categories popup
        void RenderBottomToolbar();          // Render bottom toolbar

        // Filter logic
        void ApplyFilter();
        bool PassesFilter(const DisplayMessage& msg) const;

        // Category management
        void UpdateCategoryCounts();          // Update category message counts

        // Helper functions
        ImVec4 GetLevelColor(const std::string& level) const;
        std::string GetCurrentTimestamp() const;
        std::string ToLowerCase(const std::string& str) const;

        // Configuration
        MessageLogUIConfig m_config;

        // Message storage
        std::deque<DisplayMessage> m_allMessages;        // All messages
        std::vector<size_t> m_filteredIndices;           // Filtered message indices

        // Filter state - search
        char m_searchBuffer[256] = {0};                  // Search box content
        char m_categorySearchBuffer[256] = {0};          // Categories search buffer

        // Verbosity level filter mode
        enum class VerbosityMode
        {
            None,      // None - don't display
            Filtered,  // Filtered - filter based on search box
            All        // All - display all
        };

        // Filter state - Verbosity (three-choice)
        VerbosityMode m_verboseModeFilter = VerbosityMode::All;      // Verbose filter mode
        VerbosityMode m_infoModeFilter = VerbosityMode::All;         // Info filter mode
        VerbosityMode m_warningModeFilter = VerbosityMode::All;      // Warning filter mode
        VerbosityMode m_errorModeFilter = VerbosityMode::All;        // Error filter mode
        VerbosityMode m_fatalModeFilter = VerbosityMode::All;        // Fatal filter mode

        // Filter state - Category (multiple-choice)
        std::unordered_set<std::string> m_allCategories;        // All categories that have appeared
        std::unordered_map<std::string, bool> m_categoryEnabled; // Whether category is enabled
        std::unordered_map<std::string, int> m_categoryCounts;  // Category message counts

        // UI collapse state
        bool m_verbosityCollapsed = false;               // Whether Verbosity area is collapsed
        bool m_categoriesCollapsed = false;              // Whether Categories area is collapsed

        // UI state
        bool m_needsFilterUpdate = true;                 // Needs filter update
        bool m_scrollToBottom = false;                   // Needs to scroll to bottom
    };
}
