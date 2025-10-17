#include "MessageLogUI.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <ThirdParty/imgui/imgui.h>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <fstream>

namespace enigma::core
{
    // ================================================================================================
    // ================================================================================================
    MessageLogUI::MessageLogUI()
    {
        // Initialize level filter modes (default to show all)
        m_verboseModeFilter = VerbosityMode::All;
        m_infoModeFilter    = VerbosityMode::All;
        m_warningModeFilter = VerbosityMode::All;
        m_errorModeFilter   = VerbosityMode::All;
        m_fatalModeFilter   = VerbosityMode::All;

        // Initialize collapse states (default to expanded)
        m_verbosityCollapsed  = false;
        m_categoriesCollapsed = false;

        // Initialize search buffer
        m_categorySearchBuffer[0] = '\0';

        // Add initial message
        AddMessage("LogSystem", "Info", "MessageLog UI initialized successfully");
    }

    MessageLogUI::~MessageLogUI()
    {
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::Render()
    {
        if (!m_config.showWindow)
            return;

        // Apply filter (if update needed)
        if (m_needsFilterUpdate)
        {
            ApplyFilter();
            m_needsFilterUpdate = false;
        }

        // Set window size
        ImGui::SetNextWindowSize(ImVec2(m_config.windowWidth, m_config.windowHeight), ImGuiCond_FirstUseEver);

        // Begin rendering window
        bool windowOpen = m_config.showWindow;
        if (!ImGui::Begin("MessageLog - Press ~ to toggle", &windowOpen))
        {
            ImGui::End();
            m_config.showWindow = windowOpen;
            return;
        }
        m_config.showWindow = windowOpen;

        // 1. Render top toolbar (search + Filter button)
        RenderTopToolbar();

        ImGui::Separator();

        // 2. Render main panel (fullscreen log display)
        RenderMainPanel();

        ImGui::Separator();

        // 3. Render bottom toolbar
        RenderBottomToolbar();

        ImGui::End();
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::RenderTopToolbar()
    {
        // Search box (occupies remaining width - Filter button width - spacing)
        float filterButtonWidth = 80.0f;
        float availWidth        = ImGui::GetContentRegionAvail().x;
        float searchWidth       = availWidth - filterButtonWidth - ImGui::GetStyle().ItemSpacing.x;

        ImGui::PushItemWidth(searchWidth);
        if (ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer)))
        {
            m_needsFilterUpdate = true;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // Filter button
        if (ImGui::Button("Filter", ImVec2(filterButtonWidth, 0)))
        {
            // Pre-calculate expected popup size to prevent positioning issues on first open
            // Calculate width from config: leftTableWidth + rightTableWidth + padding
            float popupWidth = m_verbosityTableConfig.GetTotalWidth() + 30.0f; // Add padding
            // Height: tableHeight + title + separator + spacing + categories section + padding
            // Adjusted from 280px to account for reduced tableHeight (150 -> 120) and new spacing
            float popupHeight = m_verbosityTableConfig.tableHeight + m_verbosityTableConfig.spacingAfterVerbosity + 130.0f;
            ImGui::SetNextWindowSize(ImVec2(popupWidth, popupHeight), ImGuiCond_Appearing);
            ImGui::OpenPopup("FilterPopup");
        }

        // Filter Popup (with AlwaysAutoResize to ensure proper sizing)
        if (ImGui::BeginPopup("FilterPopup", ImGuiWindowFlags_AlwaysAutoResize))
        {
            // Verbosity title + separator (without collapse)
            ImGui::Text("Lengthy");
            ImGui::Separator();
            RenderVerbosityFilter();

            // Add configurable spacing after Verbosity section
            ImGui::Dummy(ImVec2(0, m_verbosityTableConfig.spacingAfterVerbosity));

            // Categories select all checkbox + button
            // Calculate whether all categories are enabled
            bool allCategoriesEnabled = true;
            for (const auto& [category, enabled] : m_categoryEnabled)
            {
                if (!enabled)
                {
                    allCategoriesEnabled = false;
                    break;
                }
            }
            ImGui::Text("Categories");
            ImGui::Separator();
            // Render select all checkbox
            if (ImGui::Checkbox("##AllCategories", &allCategoriesEnabled))
            {
                // Select all or deselect all categories
                for (auto& [category, enabled] : m_categoryEnabled)
                {
                    enabled = allCategoriesEnabled;
                }
                m_needsFilterUpdate = true;
            }

            ImGui::SameLine();

            // Categories button (click to open nested popup)
            if (ImGui::Button("Category Filter", ImVec2(-1.0f, 0)))
            {
                ImGui::OpenPopup("CategoriesPopup");
            }

            // Render Categories popup
            RenderCategoryPopup();

            ImGui::EndPopup();
        }
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::RenderVerbosityFilter()
    {
        // Define this macro to switch between single table and split table layouts
        // 0 = Single table with colored cells (previous default)
        // 1 = Split layout: left table for levels + right table for buttons
#define USE_SPLIT_TABLE_LAYOUT 1

#if USE_SPLIT_TABLE_LAYOUT
        // ============================================================================================
        // SPLIT TABLE IMPLEMENTATION: Two-table layout with colored level cells on left
        // ============================================================================================

        // Selectable flags to prevent closing popup
        ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_DontClosePopups;

        // Use BeginChild to create fixed-width containers for proper horizontal layout
        // Get layout parameters from config
        float tableHeight    = m_verbosityTableConfig.tableHeight;
        float leftTableWidth = m_verbosityTableConfig.leftTableWidth;

        // Left child: Fixed width and height for level names
        ImGui::BeginChild("LeftTableChild", ImVec2(leftTableWidth, tableHeight), false, ImGuiWindowFlags_NoScrollbar);
        {
            // --- Left Table: Level names with colored backgrounds ---
            ImGuiTableFlags leftTableFlags = ImGuiTableFlags_Borders |
                ImGuiTableFlags_SizingFixedFit;

            if (ImGui::BeginTable("VerbosityLevelTable", 1, leftTableFlags))
            {
                ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, leftTableWidth - 10.0f);

                // Verbose
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(153, 153, 153, 80));
                ImGui::Text("Verbose");

                // Info
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(242, 242, 242, 80));
                ImGui::Text("Info");

                // Warning
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(255, 240, 150, 80));
                ImGui::Text("Warning");

                // Error
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(255, 180, 180, 80));
                ImGui::Text("Error");

