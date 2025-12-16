#pragma once
//-----------------------------------------------------------------------------------------------
// ProgramFallbackChain.hpp
//
// [NEW] Manages shader program fallback rules for graceful degradation
//
// This class provides:
//   - Loading fallback rules from fallback_rule.json
//   - Generating fallback chains for shader programs
//   - Enable/disable control for fallback behavior
//
// Usage:
//   ProgramFallbackChain fallbackChain;
//   fallbackChain.LoadRules(shaderPackPath / "shaders" / "fallback_rule.json");
//   auto chain = fallbackChain.GetFallbackChain("gbuffers_clouds");
//   // chain = ["gbuffers_clouds", "gbuffers_textured", "gbuffers_basic"]
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Only handles fallback rule management
//   - Open/Closed: FallbackRule structure can be extended without modifying this class
//   - Fail-safe: LoadRules fails silently (fallback rules are optional)
//
// Teaching Points:
//   - std::optional for graceful error handling from JsonHelper
//   - std::unordered_set for O(1) cycle detection in fallback chains
//   - Separation of loading logic from chain generation
//
//-----------------------------------------------------------------------------------------------

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // ProgramFallbackChain
    //
    // [NEW] Manages shader program fallback rules
    //
    // Fallback rules define how the system should try alternative shaders when
    // a requested shader program is not found. The chain format is:
    //   [requested_program, fallback1, fallback2, ..., default_program]
    //
    // Example fallback_rule.json:
    // {
    //   "default": "gbuffers_basic",
    //   "fallbacks": {
    //     "gbuffers_clouds": ["gbuffers_textured"],
    //     "gbuffers_textured": ["gbuffers_basic"]
    //   }
    // }
    //
    // GetFallbackChain("gbuffers_clouds") would return:
    //   ["gbuffers_clouds", "gbuffers_textured", "gbuffers_basic"]
    //-------------------------------------------------------------------------------------------
    class ProgramFallbackChain
    {
    public:
        //-------------------------------------------------------------------------------------------
        // LoadRules
        //
        // [NEW] Load fallback rules from JSON file
        //
        // Parameters:
        //   fallbackRuleJsonPath - Full path to fallback_rule.json
        //
        // Behavior:
        //   - Uses JsonHelper::ParseFallbackRuleJson() for parsing
        //   - Sets m_hasRules = true if successful
        //   - Sets m_enabled = true by default if rules loaded
        //   - Fails silently (no exception) if file missing or invalid
        //
        // Note: Fallback rules are optional - a ShaderBundle can work without them
        //-------------------------------------------------------------------------------------------
        void LoadRules(const std::filesystem::path& fallbackRuleJsonPath);

        //-------------------------------------------------------------------------------------------
        // GetFallbackChain
        //
        // [NEW] Generate fallback chain for a program name
        //
        // Parameters:
        //   programName - The shader program name to get fallback chain for
        //
        // Returns:
        //   Vector of program names: [programName, fallback1, fallback2, ..., defaultProgram]
        //   Empty vector if rules not loaded or disabled
        //
        // Algorithm:
        //   1. Start with requested program
        //   2. Follow fallback chain (use first fallback in each list)
        //   3. Use visited set to prevent infinite loops
        //   4. Append default program at end if not already in chain
        //
        // Example:
        //   GetFallbackChain("gbuffers_clouds")
        //   -> ["gbuffers_clouds", "gbuffers_textured", "gbuffers_basic"]
        //-------------------------------------------------------------------------------------------
        std::vector<std::string> GetFallbackChain(const std::string& programName) const;

        //-------------------------------------------------------------------------------------------
        // Query Methods
        //-------------------------------------------------------------------------------------------

        // Returns true if fallback rules were successfully loaded
        bool HasRules() const { return m_hasRules; }

        // Returns true if fallback is currently enabled
        bool IsEnabled() const { return m_enabled; }

        // Enable or disable fallback behavior
        void SetEnabled(bool enabled) { m_enabled = enabled; }

    private:
        FallbackRule m_rules; // Loaded fallback rules
        bool         m_hasRules = false; // True if rules successfully loaded
        bool         m_enabled  = false; // True if fallback is enabled
    };
} // namespace enigma::graphic
