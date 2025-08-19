#pragma once
#include <string>
#include <unordered_map>
#include "Engine/Core/Rgba8.hpp"

namespace enigma::core {

    // ANSI color code manager for console text formatting
    class AnsiColorManager {
    public:
        AnsiColorManager();
        ~AnsiColorManager() = default;

        // Enable/disable ANSI color support
        void SetAnsiSupport(bool enabled) { m_ansiSupported = enabled; }
        bool IsAnsiSupported() const { return m_ansiSupported; }

        // Color formatting
        std::string FormatText(const std::string& text, const Rgba8& color) const;
        std::string FormatTextBold(const std::string& text, const Rgba8& color) const;
        std::string FormatBackground(const std::string& text, const Rgba8& backgroundColor) const;
        
        // Predefined color methods
        std::string Red(const std::string& text) const;
        std::string Green(const std::string& text) const;
        std::string Yellow(const std::string& text) const;
        std::string Blue(const std::string& text) const;
        std::string Magenta(const std::string& text) const;
        std::string Cyan(const std::string& text) const;
        std::string White(const std::string& text) const;
        std::string Gray(const std::string& text) const;
        
        // Control codes
        std::string Reset() const;
        std::string Bold() const;
        std::string Dim() const;
        std::string ClearScreen() const;
        std::string ClearLine() const;
        
        // Cursor control
        std::string MoveCursor(int row, int col) const;
        std::string SaveCursor() const;
        std::string RestoreCursor() const;
        std::string HideCursor() const;
        std::string ShowCursor() const;

    private:
        bool m_ansiSupported = true;
        
        // Convert Rgba8 to ANSI color code
        std::string RgbaToAnsi(const Rgba8& color, bool background = false) const;
        std::string GetNearestAnsiColor(const Rgba8& color, bool background = false) const;
        
        // ANSI color constants
        static const std::unordered_map<std::string, std::string> ANSI_COLORS;
        static const std::unordered_map<std::string, std::string> ANSI_BG_COLORS;
    };

} // namespace enigma::core