                // Fatal
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(220, 120, 140, 80));
                ImGui::Text("Fatal");

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Add spacing between the two tables (horizontal)
        ImGui::SameLine();
        //ImGui::Dummy(ImVec2(10.0f, 0.0f));
        ImGui::SameLine();

        // Right child: Fixed width based on button columns
        float rightTableWidth = m_verbosityTableConfig.GetRightTableWidth();
        ImGui::BeginChild("RightTableChild", ImVec2(rightTableWidth, tableHeight), false, ImGuiWindowFlags_NoScrollbar);
        {
            // --- Right Table: None/Filtered/All buttons ---
            ImGuiTableFlags rightTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit;

            if (ImGui::BeginTable("VerbosityButtonTable", 3, rightTableFlags))
            {
                // Use config for equal-width button columns
                float buttonWidth = m_verbosityTableConfig.buttonColumnWidth;
                ImGui::TableSetupColumn("None", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
                ImGui::TableSetupColumn("Filtered", ImGuiTableColumnFlags_WidthFixed, buttonWidth);
                ImGui::TableSetupColumn("All", ImGuiTableColumnFlags_WidthFixed, buttonWidth);

                // Center-align text in button columns
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

                // Row 1: Verbose
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID("Verbose_None");
                if (ImGui::Selectable("None", m_verboseModeFilter == VerbosityMode::None, selectableFlags))
                {
                    m_verboseModeFilter = VerbosityMode::None;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("Verbose_Filtered");
                if (ImGui::Selectable("Filtered", m_verboseModeFilter == VerbosityMode::Filtered, selectableFlags))
                {
                    m_verboseModeFilter = VerbosityMode::Filtered;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID("Verbose_All");
                if (ImGui::Selectable("All", m_verboseModeFilter == VerbosityMode::All, selectableFlags))
                {
                    m_verboseModeFilter = VerbosityMode::All;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                // Row 2: Info
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID("Info_None");
                if (ImGui::Selectable("None", m_infoModeFilter == VerbosityMode::None, selectableFlags))
                {
                    m_infoModeFilter    = VerbosityMode::None;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("Info_Filtered");
                if (ImGui::Selectable("Filtered", m_infoModeFilter == VerbosityMode::Filtered, selectableFlags))
                {
                    m_infoModeFilter    = VerbosityMode::Filtered;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID("Info_All");
                if (ImGui::Selectable("All", m_infoModeFilter == VerbosityMode::All, selectableFlags))
                {
                    m_infoModeFilter    = VerbosityMode::All;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                // Row 3: Warning
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID("Warning_None");
                if (ImGui::Selectable("None", m_warningModeFilter == VerbosityMode::None, selectableFlags))
                {
                    m_warningModeFilter = VerbosityMode::None;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("Warning_Filtered");
                if (ImGui::Selectable("Filtered", m_warningModeFilter == VerbosityMode::Filtered, selectableFlags))
                {
                    m_warningModeFilter = VerbosityMode::Filtered;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID("Warning_All");
                if (ImGui::Selectable("All", m_warningModeFilter == VerbosityMode::All, selectableFlags))
                {
                    m_warningModeFilter = VerbosityMode::All;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                // Row 4: Error
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID("Error_None");
                if (ImGui::Selectable("None", m_errorModeFilter == VerbosityMode::None, selectableFlags))
                {
                    m_errorModeFilter   = VerbosityMode::None;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("Error_Filtered");
                if (ImGui::Selectable("Filtered", m_errorModeFilter == VerbosityMode::Filtered, selectableFlags))
                {
                    m_errorModeFilter   = VerbosityMode::Filtered;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID("Error_All");
                if (ImGui::Selectable("All", m_errorModeFilter == VerbosityMode::All, selectableFlags))
                {
                    m_errorModeFilter   = VerbosityMode::All;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                // Row 5: Fatal
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID("Fatal_None");
                if (ImGui::Selectable("None", m_fatalModeFilter == VerbosityMode::None, selectableFlags))
                {
                    m_fatalModeFilter   = VerbosityMode::None;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushID("Fatal_Filtered");
                if (ImGui::Selectable("Filtered", m_fatalModeFilter == VerbosityMode::Filtered, selectableFlags))
                {
                    m_fatalModeFilter   = VerbosityMode::Filtered;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(2);
                ImGui::PushID("Fatal_All");
                if (ImGui::Selectable("All", m_fatalModeFilter == VerbosityMode::All, selectableFlags))
                {
                    m_fatalModeFilter   = VerbosityMode::All;
                    m_needsFilterUpdate = true;
                }
                ImGui::PopID();

                // Pop the center-align style
                ImGui::PopStyleVar();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // SPACING REDUCTION: Move cursor up to compensate for EndChild bottom padding + default ItemSpacing
        // Risk Level: MEDIUM - This is a visual adjustment that doesn't break functionality
        // Analysis: EndChild typically adds ~4-6px bottom padding, plus ItemSpacing.y adds another ~4-8px
        // Solution: Move cursor up by 10px to significantly reduce the gap while keeping some minimal spacing
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 10.0f);

#else
        // ============================================================================================
        // SINGLE TABLE IMPLEMENTATION: Single table with colored level cells
        // ============================================================================================

        // Use ImGui Table for proper alignment
        // Table layout: 4 columns (Level | None | Filtered | All)
        ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("VerbosityFilterTable", 4, tableFlags))
        {
            // Setup columns
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("None", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Filtered", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("All", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            // REMOVED: ImGui::TableHeadersRow(); - No header row as per requirement

            // Selectable flags to prevent closing popup
            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_DontClosePopups;

            // Row 1: Verbose (Light Gray)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(153, 153, 153, 80)); // Light gray
            ImGui::Text("Verbose");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("Verbose_None");
            if (ImGui::Selectable("None", m_verboseModeFilter == VerbosityMode::None, selectableFlags))
            {
                m_verboseModeFilter = VerbosityMode::None;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("Verbose_Filtered");
            if (ImGui::Selectable("Filtered", m_verboseModeFilter == VerbosityMode::Filtered, selectableFlags))
            {
                m_verboseModeFilter = VerbosityMode::Filtered;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("Verbose_All");
            if (ImGui::Selectable("All", m_verboseModeFilter == VerbosityMode::All, selectableFlags))
            {
                m_verboseModeFilter = VerbosityMode::All;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            // Row 2: Info (Light White/Very Light Gray)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(242, 242, 242, 80)); // Very light gray
            ImGui::Text("Info");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("Info_None");
            if (ImGui::Selectable("None", m_infoModeFilter == VerbosityMode::None, selectableFlags))
            {
                m_infoModeFilter    = VerbosityMode::None;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("Info_Filtered");
            if (ImGui::Selectable("Filtered", m_infoModeFilter == VerbosityMode::Filtered, selectableFlags))
            {
                m_infoModeFilter    = VerbosityMode::Filtered;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("Info_All");
            if (ImGui::Selectable("All", m_infoModeFilter == VerbosityMode::All, selectableFlags))
            {
                m_infoModeFilter    = VerbosityMode::All;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            // Row 3: Warning (Light Yellow)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(255, 240, 150, 80)); // Light yellow
            ImGui::Text("Warning");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("Warning_None");
            if (ImGui::Selectable("None", m_warningModeFilter == VerbosityMode::None, selectableFlags))
            {
                m_warningModeFilter = VerbosityMode::None;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("Warning_Filtered");
            if (ImGui::Selectable("Filtered", m_warningModeFilter == VerbosityMode::Filtered, selectableFlags))
            {
                m_warningModeFilter = VerbosityMode::Filtered;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("Warning_All");
            if (ImGui::Selectable("All", m_warningModeFilter == VerbosityMode::All, selectableFlags))
            {
                m_warningModeFilter = VerbosityMode::All;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            // Row 4: Error (Light Red)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(255, 180, 180, 80)); // Light red
            ImGui::Text("Error");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("Error_None");
            if (ImGui::Selectable("None", m_errorModeFilter == VerbosityMode::None, selectableFlags))
            {
                m_errorModeFilter   = VerbosityMode::None;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("Error_Filtered");
            if (ImGui::Selectable("Filtered", m_errorModeFilter == VerbosityMode::Filtered, selectableFlags))
            {
                m_errorModeFilter   = VerbosityMode::Filtered;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("Error_All");
            if (ImGui::Selectable("All", m_errorModeFilter == VerbosityMode::All, selectableFlags))
            {
                m_errorModeFilter   = VerbosityMode::All;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            // Row 5: Fatal (Light Dark Red)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(220, 120, 140, 80)); // Light dark red
            ImGui::Text("Fatal");

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID("Fatal_None");
            if (ImGui::Selectable("None", m_fatalModeFilter == VerbosityMode::None, selectableFlags))
            {
                m_fatalModeFilter   = VerbosityMode::None;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID("Fatal_Filtered");
            if (ImGui::Selectable("Filtered", m_fatalModeFilter == VerbosityMode::Filtered, selectableFlags))
            {
                m_fatalModeFilter   = VerbosityMode::Filtered;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID("Fatal_All");
            if (ImGui::Selectable("All", m_fatalModeFilter == VerbosityMode::All, selectableFlags))
            {
                m_fatalModeFilter   = VerbosityMode::All;
                m_needsFilterUpdate = true;
            }
            ImGui::PopID();

            ImGui::EndTable();
        }

#endif

#undef USE_SPLIT_TABLE_LAYOUT
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::RenderCategoryFilter()
    {
        // Update category counts
        UpdateCategoryCounts();

        ImGui::Indent();

        // If no categories, show prompt
        if (m_allCategories.empty())
        {
            ImGui::TextDisabled("(No categories yet)");
        }
        else
        {
            // Iterate through all categories, display checkboxes
            for (const auto& category : m_allCategories)
            {
                // Ensure category exists in m_categoryEnabled (default enabled)
                if (m_categoryEnabled.find(category) == m_categoryEnabled.end())
                {
                    m_categoryEnabled[category] = true;
                }

                bool& enabled = m_categoryEnabled[category];
                int   count   = m_categoryCounts[category];

                // Display format: "LogGame (123)"
                std::string label = category + " (" + std::to_string(count) + ")";

                if (ImGui::Checkbox(label.c_str(), &enabled))
                {
                    m_needsFilterUpdate = true;
                }
            }
        }

        ImGui::Unindent();
    }

    // ================================================================================================
    // Categories popup rendering
    // ================================================================================================

    void MessageLogUI::RenderCategoryPopup()
    {
        // Set popup size (width ~200px, height ~300px)
        ImGui::SetNextWindowSize(ImVec2(200.0f, 300.0f), ImGuiCond_FirstUseEver);

        if (ImGui::BeginPopup("CategoriesPopup"))
        {
            // Search box
            ImGui::PushItemWidth(-1.0f); // Fill entire width
            ImGui::InputTextWithHint("##CategorySearch", "Search...", m_categorySearchBuffer, sizeof(m_categorySearchBuffer));
            ImGui::PopItemWidth();

            ImGui::Separator();

            // Scrollable checkbox list
            ImGui::BeginChild("CategoryList", ImVec2(0, 0), false);

            // Filter categories (based on search box)
            std::string searchLower = m_categorySearchBuffer;
            std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

            UpdateCategoryCounts();

            for (const auto& category : m_allCategories)
            {
                // If there's search text, filter
                if (!searchLower.empty())
                {
                    std::string categoryLower = category;
                    std::transform(categoryLower.begin(), categoryLower.end(), categoryLower.begin(), ::tolower);

                    if (categoryLower.find(searchLower) == std::string::npos)
                    {
                        continue; // Doesn't match search, skip
                    }
                }

                // Display checkbox
                std::string label = category + " (" + std::to_string(m_categoryCounts[category]) + ")";

                bool isEnabled = m_categoryEnabled[category];
                if (ImGui::Checkbox(label.c_str(), &isEnabled))
                {
                    m_categoryEnabled[category] = isEnabled;
                    m_needsFilterUpdate         = true;
                }
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    // ================================================================================================
    // Main panel rendering (fullscreen log display area)
    // ================================================================================================
    void MessageLogUI::RenderMainPanel()
    {
        // Use all remaining available space as log display area
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        availSize.y -= 60.0f; // Reserve space for bottom toolbar

        ImGui::BeginChild("LogPanel", availSize, false, ImGuiWindowFlags_HorizontalScrollbar);

        // Use ImGuiListClipper for virtual scrolling, improves performance
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_filteredIndices.size()));

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                size_t                msgIndex = m_filteredIndices[i];
                const DisplayMessage& msg      = m_allMessages[msgIndex];

                // Get level color
                ImVec4 color = GetLevelColor(msg.level);

                // Display format: "[HH:MM:SS] LogGame: Game initialized successfully"
                // Timestamp displayed in gray, other parts use level color
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", msg.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextColored(color, "%s: %s", msg.category.c_str(), msg.message.c_str());
            }
        }

        clipper.End();

        // Auto-scroll to bottom
        if (m_scrollToBottom && m_config.autoScroll)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }

        ImGui::EndChild();
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::RenderBottomToolbar()
    {
        ImGui::BeginGroup();

        // Clear button
        if (ImGui::Button("Clear"))
        {
            Clear();
        }

        ImGui::SameLine();

        // Export button
        if (ImGui::Button("Export"))
        {
            ExportToFile("MessageLog_Export.txt");
        }

        ImGui::SameLine();

        // Status information
        ImGui::Text("Total: %zu | Filtered: %zu",
                    m_allMessages.size(),
                    m_filteredIndices.size());

        ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);

        // Auto-scroll checkbox
        ImGui::Checkbox("Auto-scroll", &m_config.autoScroll);

        ImGui::EndGroup();
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::AddMessage(const std::string& category, const std::string& level, const std::string& message)
    {
        // Get current timestamp
        std::string timestamp = GetCurrentTimestamp();

        // Create message
        DisplayMessage msg(timestamp, category, level, message);

        // Add to message list
        m_allMessages.push_back(msg);

        // Limit message count
        if (m_allMessages.size() > m_config.maxMessages)
        {
            m_allMessages.pop_front();
        }

        // Record category (if new category, enable by default)
        if (m_allCategories.find(category) == m_allCategories.end())
        {
            m_allCategories.insert(category);
            m_categoryEnabled[category] = true; // Enable new category by default
        }

        // Mark needs filter update
        m_needsFilterUpdate = true;
        m_scrollToBottom    = true;
    }

    void MessageLogUI::Clear()
    {
        m_allMessages.clear();
        m_filteredIndices.clear();
        m_allCategories.clear();
        m_categoryEnabled.clear();
        m_categoryCounts.clear();
        m_needsFilterUpdate = true;

        // Add clear prompt message
        AddMessage("LogSystem", "Info", "Message log cleared");
    }

    void MessageLogUI::ExportToFile(const std::string& filepath)
    {
        std::ofstream outFile(filepath);
        if (!outFile.is_open())
        {
            AddMessage("LogSystem", "Error", "Failed to export to file: " + filepath);
            return;
        }

        // Write header
        outFile << "MessageLog Export - " << GetCurrentTimestamp() << "\n";
        outFile << "Total Messages: " << m_allMessages.size() << "\n";
        outFile << "========================================\n\n";

        // Write messages
        for (const auto& msg : m_allMessages)
        {
            outFile << "[" << msg.timestamp << "] ";
            outFile << msg.category << ": ";
            outFile << msg.message;
            outFile << " [" << msg.level << "]";
            outFile << "\n";
        }

        outFile.close();
        AddMessage("LogSystem", "Info", "Exported " + std::to_string(m_allMessages.size()) + " messages to " + filepath);
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::ApplyFilter()
    {
        m_filteredIndices.clear();

        for (size_t i = 0; i < m_allMessages.size(); ++i)
        {
            if (PassesFilter(m_allMessages[i]))
            {
                m_filteredIndices.push_back(i);
            }
        }
    }

    bool MessageLogUI::PassesFilter(const DisplayMessage& msg) const
    {
        // Helper function: get VerbosityMode for specified level
        VerbosityMode mode = VerbosityMode::All;
        if (msg.level == "Verbose")
            mode = m_verboseModeFilter;
        else if (msg.level == "Info")
            mode = m_infoModeFilter;
        else if (msg.level == "Warning")
            mode = m_warningModeFilter;
        else if (msg.level == "Error")
            mode = m_errorModeFilter;
        else if (msg.level == "Fatal")
            mode = m_fatalModeFilter;

        // 1. Verbosity level filtering (three-choice)
        if (mode == VerbosityMode::None)
        {
            return false; // None - don't display
        }
        else if (mode == VerbosityMode::Filtered)
        {
            // Filtered - needs to pass through search box filter
            if (m_searchBuffer[0] != '\0')
            {
                std::string lowerSearch = ToLowerCase(m_searchBuffer);
                if (msg.searchableText.find(lowerSearch) == std::string::npos)
                    return false; // Doesn't match search, don't display
            }
            // If search box is empty, don't display (Filtered mode requires search criteria)
            else
            {
                return false;
            }
        }
        // When mode == VerbosityMode::All, ignore search box, continue checking category

        // 2. Category filtering (multiple-choice)
        auto it = m_categoryEnabled.find(msg.category);
        if (it != m_categoryEnabled.end() && !it->second)
        {
            return false; // Category not enabled
        }

        return true;
    }

    // ================================================================================================
    // ================================================================================================
    void MessageLogUI::UpdateCategoryCounts()
    {
        // Reset counts
        m_categoryCounts.clear();

        // Count messages
        for (const auto& msg : m_allMessages)
        {
            m_categoryCounts[msg.category]++;
        }
    }

    // ================================================================================================
    // ================================================================================================
    ImVec4 MessageLogUI::GetLevelColor(const std::string& level) const
    {
        // Verbose: Gray
        if (level == "Verbose")
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

        // Info: White
        if (level == "Info")
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Warning: Yellow
        if (level == "Warning")
            return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);

        // Error: Red
        if (level == "Error")
            return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

        // Fatal: Dark red/Magenta
        if (level == "Fatal")
            return ImVec4(0.8f, 0.0f, 0.2f, 1.0f);

        // Default: White
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    std::string MessageLogUI::GetCurrentTimestamp() const
    {
        auto now  = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        struct tm timeinfo;
        localtime_s(&timeinfo, &time);

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        return std::string(buffer);
    }

    std::string MessageLogUI::ToLowerCase(const std::string& str) const
    {
        std::string result = str;
        for (char& c : result)
        {
            c = static_cast<char>(tolower(c));
        }
        return result;
    }

    void MessageLogUI::ToggleWindow()
    {
        m_config.showWindow = !m_config.showWindow;
    }
}
