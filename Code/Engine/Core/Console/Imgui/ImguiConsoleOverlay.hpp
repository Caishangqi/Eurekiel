#pragma once

#include <string>
#include <vector>

namespace enigma::core
{
    class ImguiConsole;

    // Static-only overlay module for ImGui Console
    // Renders autocomplete suggestions and command history as a popup above the input line.
    // Autocomplete uses fuzzy subsequence matching with highlighted matched characters.
    // History shows when input is empty. Works in both bottom-docked and MessageLog-docked modes.
    class ImguiConsoleOverlay
    {
    public:
        // Static-only class - no instantiation
        ImguiConsoleOverlay()                                        = delete;
        ImguiConsoleOverlay(const ImguiConsoleOverlay&)              = delete;
        ImguiConsoleOverlay& operator=(const ImguiConsoleOverlay&)   = delete;

        // Main render entry (called after console window End())
        static void Render(ImguiConsole& console);

        // Quick check: are there any autocomplete matches for the given input?
        static bool HasAutocompleteSuggestions(const std::string& input);

        // Trigger conditions
        static bool ShouldShowAutocomplete(const ImguiConsole& console);
        static bool ShouldShowHistory(const ImguiConsole& console);

    private:
        // Autocomplete rendering
        static void RenderAutocompleteList(ImguiConsole& console);
        static std::vector<std::string> GetAutocompleteSuggestions(const std::string& input);
        static void RenderFuzzyMatchHighlight(const std::string& text, const std::string& pattern);

        // History rendering
        static void RenderHistoryList(ImguiConsole& console);

        // Interaction handling
        static void HandleOverlayNavigation(ImguiConsole& console, int itemCount);
        static void HandleOverlayMouseInteraction(ImguiConsole& console, int itemIndex, const std::string& itemText);

        // Fuzzy matching utility
        static bool FuzzyMatch(const std::string& text, const std::string& pattern, std::vector<int>* outMatchedIndices = nullptr);
    };
} // namespace enigma::core
