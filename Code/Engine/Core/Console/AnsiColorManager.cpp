#include "AnsiColorManager.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace enigma::core {

    const std::unordered_map<std::string, std::string> AnsiColorManager::ANSI_COLORS = {
        {"reset",     "\033[0m"},
        {"bold",      "\033[1m"},
        {"dim",       "\033[2m"},
        {"black",     "\033[30m"},
        {"red",       "\033[31m"},
        {"green",     "\033[32m"},
        {"yellow",    "\033[33m"},
        {"blue",      "\033[34m"},
        {"magenta",   "\033[35m"},
        {"cyan",      "\033[36m"},
        {"white",     "\033[37m"},
        {"gray",      "\033[90m"},
        {"bright_red",     "\033[91m"},
        {"bright_green",   "\033[92m"},
        {"bright_yellow",  "\033[93m"},
        {"bright_blue",    "\033[94m"},
        {"bright_magenta", "\033[95m"},
        {"bright_cyan",    "\033[96m"},
        {"bright_white",   "\033[97m"}
    };

    const std::unordered_map<std::string, std::string> AnsiColorManager::ANSI_BG_COLORS = {
        {"bg_black",   "\033[40m"},
        {"bg_red",     "\033[41m"},
        {"bg_green",   "\033[42m"},
        {"bg_yellow",  "\033[43m"},
        {"bg_blue",    "\033[44m"},
        {"bg_magenta", "\033[45m"},
        {"bg_cyan",    "\033[46m"},
        {"bg_white",   "\033[47m"}
    };

    AnsiColorManager::AnsiColorManager() {}

    std::string AnsiColorManager::FormatText(const std::string& text, const Rgba8& color) const {
        if (!m_ansiSupported) {
            return text;
        }
        
        return RgbaToAnsi(color) + text + Reset();
    }

    std::string AnsiColorManager::FormatTextBold(const std::string& text, const Rgba8& color) const {
        if (!m_ansiSupported) {
            return text;
        }
        
        return Bold() + RgbaToAnsi(color) + text + Reset();
    }

    std::string AnsiColorManager::FormatBackground(const std::string& text, const Rgba8& backgroundColor) const {
        if (!m_ansiSupported) {
            return text;
        }
        
        return RgbaToAnsi(backgroundColor, true) + text + Reset();
    }

    std::string AnsiColorManager::Red(const std::string& text) const {
        return FormatText(text, Rgba8::RED);
    }

    std::string AnsiColorManager::Green(const std::string& text) const {
        return FormatText(text, Rgba8::GREEN);
    }

    std::string AnsiColorManager::Yellow(const std::string& text) const {
        return FormatText(text, Rgba8::YELLOW);
    }

    std::string AnsiColorManager::Blue(const std::string& text) const {
        return FormatText(text, Rgba8::BLUE);
    }

    std::string AnsiColorManager::Magenta(const std::string& text) const {
        return FormatText(text, Rgba8::MAGENTA);
    }

    std::string AnsiColorManager::Cyan(const std::string& text) const {
        return FormatText(text, Rgba8::CYAN);
    }

    std::string AnsiColorManager::White(const std::string& text) const {
        return FormatText(text, Rgba8::WHITE);
    }

    std::string AnsiColorManager::Gray(const std::string& text) const {
        return FormatText(text, Rgba8::GRAY);
    }

    std::string AnsiColorManager::Reset() const {
        return m_ansiSupported ? ANSI_COLORS.at("reset") : "";
    }

    std::string AnsiColorManager::Bold() const {
        return m_ansiSupported ? ANSI_COLORS.at("bold") : "";
    }

    std::string AnsiColorManager::Dim() const {
        return m_ansiSupported ? ANSI_COLORS.at("dim") : "";
    }

    std::string AnsiColorManager::ClearScreen() const {
        return m_ansiSupported ? "\033[2J\033[H" : "";
    }

    std::string AnsiColorManager::ClearLine() const {
        return m_ansiSupported ? "\033[K" : "";
    }

    std::string AnsiColorManager::MoveCursor(int row, int col) const {
        if (!m_ansiSupported) return "";
        
        std::ostringstream oss;
        oss << "\033[" << row << ";" << col << "H";
        return oss.str();
    }

    std::string AnsiColorManager::SaveCursor() const {
        return m_ansiSupported ? "\033[s" : "";
    }

    std::string AnsiColorManager::RestoreCursor() const {
        return m_ansiSupported ? "\033[u" : "";
    }

    std::string AnsiColorManager::HideCursor() const {
        return m_ansiSupported ? "\033[?25l" : "";
    }

    std::string AnsiColorManager::ShowCursor() const {
        return m_ansiSupported ? "\033[?25h" : "";
    }

    std::string AnsiColorManager::RgbaToAnsi(const Rgba8& color, bool background) const {
        if (!m_ansiSupported) return "";
        
        // Use 24-bit color support (RGB)
        std::ostringstream oss;
        if (background) {
            oss << "\033[48;2;" << static_cast<int>(color.r) << ";" 
                << static_cast<int>(color.g) << ";" << static_cast<int>(color.b) << "m";
        } else {
            oss << "\033[38;2;" << static_cast<int>(color.r) << ";" 
                << static_cast<int>(color.g) << ";" << static_cast<int>(color.b) << "m";
        }
        return oss.str();
    }

    std::string AnsiColorManager::GetNearestAnsiColor(const Rgba8& color, bool background) const {
        if (!m_ansiSupported) return "";
        
        // Find nearest standard ANSI color for terminals that don't support 24-bit color
        struct ColorDistance {
            std::string name;
            double distance;
        };
        
        std::vector<ColorDistance> distances;
        
        // Standard ANSI colors with their RGB values
        std::vector<std::pair<std::string, Rgba8>> standardColors = {
            {"black",   Rgba8(0, 0, 0, 255)},
            {"red",     Rgba8(128, 0, 0, 255)},
            {"green",   Rgba8(0, 128, 0, 255)},
            {"yellow",  Rgba8(128, 128, 0, 255)},
            {"blue",    Rgba8(0, 0, 128, 255)},
            {"magenta", Rgba8(128, 0, 128, 255)},
            {"cyan",    Rgba8(0, 128, 128, 255)},
            {"white",   Rgba8(192, 192, 192, 255)}
        };
        
        for (const auto& standardColor : standardColors) {
            double distance = std::sqrt(
                std::pow(color.r - standardColor.second.r, 2) +
                std::pow(color.g - standardColor.second.g, 2) +
                std::pow(color.b - standardColor.second.b, 2)
            );
            distances.push_back({standardColor.first, distance});
        }
        
        // Find minimum distance
        auto minIt = std::min_element(distances.begin(), distances.end(),
            [](const ColorDistance& a, const ColorDistance& b) {
                return a.distance < b.distance;
            });
        
        if (minIt != distances.end()) {
            if (background) {
                auto bgColorName = "bg_" + minIt->name;
                auto it = ANSI_BG_COLORS.find(bgColorName);
                if (it != ANSI_BG_COLORS.end()) {
                    return it->second;
                }
            } else {
                auto it = ANSI_COLORS.find(minIt->name);
                if (it != ANSI_COLORS.end()) {
                    return it->second;
                }
            }
        }
        
        return "";
    }

} // namespace enigma::core