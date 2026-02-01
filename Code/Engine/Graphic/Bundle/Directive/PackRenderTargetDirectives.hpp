#pragma once

// ============================================================================
// [REFACTOR] PackRenderTargetDirectives.hpp - RT format directive parser
// Part of RenderTarget Format Refactor Feature
// ============================================================================

#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"
#include "Engine/Math/Vec4.hpp"
#include <map>
#include <string>
#include <vector>

namespace enigma::graphic
{
    // Forward declaration
    class ConstDirectiveParser;

    /**
     * @class PackRenderTargetDirectives
     * @brief Collects RT format configuration from shader source directives
     *
     * Parses Iris-compatible directives:
     * - colortexNFormat (in comments): RT format specification
     * - colortexNClear: Enable/disable clear per frame
     * - colortexNClearColor: Clear color value
     *
     * Similar directives for depthtex, shadowcolor, shadowtex.
     *
     * @note Based on Iris PackRenderTargetDirectives.java
     */
    class PackRenderTargetDirectives
    {
    public:
        /**
         * @brief Constructor with default configs for each RT type
         * @param defaultColorConfig Default config for colortex (from YAML)
         * @param defaultDepthConfig Default config for depthtex (from YAML)
         * @param defaultShadowColorConfig Default config for shadowcolor (from YAML)
         * @param defaultShadowTexConfig Default config for shadowtex (from YAML)
         */
        PackRenderTargetDirectives(
            const RTConfig& defaultColorConfig,
            const RTConfig& defaultDepthConfig,
            const RTConfig& defaultShadowColorConfig,
            const RTConfig& defaultShadowTexConfig
        );

        /**
         * @brief Accept and parse directives from ConstDirectiveParser
         * @param parser Parser containing parsed const directives
         *
         * Extracts clear and clearColor directives (valid HLSL values)
         */
        void AcceptDirectives(const ConstDirectiveParser& parser);

        /**
         * @brief Parse format directives from raw shader source lines
         * @param lines Shader source lines (including comments)
         *
         * Format directives must be in comments because format names
         * are not valid HLSL values:
         * @code
         * // const int colortex0Format = R16G16B16A16_FLOAT;
         * @endcode
         */
        void ParseFormatDirectives(const std::vector<std::string>& lines);

        // ========================================================================
        // ColorTex Config Access
        // ========================================================================

        RTConfig GetColorTexConfig(int index) const;
        bool     HasColorTexConfig(int index) const;
        int      GetMaxColorTexIndex() const;

        // ========================================================================
        // DepthTex Config Access
        // ========================================================================

        RTConfig GetDepthTexConfig(int index) const;
        bool     HasDepthTexConfig(int index) const;
        int      GetMaxDepthTexIndex() const;

        // ========================================================================
        // ShadowColor Config Access
        // ========================================================================

        RTConfig GetShadowColorConfig(int index) const;
        bool     HasShadowColorConfig(int index) const;
        int      GetMaxShadowColorIndex() const;

        // ========================================================================
        // ShadowTex Config Access
        // ========================================================================

        RTConfig GetShadowTexConfig(int index) const;
        bool     HasShadowTexConfig(int index) const;
        int      GetMaxShadowTexIndex() const;

        // ========================================================================
        // Utility
        // ========================================================================

        void        Clear();
        std::string GetDebugInfo() const;

    private:
        // ========================================================================
        // Internal Parsing Helpers
        // ========================================================================

        bool ParseFormatLine(const std::string& line);
        int  ExtractIndex(const std::string& name, const std::string& prefix, const std::string& suffix) const;

        void ApplyFormat(
            std::map<int, RTConfig>& configs,
            int                      index,
            DXGI_FORMAT              format,
            const RTConfig&          defaultConfig,
            int                      maxIndex
        );

        void ApplyClear(
            std::map<int, RTConfig>& configs,
            int                      index,
            bool                     enableClear,
            const RTConfig&          defaultConfig,
            int                      maxIndex
        );

        void ApplyClearColor(
            std::map<int, RTConfig>& configs,
            int                      index,
            const Vec4&              clearColor,
            const RTConfig&          defaultConfig,
            int                      maxIndex
        );

        // ========================================================================
        // Private Members
        // ========================================================================

        RTConfig m_defaultColorConfig;
        RTConfig m_defaultDepthConfig;
        RTConfig m_defaultShadowColorConfig;
        RTConfig m_defaultShadowTexConfig;

        std::map<int, RTConfig> m_colorTexConfigs;
        std::map<int, RTConfig> m_depthTexConfigs;
        std::map<int, RTConfig> m_shadowColorConfigs;
        std::map<int, RTConfig> m_shadowTexConfigs;
    };
} // namespace enigma::graphic
