#include "MessageLogUI.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <ThirdParty/imgui/imgui.h>
#include <ThirdParty/imgui/imgui_internal.h>
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
        if (!ImGui::Begin("MessageLog", &windowOpen))
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

            if (ImGui::BeginTable("LevelNamesTable", 1, leftTableFlags))
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
            std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                           [](unsigned char c)
                           {
                               int result = std::tolower(c);
                               return static_cast<char>(static_cast<unsigned char>(result));
                           });

            UpdateCategoryCounts();

            for (const auto& category : m_allCategories)
            {
                // If there's search text, filter
                if (!searchLower.empty())
                {
                    std::string categoryLower = category;
                    std::transform(categoryLower.begin(), categoryLower.end(), categoryLower.begin(), [](unsigned char c) -> char
                    {
                        return static_cast<char>(std::tolower(c));
                    });

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
    // Main panel rendering (fullscreen log display area) - WITH MULTI-SELECTION SUPPORT
    // ================================================================================================
    void MessageLogUI::RenderMainPanel()
    {
        // Use all remaining available space as log display area
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        availSize.y      -= 60.0f; // Reserve space for bottom toolbar

        ImGui::BeginChild("LogPanel", availSize, false, ImGuiWindowFlags_HorizontalScrollbar);

        // ========================================
        // Box Selection: Start detection (in empty space)
        // ========================================
        ImGuiIO&   io          = ImGui::GetIO();
        const bool isCtrlHeld  = io.KeyCtrl;
        const bool isShiftHeld = io.KeyShift;

        // Detect box selection start (left click in window)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isShiftHeld)
        {
            // Only start box select if clicking on empty space (not on a Selectable)
            // We'll check this by seeing if mouse was over any Selectable later
            // For now, mark as potentially starting box select
            m_isBoxSelecting   = true;
            m_boxSelectStart   = ImGui::GetMousePos();
            m_boxSelectEnd     = m_boxSelectStart;
            m_boxSelectScrollY = ImGui::GetScrollY(); // Save scroll position at start
            // Save initial state: if Ctrl is held, we're adding to selection; otherwise start fresh
            if (isCtrlHeld)
            {
                m_boxSelectInitialSelection = m_selectedMessageIndices; // Save initial state for additive selection
            }
            else
            {
                m_boxSelectInitialSelection.clear(); // Start with empty selection
            }
        }

        // ========================================
        // Box Selection: Update (if active)
        // ========================================
        if (m_isBoxSelecting)
        {
            m_boxSelectEnd = ImGui::GetMousePos();

            // Calculate scroll offset since box selection started
            float currentScrollY = ImGui::GetScrollY();
            float scrollDelta    = currentScrollY - m_boxSelectScrollY;

            // Draw selection rectangle (adjusted for scroll)
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            // Adjust box coordinates by scroll delta
            ImVec2 adjustedStart = ImVec2(m_boxSelectStart.x, m_boxSelectStart.y - scrollDelta);
            ImVec2 adjustedEnd   = ImVec2(m_boxSelectEnd.x, m_boxSelectEnd.y);

            ImVec2 rectMin = ImVec2(ImMin(adjustedStart.x, adjustedEnd.x),
                                    ImMin(adjustedStart.y, adjustedEnd.y));
            ImVec2 rectMax = ImVec2(ImMax(adjustedStart.x, adjustedEnd.x),
                                    ImMax(adjustedStart.y, adjustedEnd.y));

            // Draw border and filled rectangle
            drawList->AddRect(rectMin, rectMax, IM_COL32(100, 150, 255, 255), 0.0f, 0, 2.0f);
            drawList->AddRectFilled(rectMin, rectMax, IM_COL32(100, 150, 255, 30));

            // End box selection when mouse released
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_isBoxSelecting = false;
            }
        }

        // ========================================
        // Keyboard Shortcuts
        // ========================================
        if (ImGui::IsWindowFocused())
        {
            // Ctrl+A: Select all
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
            {
                ClearSelection();
                for (int i = 0; i < static_cast<int>(m_filteredIndices.size()); ++i)
                {
                    SelectMessage(i, true);
                }
            }

            // Ctrl+C: Copy selected messages
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && !m_selectedMessageIndices.empty())
            {
                CopySelectedMessagesToClipboard(false);
            }

            // Escape: Clear selection
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                ClearSelection();
            }

            // Arrow keys for navigation (single selection mode compatibility)
            if (m_selectedMessageIndex >= 0)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
                {
                    if (m_selectedMessageIndex > 0)
                        m_selectedMessageIndex--;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
                {
                    if (m_selectedMessageIndex < static_cast<int>(m_filteredIndices.size()) - 1)
                        m_selectedMessageIndex++;
                }
            }
        }

        // ========================================
        // Box Selection: Calculate which items should be selected (before rendering)
        // ========================================
        std::vector<int> itemsInBoxSelection; // Track items currently in box
        if (m_isBoxSelecting)
        {
            // We'll build this list as we render items below
            itemsInBoxSelection.reserve(m_filteredIndices.size());
        }

        // ========================================
        // Render Message List with Clipper
        // ========================================
        // Use ImGuiListClipper for virtual scrolling, improves performance
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_filteredIndices.size()));

        // Track if any Selectable was clicked (for box select cancel)
        bool anyItemClicked = false;

        // First pass: collect items in box selection (if box selecting)
        // We need to do this in a first pass because clipper only renders visible items
        // But we want to track ALL items in the selection box
        if (m_isBoxSelecting)
        {
            // Calculate box selection rectangle (normalized)
            ImVec2 rectMin = ImVec2(ImMin(m_boxSelectStart.x, m_boxSelectEnd.x),
                                    ImMin(m_boxSelectStart.y, m_boxSelectEnd.y));
            ImVec2 rectMax = ImVec2(ImMax(m_boxSelectStart.x, m_boxSelectEnd.x),
                                    ImMax(m_boxSelectStart.y, m_boxSelectEnd.y));

            // For each visible item during rendering, we'll check if it's in the box
            // and add to itemsInBoxSelection
        }

        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                size_t                msgIndex = m_filteredIndices[i];
                const DisplayMessage& msg      = m_allMessages[msgIndex];

                // Get level color
                ImVec4 color = GetLevelColor(msg.level);

                // Check if this message is selected (use multi-selection system)
                bool isSelected = IsMessageSelected(i);

                // During box selection, check if item should be selected based on current box
                if (m_isBoxSelecting)
                {
                    // We'll determine selection after getting item rect
                    // For now, use the calculated selection state
                    // (This will be updated below after we know if item is in box)
                }

                // Create display text: "[HH:MM:SS] LogGame: Game initialized successfully"
                std::string displayText = Stringf("[%s] %s: %s",
                                                  msg.timestamp.c_str(),
                                                  msg.category.c_str(),
                                                  msg.message.c_str());

                // Apply selection highlight colors
                if (isSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.6f, 0.9f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.7f, 1.0f, 0.7f));
                }

                // Use Selectable to allow selection
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                bool wasClicked = ImGui::Selectable(displayText.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick);
                ImGui::PopStyleColor(); // Pop Text color

                if (isSelected)
                {
                    ImGui::PopStyleColor(3); // Pop Header colors
                }

                // Get item rect for box selection test
                ImVec2 itemMin = ImGui::GetItemRectMin();
                ImVec2 itemMax = ImGui::GetItemRectMax();

                // ========================================
                // Box selection: Track items in box during rendering
                // ========================================
                if (m_isBoxSelecting && IsMessageInBoxSelection(i, itemMin, itemMax))
                {
                    itemsInBoxSelection.push_back(i);
                }

                // ========================================
                // Multi-selection logic (click handling)
                // ========================================
                if (wasClicked)
                {
                    anyItemClicked = true;

                    // Cancel box selection if we clicked on an item
                    if (m_isBoxSelecting)
                    {
                        m_isBoxSelecting = false;
                    }

                    if (isShiftHeld && m_lastClickedIndex >= 0)
                    {
                        // Shift+Click: Range selection
                        // Keep existing selection and add range from m_lastClickedIndex to current item
                        int startIdx = ImMin(m_lastClickedIndex, i);
                        int endIdx   = ImMax(m_lastClickedIndex, i);

                        // Don't clear selection - preserve existing selections
                        // SelectRange will add to current selection
                        SelectRange(startIdx, endIdx);
                        // Don't update m_lastClickedIndex for Shift+Click
                        // This allows extending the range from the same anchor point
                    }
                    else if (isCtrlHeld)
                    {
                        // Ctrl+Click: Toggle individual item selection
                        ToggleMessageSelection(i);
                        m_lastClickedIndex = i;
                    }
                    else
                    {
                        // Normal click: Clear other selections, select only this
                        ClearSelection();
                        SelectMessage(i, true);
                        m_lastClickedIndex = i;
                    }

                    // Update legacy single-selection index for compatibility
                    m_selectedMessageIndex = i;

                    // Double-click to copy (use multi-selection copy if multiple selected)
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (m_selectedMessageIndices.size() > 1)
                        {
                            CopySelectedMessagesToClipboard(false);
                        }
                        else
                        {
                            CopySelectedMessageToClipboard(false);
                        }
                    }
                }

                // ========================================
                // Right-click context menu
                // ========================================
                if (isSelected && ImGui::BeginPopupContextItem())
                {
                    // Display selected count
                    ImGui::Text("Selected: %d message(s)", static_cast<int>(m_selectedMessageIndices.size()));
                    ImGui::Separator();

                    if (ImGui::MenuItem("Copy Message(s) (Ctrl+C)"))
                    {
                        if (m_selectedMessageIndices.size() > 1)
                        {
                            CopySelectedMessagesToClipboard(false);
                        }
                        else
                        {
                            CopySelectedMessageToClipboard(false);
                        }
                    }

                    if (ImGui::MenuItem("Copy Full Details"))
                    {
                        if (m_selectedMessageIndices.size() > 1)
                        {
                            CopySelectedMessagesToClipboard(true);
                        }
                        else
                        {
                            CopySelectedMessageToClipboard(true);
                        }
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Select All (Ctrl+A)"))
                    {
                        ClearSelection();
                        for (int j = 0; j < static_cast<int>(m_filteredIndices.size()); ++j)
                        {
                            SelectMessage(j, true);
                        }
                    }

                    if (ImGui::MenuItem("Clear Selection (Esc)"))
                    {
                        ClearSelection();
                        m_selectedMessageIndex = -1;
                    }

                    ImGui::EndPopup();
                }
            }
        }

        clipper.End();

        // ========================================
        // Box Selection: Apply final selection state (after rendering all items)
        // ========================================
        if (m_isBoxSelecting)
        {
            // Rebuild selection: initial selection + items in current box
            // This ensures shrinking the box correctly removes items
            std::vector<int> newSelection = m_boxSelectInitialSelection;

            // Add all items currently in box
            for (int itemIdx : itemsInBoxSelection)
            {
                // Only add if not already in initial selection
                if (std::find(newSelection.begin(), newSelection.end(), itemIdx) == newSelection.end())
                {
                    newSelection.push_back(itemIdx);
                }
            }

            // Sort for consistency
            std::sort(newSelection.begin(), newSelection.end());

            // Apply the new selection
            m_selectedMessageIndices = newSelection;
        }

        // If box select ended on empty space, clear selection
        if (!m_isBoxSelecting && !anyItemClicked && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
        {
            ClearSelection();
            m_selectedMessageIndex = -1;
        }

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
    // Hybrid API: Type-safe Category + String Level
    // ================================================================================================
    void MessageLogUI::AddMessage(const LogCategoryBase& category, const std::string& level, const std::string& message)
    {
        AddMessage(category.GetName(), level, message);
    }

    // ================================================================================================
    // Legacy API: String Category + String Level (kept for backward compatibility)
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

        // Reset selection when clearing (both legacy and multi-selection)
        m_selectedMessageIndex = -1;
        ClearSelection(); // Clear multi-selection state

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

        // Reset legacy single selection if it's out of bounds after filtering
        if (m_selectedMessageIndex >= static_cast<int>(m_filteredIndices.size()))
        {
            m_selectedMessageIndex = -1;
        }

        // Clean up multi-selection: Remove any selected indices that are out of bounds
        // This is important because filtering changes the valid index range
        m_selectedMessageIndices.erase(
            std::remove_if(m_selectedMessageIndices.begin(),
                           m_selectedMessageIndices.end(),
                           [this](int idx)
                           {
                               return idx < 0 || idx >= static_cast<int>(m_filteredIndices.size());
                           }),
            m_selectedMessageIndices.end());
    }

    bool MessageLogUI::PassesFilter(const DisplayMessage& msg) const
    {
        // Helper function: get VerbosityMode for specified level
        // Convert level string to uppercase for case-insensitive comparison
        std::string levelUpper = msg.level;
        std::transform(levelUpper.begin(), levelUpper.end(), levelUpper.begin(), [](unsigned char c) -> char
        {
            return static_cast<char>(std::toupper(c));
        });

        VerbosityMode mode = VerbosityMode::All;

        // Map level strings to filters (case-insensitive)
        if (levelUpper == "TRACE" || levelUpper == "DEBUG" || levelUpper == "VERBOSE")
            mode = m_verboseModeFilter;
        else if (levelUpper == "INFO")
            mode = m_infoModeFilter;
        else if (levelUpper == "WARN" || levelUpper == "WARNING")
            mode = m_warningModeFilter;
        else if (levelUpper == "ERROR")
            mode = m_errorModeFilter;
        else if (levelUpper == "FATAL")
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
        // Convert level string to uppercase for case-insensitive comparison
        std::string levelUpper = level;
        std::transform(levelUpper.begin(), levelUpper.end(), levelUpper.begin(), [](unsigned char c) -> char
        {
            return static_cast<char>(std::toupper(c));
        });

        // Verbose/Debug/Trace: Gray
        if (levelUpper == "VERBOSE" || levelUpper == "DEBUG" || levelUpper == "TRACE")
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

        // Info: White
        if (levelUpper == "INFO")
            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Warning/Warn: Yellow
        if (levelUpper == "WARNING" || levelUpper == "WARN")
            return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);

        // Error: Red
        if (levelUpper == "ERROR")
            return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

        // Fatal: Dark red/Magenta
        if (levelUpper == "FATAL")
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

    // ================================================================================================
    // Copy selected message to clipboard
    // ================================================================================================
    void MessageLogUI::CopySelectedMessageToClipboard(bool includeMetadata)
    {
        // Validate selection
        if (m_selectedMessageIndex < 0 || m_selectedMessageIndex >= static_cast<int>(m_filteredIndices.size()))
            return;

        // Get the selected message
        size_t                msgIndex = m_filteredIndices[m_selectedMessageIndex];
        const DisplayMessage& msg      = m_allMessages[msgIndex];

        // Build text to copy
        std::string textToCopy;
        if (includeMetadata)
        {
            // Full format: [HH:MM:SS] [Level] [Category] Message
            textToCopy = Stringf("[%s] [%s] [%s] %s",
                                 msg.timestamp.c_str(),
                                 msg.level.c_str(),
                                 msg.category.c_str(),
                                 msg.message.c_str());
        }
        else
        {
            // Simple format: Message content only
            textToCopy = msg.message;
        }

        // Copy to clipboard using ImGui API
        ImGui::SetClipboardText(textToCopy.c_str());

        // Optional: Add a feedback message to log
        AddMessage("LogSystem", "Info", "Message copied to clipboard");
    }

    // ================================================================================================
    // Multi-selection helper methods
    // ================================================================================================

    bool MessageLogUI::IsMessageSelected(int index) const
    {
        return std::find(m_selectedMessageIndices.begin(),
                         m_selectedMessageIndices.end(),
                         index) != m_selectedMessageIndices.end();
    }

    void MessageLogUI::SelectMessage(int index, bool selected)
    {
        if (selected)
        {
            // Add to selection if not already selected
            if (!IsMessageSelected(index))
            {
                m_selectedMessageIndices.push_back(index);
                // Keep sorted for efficient range operations
                std::sort(m_selectedMessageIndices.begin(), m_selectedMessageIndices.end());
            }
        }
        else
        {
            // Remove from selection
            auto it = std::find(m_selectedMessageIndices.begin(),
                                m_selectedMessageIndices.end(),
                                index);
            if (it != m_selectedMessageIndices.end())
            {
                m_selectedMessageIndices.erase(it);
            }
        }
    }

    void MessageLogUI::ClearSelection()
    {
        m_selectedMessageIndices.clear();
        m_lastClickedIndex = -1;
    }

    void MessageLogUI::SelectRange(int startIndex, int endIndex)
    {
        // Ensure start <= end
        if (startIndex > endIndex)
        {
            std::swap(startIndex, endIndex);
        }

        // Select all messages in range
        for (int i = startIndex; i <= endIndex; ++i)
        {
            SelectMessage(i, true);
        }
    }

    void MessageLogUI::ToggleMessageSelection(int index)
    {
        if (IsMessageSelected(index))
        {
            SelectMessage(index, false); // Deselect
        }
        else
        {
            SelectMessage(index, true); // Select
        }
    }

    bool MessageLogUI::IsMessageInBoxSelection(int index, const ImVec2& itemMin, const ImVec2& itemMax) const
    {
        UNUSED(index);
        // Calculate scroll offset since box selection started
        float currentScrollY = ImGui::GetScrollY();
        float scrollDelta    = currentScrollY - m_boxSelectScrollY;

        // Adjust box start position by scroll delta
        ImVec2 adjustedStart = ImVec2(m_boxSelectStart.x, m_boxSelectStart.y - scrollDelta);
        ImVec2 adjustedEnd   = ImVec2(m_boxSelectEnd.x, m_boxSelectEnd.y);

        // Calculate box selection rectangle (normalized)
        ImVec2 rectMin = ImVec2(ImMin(adjustedStart.x, adjustedEnd.x),
                                ImMin(adjustedStart.y, adjustedEnd.y));
        ImVec2 rectMax = ImVec2(ImMax(adjustedStart.x, adjustedEnd.x),
                                ImMax(adjustedStart.y, adjustedEnd.y));

        // Check if item rectangle intersects with selection rectangle
        return !(itemMax.x < rectMin.x || itemMin.x > rectMax.x ||
            itemMax.y < rectMin.y || itemMin.y > rectMax.y);
    }

    // ================================================================================================
    // Multi-selection copy functionality
    // ================================================================================================

    void MessageLogUI::CopySelectedMessagesToClipboard(bool includeMetadata)
    {
        if (m_selectedMessageIndices.empty())
            return;

        std::string textToCopy;

        for (int idx : m_selectedMessageIndices)
        {
            // Validate index
            if (idx < 0 || idx >= static_cast<int>(m_filteredIndices.size()))
                continue;

            size_t msgIndex = m_filteredIndices[idx];
            if (msgIndex >= m_allMessages.size())
                continue;

            const DisplayMessage& msg = m_allMessages[msgIndex];

            // Add separator between messages
            if (!textToCopy.empty())
                textToCopy += "\n";

            // Build message text
            if (includeMetadata)
            {
                textToCopy += Stringf("[%s] [%s] [%s] %s",
                                      msg.timestamp.c_str(),
                                      msg.level.c_str(),
                                      msg.category.c_str(),
                                      msg.message.c_str());
            }
            else
            {
                textToCopy += msg.message;
            }
        }

        // Copy to clipboard
        ImGui::SetClipboardText(textToCopy.c_str());

        // Add feedback message
        AddMessage("LogSystem", "Info",
                   Stringf("Copied %d messages to clipboard", static_cast<int>(m_selectedMessageIndices.size())));
    }
}
