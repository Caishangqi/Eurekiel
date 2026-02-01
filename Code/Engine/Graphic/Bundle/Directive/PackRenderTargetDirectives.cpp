// ============================================================================
// [REFACTOR] PackRenderTargetDirectives.cpp - RT format directive parser impl
// Part of RenderTarget Format Refactor Feature
// ============================================================================

#include "PackRenderTargetDirectives.hpp"
#include "Engine/Graphic/Shader/Program/Parsing/ConstDirectiveParser.hpp"
#include "Engine/Graphic/Shader/Program/Parsing/DXGIFormatParser.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace enigma::graphic
{
    namespace
    {
        // Trim whitespace from string
        std::string Trim(const std::string& str)
        {
            size_t start = str.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            size_t end = str.find_last_not_of(" \t\r\n");
            return str.substr(start, end - start + 1);
        }

        // Convert to lowercase for comparison
        std::string ToLower(const std::string& str)
        {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return result;
        }
    } // anonymous namespace

    // ============================================================================
    // Constructor
    // ============================================================================

    PackRenderTargetDirectives::PackRenderTargetDirectives(
        const RenderTargetConfig& defaultColorConfig,
        const RenderTargetConfig& defaultDepthConfig,
        const RenderTargetConfig& defaultShadowColorConfig,
        const RenderTargetConfig& defaultShadowTexConfig)
        : m_defaultColorConfig(defaultColorConfig)
          , m_defaultDepthConfig(defaultDepthConfig)
          , m_defaultShadowColorConfig(defaultShadowColorConfig)
          , m_defaultShadowTexConfig(defaultShadowTexConfig)
    {
    }

    // ============================================================================
    // Directive Parsing
    // ============================================================================

    void PackRenderTargetDirectives::AcceptDirectives(const ConstDirectiveParser& parser)
    {
        // Parse colortexNClear (bool) and colortexNClearColor (vec4/float4)
        for (int i = 0; i < MAX_COLOR_TEXTURES; ++i)
        {
            // colortexNClear
            std::string clearName = "colortex" + std::to_string(i) + "Clear";
            auto        clearOpt  = parser.GetBool(clearName);
            if (clearOpt.has_value())
            {
                ApplyClear(m_colorTexConfigs, i, clearOpt.value(), m_defaultColorConfig, MAX_COLOR_TEXTURES);
            }

            // colortexNClearColor
            std::string clearColorName = "colortex" + std::to_string(i) + "ClearColor";
            auto        clearColorOpt  = parser.GetVec4(clearColorName);
            if (clearColorOpt.has_value())
            {
                ApplyClearColor(m_colorTexConfigs, i, clearColorOpt.value(), m_defaultColorConfig, MAX_COLOR_TEXTURES);
            }
        }

        // Parse depthtexNClear
        for (int i = 0; i < MAX_DEPTH_TEXTURES; ++i)
        {
            std::string clearName = "depthtex" + std::to_string(i) + "Clear";
            auto        clearOpt  = parser.GetBool(clearName);
            if (clearOpt.has_value())
            {
                ApplyClear(m_depthTexConfigs, i, clearOpt.value(), m_defaultDepthConfig, MAX_DEPTH_TEXTURES);
            }
        }

        // Parse shadowcolorNClear and shadowcolorNClearColor
        for (int i = 0; i < MAX_SHADOW_COLORS; ++i)
        {
            std::string clearName = "shadowcolor" + std::to_string(i) + "Clear";
            auto        clearOpt  = parser.GetBool(clearName);
            if (clearOpt.has_value())
            {
                ApplyClear(m_shadowColorConfigs, i, clearOpt.value(), m_defaultShadowColorConfig, MAX_SHADOW_COLORS);
            }

            std::string clearColorName = "shadowcolor" + std::to_string(i) + "ClearColor";
            auto        clearColorOpt  = parser.GetVec4(clearColorName);
            if (clearColorOpt.has_value())
            {
                ApplyClearColor(m_shadowColorConfigs, i, clearColorOpt.value(), m_defaultShadowColorConfig, MAX_SHADOW_COLORS);
            }
        }

        // Parse shadowtexNClear
        for (int i = 0; i < MAX_SHADOW_TEXTURES; ++i)
        {
            std::string clearName = "shadowtex" + std::to_string(i) + "Clear";
            auto        clearOpt  = parser.GetBool(clearName);
            if (clearOpt.has_value())
            {
                ApplyClear(m_shadowTexConfigs, i, clearOpt.value(), m_defaultShadowTexConfig, MAX_SHADOW_TEXTURES);
            }
        }
    }

    void PackRenderTargetDirectives::ParseFormatDirectives(const std::vector<std::string>& lines)
    {
        for (const auto& line : lines)
        {
            ParseFormatLine(line);
        }
    }

    bool PackRenderTargetDirectives::ParseFormatLine(const std::string& line)
    {
        // Format directives are in comments: const int colortexNFormat = FORMAT_NAME;
        // Regex: const\s+int\s+(\w+Format)\s*=\s*(\w+)\s*;

        std::string trimmed = Trim(line);

        // Skip empty lines
        if (trimmed.empty()) return false;

        // Check for "const int" pattern
        static const std::regex formatRegex(
            R"(const\s+int\s+(\w+Format)\s*=\s*(\w+)\s*;?)",
            std::regex::icase
        );

        std::smatch match;
        if (!std::regex_search(trimmed, match, formatRegex))
        {
            return false;
        }

        std::string directiveName = match[1].str();
        std::string formatName    = match[2].str();

        // Parse format
        auto formatOpt = DXGIFormatParser::Parse(formatName);
        if (!formatOpt.has_value())
        {
            core::LogWarn("PackRenderTargetDirectives",
                          "Invalid format '%s' in directive '%s'",
                          formatName.c_str(), directiveName.c_str());
            return false;
        }

        DXGI_FORMAT format    = formatOpt.value();
        std::string lowerName = ToLower(directiveName);

        // Match colortexNFormat
        int index = ExtractIndex(lowerName, "colortex", "format");
        if (index >= 0 && index < MAX_COLOR_TEXTURES)
        {
            ApplyFormat(m_colorTexConfigs, index, format, m_defaultColorConfig, MAX_COLOR_TEXTURES);
            return true;
        }

        // Match depthtexNFormat
        index = ExtractIndex(lowerName, "depthtex", "format");
        if (index >= 0 && index < MAX_DEPTH_TEXTURES)
        {
            ApplyFormat(m_depthTexConfigs, index, format, m_defaultDepthConfig, MAX_DEPTH_TEXTURES);
            return true;
        }

        // Match shadowcolorNFormat
        index = ExtractIndex(lowerName, "shadowcolor", "format");
        if (index >= 0 && index < MAX_SHADOW_COLORS)
        {
            ApplyFormat(m_shadowColorConfigs, index, format, m_defaultShadowColorConfig, MAX_SHADOW_COLORS);
            return true;
        }

        // Match shadowtexNFormat
        index = ExtractIndex(lowerName, "shadowtex", "format");
        if (index >= 0 && index < MAX_SHADOW_TEXTURES)
        {
            ApplyFormat(m_shadowTexConfigs, index, format, m_defaultShadowTexConfig, MAX_SHADOW_TEXTURES);
            return true;
        }

        return false;
    }

    int PackRenderTargetDirectives::ExtractIndex(
        const std::string& name,
        const std::string& prefix,
        const std::string& suffix) const
    {
        // Check prefix
        if (name.find(prefix) != 0) return -1;

        // Check suffix
        size_t suffixPos = name.rfind(suffix);
        if (suffixPos == std::string::npos) return -1;

        // Extract number between prefix and suffix
        size_t      numStart = prefix.length();
        size_t      numEnd   = suffixPos;
        std::string numStr   = name.substr(numStart, numEnd - numStart);

        if (numStr.empty()) return -1;

        try
        {
            return std::stoi(numStr);
        }
        catch (...)
        {
            return -1;
        }
    }

    // ============================================================================
    // Apply Helpers
    // ============================================================================

    void PackRenderTargetDirectives::ApplyFormat(
        std::map<int, RenderTargetConfig>& configs,
        int                                index,
        DXGI_FORMAT                        format,
        const RenderTargetConfig&          defaultConfig,
        int                                maxIndex)
    {
        if (index < 0 || index >= maxIndex) return;

        // Get or create config entry
        auto it = configs.find(index);
        if (it == configs.end())
        {
            configs[index]        = defaultConfig;
            configs[index].format = format;
        }
        else
        {
            it->second.format = format;
        }
    }

    void PackRenderTargetDirectives::ApplyClear(
        std::map<int, RenderTargetConfig>& configs,
        int                                index,
        bool                               enableClear,
        const RenderTargetConfig&          defaultConfig,
        int                                maxIndex)
    {
        if (index < 0 || index >= maxIndex) return;

        auto it = configs.find(index);
        if (it == configs.end())
        {
            configs[index]            = defaultConfig;
            configs[index].loadAction = enableClear ? LoadAction::Clear : LoadAction::Load;
        }
        else
        {
            it->second.loadAction = enableClear ? LoadAction::Clear : LoadAction::Load;
        }
    }

    void PackRenderTargetDirectives::ApplyClearColor(
        std::map<int, RenderTargetConfig>& configs,
        int                                index,
        const Vec4&                        clearColor,
        const RenderTargetConfig&          defaultConfig,
        int                                maxIndex)
    {
        if (index < 0 || index >= maxIndex) return;

        auto it = configs.find(index);
        if (it == configs.end())
        {
            configs[index]            = defaultConfig;
            configs[index].clearValue = ClearValue::Color(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        }
        else
        {
            it->second.clearValue = ClearValue::Color(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
        }
    }

    // ============================================================================
    // Config Access - ColorTex
    // ============================================================================

    RenderTargetConfig PackRenderTargetDirectives::GetColorTexConfig(int index) const
    {
        auto it = m_colorTexConfigs.find(index);
        return (it != m_colorTexConfigs.end()) ? it->second : m_defaultColorConfig;
    }

    bool PackRenderTargetDirectives::HasColorTexConfig(int index) const
    {
        return m_colorTexConfigs.find(index) != m_colorTexConfigs.end();
    }

    int PackRenderTargetDirectives::GetMaxColorTexIndex() const
    {
        if (m_colorTexConfigs.empty()) return -1;
        return m_colorTexConfigs.rbegin()->first;
    }

    // ============================================================================
    // Config Access - DepthTex
    // ============================================================================

    RenderTargetConfig PackRenderTargetDirectives::GetDepthTexConfig(int index) const
    {
        auto it = m_depthTexConfigs.find(index);
        return (it != m_depthTexConfigs.end()) ? it->second : m_defaultDepthConfig;
    }

    bool PackRenderTargetDirectives::HasDepthTexConfig(int index) const
    {
        return m_depthTexConfigs.find(index) != m_depthTexConfigs.end();
    }

    int PackRenderTargetDirectives::GetMaxDepthTexIndex() const
    {
        if (m_depthTexConfigs.empty()) return -1;
        return m_depthTexConfigs.rbegin()->first;
    }

    // ============================================================================
    // Config Access - ShadowColor
    // ============================================================================

    RenderTargetConfig PackRenderTargetDirectives::GetShadowColorConfig(int index) const
    {
        auto it = m_shadowColorConfigs.find(index);
        return (it != m_shadowColorConfigs.end()) ? it->second : m_defaultShadowColorConfig;
    }

    bool PackRenderTargetDirectives::HasShadowColorConfig(int index) const
    {
        return m_shadowColorConfigs.find(index) != m_shadowColorConfigs.end();
    }

    int PackRenderTargetDirectives::GetMaxShadowColorIndex() const
    {
        if (m_shadowColorConfigs.empty()) return -1;
        return m_shadowColorConfigs.rbegin()->first;
    }

    // ============================================================================
    // Config Access - ShadowTex
    // ============================================================================

    RenderTargetConfig PackRenderTargetDirectives::GetShadowTexConfig(int index) const
    {
        auto it = m_shadowTexConfigs.find(index);
        return (it != m_shadowTexConfigs.end()) ? it->second : m_defaultShadowTexConfig;
    }

    bool PackRenderTargetDirectives::HasShadowTexConfig(int index) const
    {
        return m_shadowTexConfigs.find(index) != m_shadowTexConfigs.end();
    }

    int PackRenderTargetDirectives::GetMaxShadowTexIndex() const
    {
        if (m_shadowTexConfigs.empty()) return -1;
        return m_shadowTexConfigs.rbegin()->first;
    }

    // ============================================================================
    // Utility
    // ============================================================================

    void PackRenderTargetDirectives::Clear()
    {
        m_colorTexConfigs.clear();
        m_depthTexConfigs.clear();
        m_shadowColorConfigs.clear();
        m_shadowTexConfigs.clear();
    }

    std::string PackRenderTargetDirectives::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "=== PackRenderTargetDirectives ===\n";

        oss << "ColorTex configs: " << m_colorTexConfigs.size() << "\n";
        for (const auto& [idx, cfg] : m_colorTexConfigs)
        {
            oss << "  colortex" << idx << ": format=" << DXGIFormatParser::ToString(cfg.format)
                << ", clear=" << (cfg.loadAction == LoadAction::Clear ? "true" : "false") << "\n";
        }

        oss << "DepthTex configs: " << m_depthTexConfigs.size() << "\n";
        for (const auto& [idx, cfg] : m_depthTexConfigs)
        {
            oss << "  depthtex" << idx << ": format=" << DXGIFormatParser::ToString(cfg.format) << "\n";
        }

        oss << "ShadowColor configs: " << m_shadowColorConfigs.size() << "\n";
        for (const auto& [idx, cfg] : m_shadowColorConfigs)
        {
            oss << "  shadowcolor" << idx << ": format=" << DXGIFormatParser::ToString(cfg.format) << "\n";
        }

        oss << "ShadowTex configs: " << m_shadowTexConfigs.size() << "\n";
        for (const auto& [idx, cfg] : m_shadowTexConfigs)
        {
            oss << "  shadowtex" << idx << ": format=" << DXGIFormatParser::ToString(cfg.format) << "\n";
        }

        return oss.str();
    }
} // namespace enigma::graphic
