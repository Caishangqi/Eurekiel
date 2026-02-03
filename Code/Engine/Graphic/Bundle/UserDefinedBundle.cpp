//-----------------------------------------------------------------------------------------------
// UserDefinedBundle.cpp
//
// [NEW] Implementation of UserDefinedBundle class
//
// This file implements:
//   - Constructor: Initialize bundle name and path
//   - PrecompileAll: Compile all shader programs in bundle directory
//   - GetProgram: Single program query
//   - GetPrograms: Regex-based batch query
//   - HasProgram: Existence check
//
// Key Implementation Details:
//   - Uses ShaderScanHelper for shader file discovery
//   - Uses RendererSubsystem::CreateShaderProgramFromFiles for compilation
//   - Resilient loading: Individual failures don't break entire bundle
//   - Logs all operations to LogShaderBundle category
//
//-----------------------------------------------------------------------------------------------

#include "UserDefinedBundle.hpp"
#include "Engine/Graphic/Bundle/Helper/ShaderScanHelper.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"  // For g_theRendererSubsystem
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------------------------
    UserDefinedBundle::UserDefinedBundle(const std::string&           bundleName,
                                         const std::filesystem::path& bundlePath)
        : m_bundleName(bundleName)
          , m_bundlePath(bundlePath)
          , m_programs()
    {
        LogInfo(LogShaderBundle, "UserDefinedBundle:: Created bundle '%s' at path: %s",
                m_bundleName.c_str(), m_bundlePath.string().c_str());
    }

    //-------------------------------------------------------------------------------------------
    // PrecompileAll
    //-------------------------------------------------------------------------------------------
    void UserDefinedBundle::PrecompileAll()
    {
        // Precondition check
        if (!g_theRendererSubsystem)
        {
            LogError(LogShaderBundle, "UserDefinedBundle:: Cannot precompile: g_theRendererSubsystem is null");
            return;
        }

        // 1. Scan directory for shader programs
        std::vector<std::string> programNames = ShaderScanHelper::ScanShaderPrograms(m_bundlePath);

        if (programNames.empty())
        {
            LogWarn(LogShaderBundle, "UserDefinedBundle:: No shader programs found in bundle '%s'",
                    m_bundleName.c_str());
            return;
        }

        LogInfo(LogShaderBundle, "UserDefinedBundle:: PreCompiling %zu programs in bundle '%s'", programNames.size(), m_bundleName.c_str());

        // 2. Compile each program
        int successCount = 0;
        int failCount    = 0;

        for (const auto& programName : programNames)
        {
            // 2a. Find shader files
            auto shaderFilesOpt = ShaderScanHelper::FindShaderFiles(m_bundlePath, programName);
            if (!shaderFilesOpt)
            {
                LogWarn(LogShaderBundle, "UserDefinedBundle:: Shader files not found for program: %s",
                        programName.c_str());
                ++failCount;
                continue;
            }

            const auto& vsPath = shaderFilesOpt->first;
            const auto& psPath = shaderFilesOpt->second;

            // 2b. Compile shader program
            try
            {
                ShaderCompileOptions shaderCompileOptions;
                shaderCompileOptions.enableDebugInfo   = true;
                std::shared_ptr<ShaderProgram> program =
                    g_theRendererSubsystem->CreateShaderProgramFromFiles(
                        vsPath, psPath, programName, shaderCompileOptions);

                if (program)
                {
                    // 2c. Store in cache
                    m_programs[programName] = program;
                    LogInfo(LogShaderBundle, "UserDefinedBundle:: Compiled program: %s",
                            programName.c_str());
                    ++successCount;
                }
                else
                {
                    LogWarn(LogShaderBundle, "UserDefinedBundle:: Failed to compile program (returned nullptr): %s",
                            programName.c_str());
                    ++failCount;
                }
            }
            catch (const std::exception& e)
            {
                // 2d. Resilient design: Log warning and continue with other programs
                LogWarn(LogShaderBundle, "UserDefinedBundle:: Exception compiling program '%s': %s",
                        programName.c_str(), e.what());
                ++failCount;
            }
        }

        // 3. Summary log
        LogInfo(LogShaderBundle, "UserDefinedBundle:: Precompilation complete for bundle '%s': %d succeeded, %d failed",
                m_bundleName.c_str(), successCount, failCount);
    }

    //-------------------------------------------------------------------------------------------
    // GetProgram
    //-------------------------------------------------------------------------------------------
    std::shared_ptr<ShaderProgram> UserDefinedBundle::GetProgram(const std::string& programName)
    {
        auto it = m_programs.find(programName);
        if (it != m_programs.end())
        {
            return it->second;
        }
        return nullptr; // Not found, return nullptr (no exception)
    }

    //-------------------------------------------------------------------------------------------
    // GetPrograms
    //-------------------------------------------------------------------------------------------
    std::vector<std::shared_ptr<ShaderProgram>> UserDefinedBundle::GetPrograms(const std::string& searchRule)
    {
        // 1. Collect all program names from cache
        std::vector<std::string> allNames;
        allNames.reserve(m_programs.size());
        for (const auto& pair : m_programs)
        {
            allNames.push_back(pair.first);
        }

        // 2. Match names against pattern using ShaderScanHelper
        std::vector<std::string> matchedNames =
            ShaderScanHelper::MatchProgramsByPattern(allNames, searchRule);

        // 3. Build result vector of raw pointers
        std::vector<std::shared_ptr<ShaderProgram>> result;
        result.reserve(matchedNames.size());
        for (const auto& name : matchedNames)
        {
            auto it = m_programs.find(name);
            if (it != m_programs.end())
            {
                result.push_back(it->second);
            }
        }

        return result; // Empty vector if no matches (no exception)
    }

    //-------------------------------------------------------------------------------------------
    // HasProgram
    //-------------------------------------------------------------------------------------------
    bool UserDefinedBundle::HasProgram(const std::string& programName) const
    {
        return m_programs.find(programName) != m_programs.end();
    }
} // namespace enigma::graphic
