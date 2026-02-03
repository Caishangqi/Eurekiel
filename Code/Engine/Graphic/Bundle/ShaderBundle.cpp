//----------------------------------------------------------------------------------------------
// ShaderBundle.cpp
//
// [NEW] Implementation of ShaderBundle with three-tier fallback mechanism
//
//-----------------------------------------------------------------------------------------------

#include "ShaderBundle.hpp"

#include <regex>
#include <sstream>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/Helper/ShaderScanHelper.hpp"
#include "Engine/Graphic/Bundle/Directive/PackRenderTargetDirectives.hpp"
#include "Engine/Graphic/Shader/Program/Parsing/ConstDirectiveParser.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Constructor (RAII)
    //
    // [REFACTOR] Complete initialization in constructor following RAII pattern
    //-------------------------------------------------------------------------------------------
    ShaderBundle::ShaderBundle(const ShaderBundleMeta&       meta,
                               std::shared_ptr<ShaderBundle> engineBundle)
        : m_meta(meta)
          , m_engineBundle(std::move(engineBundle))
          , m_fallbackChain(std::make_unique<ProgramFallbackChain>())
    {
        LogInfo(LogShaderBundle, "ShaderBundle:: Initializing: %s (isEngine: %s)",
                m_meta.name.c_str(),
                m_meta.isEngineBundle ? "true" : "false");

        // Step 1: Load fallback rules from shaders/fallback_rule.json
        auto fallbackRulePath = m_meta.path / "shaders" / "fallback_rule.json";
        m_fallbackChain->LoadRules(fallbackRulePath);

        if (m_fallbackChain->HasRules())
        {
            LogInfo(LogShaderBundle, "ShaderBundle:: Fallback rules loaded from: %s",
                    fallbackRulePath.string().c_str());
        }
        else
        {
            LogInfo(LogShaderBundle, "ShaderBundle:: No fallback rules found (optional)");
        }

        // Step 2: Discover UserDefinedBundles from shaders/bundle/ directory
        auto bundleDir = m_meta.path / "shaders" / "bundle";
        if (FileSystemHelper::DirectoryExists(bundleDir))
        {
            auto subdirs = FileSystemHelper::ListSubdirectories(bundleDir);
            for (const auto& subdir : subdirs)
            {
                std::string bundleName = subdir.filename().string();
                LogInfo(LogShaderBundle, "ShaderBundle:: Discovering UserDefinedBundle: %s",
                        bundleName.c_str());

                auto userBundle = std::make_unique<UserDefinedBundle>(bundleName, subdir);

                // Precompile all programs in the bundle
                // Note: PrecompileAll logs warnings for failed compilations but doesn't throw
                userBundle->PrecompileAll();

                LogInfo(LogShaderBundle, "ShaderBundle:: UserDefinedBundle '%s' loaded with %zu programs",
                        bundleName.c_str(), userBundle->GetProgramCount());

                m_userDefinedBundles.push_back(std::move(userBundle));
            }

            // Set first bundle as current (if any exist)
            if (!m_userDefinedBundles.empty())
            {
                m_currentUserBundle = m_userDefinedBundles[0].get();
                LogInfo(LogShaderBundle, "ShaderBundle:: Current UserDefinedBundle set to: %s",
                        m_currentUserBundle->GetName().c_str());
            }
        }
        else
        {
            LogInfo(LogShaderBundle, "ShaderBundle:: No bundle/ directory found (using program/ only)");
        }

        // [NEW] Step 3: Parse RT directives from program/ folder
        const auto& config = g_theRendererSubsystem->GetConfiguration();
        m_rtDirectives     = std::make_unique<PackRenderTargetDirectives>(
            config.colorTexConfig.defaultConfig,
            config.depthTexConfig.defaultConfig,
            config.shadowColorConfig.defaultConfig,
            config.shadowTexConfig.defaultConfig
        );

        auto programDir = m_meta.path / "shaders" / "program";
        if (FileSystemHelper::DirectoryExists(programDir))
        {
            ConstDirectiveParser     constParser;
            std::vector<std::string> allLines;

            // Scan all .hlsl files in program/ folder
            for (const auto& entry : std::filesystem::directory_iterator(programDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".hlsl")
                {
                    std::string content;
                    if (FileReadToString(content, entry.path().string()) != 0 || content.empty())
                        continue;

                    // Split content into lines
                    std::vector<std::string> lines;
                    std::istringstream       stream(std::move(content));
                    std::string              line;
                    while (std::getline(stream, line))
                    {
                        lines.push_back(line);
                        allLines.push_back(line);
                    }

                    // Parse format directives from this file (in comments)
                    m_rtDirectives->ParseFormatDirectives(lines);
                }
            }

            // Parse const directives from all collected lines
            constParser.ParseLines(allLines);
            m_rtDirectives->AcceptDirectives(constParser);

            LogInfo(LogShaderBundle, "ShaderBundle:: Parsed RT directives from %s",
                    programDir.string().c_str());
        }

        LogInfo(LogShaderBundle, "ShaderBundle:: Created: %s (%zu UserDefinedBundles)",
                m_meta.name.c_str(), m_userDefinedBundles.size());
    }

    //-------------------------------------------------------------------------------------------
    // GetProgram (single program, default bundle)
    //-------------------------------------------------------------------------------------------
    std::shared_ptr<ShaderProgram> ShaderBundle::GetProgram(const std::string& programName, bool enableFallback)
    {
        // Level 1: Current UserDefinedBundle (bundle/{current}/)
        if (m_currentUserBundle)
        {
            auto program = m_currentUserBundle->GetProgram(programName);
            if (program)
            {
                return program;
            }
        }

        // Level 2: Program folder with fallback rules (program/)
        if (enableFallback && m_fallbackChain->IsEnabled())
        {
            // Use fallback chain to get list of programs to try
            auto chain = m_fallbackChain->GetFallbackChain(programName);
            for (const auto& name : chain)
            {
                auto program = LoadFromProgramFolder(name);
                if (program)
                {
                    return program;
                }
            }
        }
        else
        {
            // No fallback chain, just try the exact program name
            auto program = LoadFromProgramFolder(programName);
            if (program)
            {
                return program;
            }
        }

        // Level 3: Engine bundle (only if this is user bundle)
        if (!m_meta.isEngineBundle && m_engineBundle)
        {
            // Query engine bundle without fallback to avoid recursion
            return m_engineBundle->GetProgram(programName, false);
        }

        // Not found at any level
        return nullptr;
    }

    //-------------------------------------------------------------------------------------------
    // GetProgram (single program, specific bundle)
    //-------------------------------------------------------------------------------------------
    std::shared_ptr<ShaderProgram> ShaderBundle::GetProgram(const std::string& bundleName,
                                                            const std::string& programName,
                                                            bool               enableFallback)
    {
        // Level 1: Specified UserDefinedBundle
        auto* targetBundle = FindUserBundle(bundleName);
        if (targetBundle)
        {
            auto program = targetBundle->GetProgram(programName);
            if (program)
            {
                return program;
            }
        }

        // If bundle not found or program not in bundle, continue with Level 2 and 3
        // (same as single-argument GetProgram from this point)

        // Level 2: Program folder with fallback rules (program/)
        if (enableFallback && m_fallbackChain->IsEnabled())
        {
            auto chain = m_fallbackChain->GetFallbackChain(programName);
            for (const auto& name : chain)
            {
                auto program = LoadFromProgramFolder(name);
                if (program)
                {
                    return program;
                }
            }
        }
        else
        {
            auto program = LoadFromProgramFolder(programName);
            if (program)
            {
                return program;
            }
        }

        // Level 3: Engine bundle (only if this is user bundle)
        if (!m_meta.isEngineBundle && m_engineBundle)
        {
            return m_engineBundle->GetProgram(programName, false);
        }

        return nullptr;
    }

    //-------------------------------------------------------------------------------------------
    // GetPrograms (batch query)
    //-------------------------------------------------------------------------------------------
    std::vector<std::shared_ptr<ShaderProgram>> ShaderBundle::GetPrograms(const std::string& searchRule,
                                                                          bool               enableFallback)
    {
        std::vector<std::shared_ptr<ShaderProgram>> results;

        // Collect from current UserDefinedBundle
        if (m_currentUserBundle)
        {
            auto bundlePrograms = m_currentUserBundle->GetPrograms(searchRule);
            results.insert(results.end(), bundlePrograms.begin(), bundlePrograms.end());
        }

        // If fallback enabled and can not search in current user defined bundle, then we also search program/ folder cache
        if (enableFallback && results.size() == 0)
        {
            // Build regex pattern
            try
            {
                std::regex pattern(searchRule);

                // Search through cached programs
                for (const auto& [name, programPtr] : m_programCache)
                {
                    if (std::regex_match(name, pattern))
                    {
                        // Check if not already in results (avoid duplicates)
                        bool found = false;
                        for (auto existing : results)
                        {
                            if (existing == programPtr)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            results.push_back(programPtr);
                        }
                    }
                }

                // Also scan program/ folder for programs not yet cached
                auto programDir = m_meta.path / "shaders" / "program";
                if (FileSystemHelper::DirectoryExists(programDir))
                {
                    auto programNames = ShaderScanHelper::ScanShaderPrograms(programDir);
                    auto matches      = ShaderScanHelper::MatchProgramsByPattern(programNames, searchRule);

                    for (const auto& name : matches)
                    {
                        auto program = LoadFromProgramFolder(name);
                        if (program)
                        {
                            // Check if not already in results
                            bool found = false;
                            for (auto existing : results)
                            {
                                if (existing == program)
                                {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                            {
                                results.push_back(program);
                            }
                        }
                    }
                }
            }
            catch (const std::regex_error& e)
            {
                LogWarn(LogShaderBundle, "ShaderBundle:: Invalid regex pattern '%s': %s",
                        searchRule.c_str(), e.what());
            }
        }

        return results;
    }

    //-------------------------------------------------------------------------------------------
    // SwitchBundle
    //-------------------------------------------------------------------------------------------
    bool ShaderBundle::SwitchBundle(const std::string& targetBundleName)
    {
        // Engine bundle does not support SwitchBundle
        if (m_meta.isEngineBundle)
        {
            LogWarn(LogShaderBundle, "ShaderBundle:: Engine bundle does not support SwitchBundle operation");
            return false;
        }

        // Find target bundle
        auto* targetBundle = FindUserBundle(targetBundleName);
        if (!targetBundle)
        {
            LogWarn(LogShaderBundle, "ShaderBundle:: Bundle not found: %s", targetBundleName.c_str());
            return false;
        }

        // Switch to target bundle
        m_currentUserBundle = targetBundle;
        LogInfo(LogShaderBundle, "ShaderBundle:: Switched to bundle: %s", targetBundleName.c_str());
        return true;
    }

    //-------------------------------------------------------------------------------------------
    // Fallback Configuration Methods
    //-------------------------------------------------------------------------------------------
    bool ShaderBundle::HasFallbackConfiguration() const
    {
        return m_fallbackChain && m_fallbackChain->HasRules();
    }

    bool ShaderBundle::EnableFallbackRules(bool newState)
    {
        if (m_fallbackChain)
        {
            m_fallbackChain->SetEnabled(newState);
            LogInfo(LogShaderBundle, "ShaderBundle:: Fallback rules %s",
                    newState ? "enabled" : "disabled");
            return newState;
        }
        return false;
    }

    bool ShaderBundle::GetEnableFallbackRules() const
    {
        return m_fallbackChain && m_fallbackChain->IsEnabled();
    }

    //-------------------------------------------------------------------------------------------
    // Metadata Access Methods
    //-------------------------------------------------------------------------------------------
    std::string ShaderBundle::GetCurrentUserBundleName() const
    {
        if (m_currentUserBundle)
        {
            return m_currentUserBundle->GetName();
        }
        return "";
    }

    std::vector<std::string> ShaderBundle::GetUserBundleNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_userDefinedBundles.size());

        for (const auto& bundle : m_userDefinedBundles)
        {
            names.push_back(bundle->GetName());
        }
        return names;
    }

    //-------------------------------------------------------------------------------------------
    // Private Helper Methods
    //-------------------------------------------------------------------------------------------
    std::shared_ptr<ShaderProgram> ShaderBundle::LoadFromProgramFolder(const std::string& programName)
    {
        // Check cache first
        auto it = m_programCache.find(programName);
        if (it != m_programCache.end())
        {
            return it->second;
        }

        // Not in cache, try to compile from program/ folder
        auto programDir = m_meta.path / "shaders" / "program";
        auto files      = ShaderScanHelper::FindShaderFiles(programDir, programName);

        if (!files)
        {
            // Shader files not found, return nullptr (no exception)
            return nullptr;
        }

        ShaderCompileOptions shaderCompileOptions;
        shaderCompileOptions.enableDebugInfo = true;
        // Compile the shader program using RendererSubsystem
        auto program = g_theRendererSubsystem->CreateShaderProgramFromFiles(
            files->first, // VS path
            files->second, // PS path
            programName, // Program name
            shaderCompileOptions
        );

        if (!program)
        {
            // Compilation failed, log warning and return nullptr
            LogWarn(LogShaderBundle, "ShaderBundle:: Failed to compile program from program/ folder: %s",
                    programName.c_str());
            return nullptr;
        }

        // Cache the compiled program
        m_programCache[programName] = program;
        LogInfo(LogShaderBundle, "ShaderBundle:: Loaded program from program/ folder: %s",
                programName.c_str());

        return program;
    }

    UserDefinedBundle* ShaderBundle::FindUserBundle(const std::string& bundleName)
    {
        for (const auto& bundle : m_userDefinedBundles)
        {
            if (bundle->GetName() == bundleName)
            {
                return bundle.get();
            }
        }
        return nullptr;
    }
} // namespace enigma::graphic
