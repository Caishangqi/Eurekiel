//-----------------------------------------------------------------------------------------------
// ProgramFallbackChain.cpp
//
// [NEW] Implementation of shader program fallback rule management
//
//-----------------------------------------------------------------------------------------------

#include "ProgramFallbackChain.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/Helper/JsonHelper.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // LoadRules
    //-------------------------------------------------------------------------------------------
    void ProgramFallbackChain::LoadRules(const std::filesystem::path& fallbackRuleJsonPath)
    {
        // Attempt to parse fallback rules from JSON
        // Note: JsonHelper::ParseFallbackRuleJson returns nullopt if file doesn't exist or parse fails
        std::optional<FallbackRule> ruleOpt = JsonHelper::ParseFallbackRuleJson(fallbackRuleJsonPath);

        if (!ruleOpt)
        {
            // Failed to load - this is acceptable as fallback rules are optional
            m_hasRules = false;
            m_enabled  = false;
            LogWarn(LogShaderBundle, "Failed to load fallback rules from: %s (this is optional)",
                    fallbackRuleJsonPath.string().c_str());
            return;
        }

        // Successfully loaded rules
        m_rules    = std::move(*ruleOpt);
        m_hasRules = true;
        m_enabled  = true; // Enable by default when rules are loaded

        LogInfo(LogShaderBundle, "Loaded fallback rules with default: %s, %zu fallback chains",
                m_rules.defaultProgram.c_str(), m_rules.fallbacks.size());
    }

    //-------------------------------------------------------------------------------------------
    // GetFallbackChain
    //-------------------------------------------------------------------------------------------
    std::vector<std::string> ProgramFallbackChain::GetFallbackChain(const std::string& programName) const
    {
        // Return empty chain if disabled or no rules loaded
        if (!m_enabled || !m_hasRules)
        {
            return {};
        }

        std::vector<std::string> chain;
        chain.push_back(programName); // Start with the requested program

        // Use visited set to prevent infinite loops in fallback chains
        // Teaching Point: O(1) lookup with unordered_set vs O(n) with vector
        std::unordered_set<std::string> visited;
        visited.insert(programName);

        // Follow the fallback chain
        std::string current = programName;
        while (true)
        {
            // Check if current program has a fallback defined
            auto it = m_rules.fallbacks.find(current);
            if (it == m_rules.fallbacks.end())
            {
                // No fallback defined for current program
                break;
            }

            const std::vector<std::string>& fallbackList = it->second;
            if (fallbackList.empty())
            {
                // Empty fallback list
                break;
            }

            // Use the first fallback in the list
            const std::string& nextProgram = fallbackList[0];

            // Check for cycle (infinite loop prevention)
            if (visited.count(nextProgram) > 0)
            {
                LogWarn(LogShaderBundle, "Cycle detected in fallback chain at: %s", nextProgram.c_str());
                break;
            }

            // Add to chain and continue
            chain.push_back(nextProgram);
            visited.insert(nextProgram);
            current = nextProgram;
        }

        // Append default program at the end if not already in chain
        // Teaching Point: std::find returns iterator to end if not found
        if (std::find(chain.begin(), chain.end(), m_rules.defaultProgram) == chain.end())
        {
            chain.push_back(m_rules.defaultProgram);
        }

        return chain;
    }
} // namespace enigma::graphic
