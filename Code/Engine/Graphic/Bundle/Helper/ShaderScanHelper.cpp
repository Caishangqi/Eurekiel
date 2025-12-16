//-----------------------------------------------------------------------------------------------
// ShaderScanHelper.cpp
//
// [NEW] Implementation of shader file discovery and matching utilities
//
//-----------------------------------------------------------------------------------------------

#include "ShaderScanHelper.hpp"
#include "Engine/Core/FileSystemHelper.hpp"
#include <regex>

namespace enigma::graphic
{
    //-----------------------------------------------------------------------------------------------
    // EndsWith - C++17 compatible string suffix check
    //-----------------------------------------------------------------------------------------------
    bool ShaderScanHelper::EndsWith(const std::string& str, const std::string& suffix)
    {
        if (suffix.size() > str.size())
        {
            return false;
        }
        return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    //-----------------------------------------------------------------------------------------------
    // ScanShaderPrograms
    //-----------------------------------------------------------------------------------------------
    std::vector<std::string> ShaderScanHelper::ScanShaderPrograms(
        const std::filesystem::path& directory)
    {
        std::vector<std::string> result;

        // Early return if directory doesn't exist
        if (!FileSystemHelper::DirectoryExists(directory))
        {
            return result;
        }

        // Scan for .vs.hlsl files
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            // Skip non-regular files (directories, symlinks, etc.)
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string filename = entry.path().filename().string();

            // Check if file ends with .vs.hlsl (C++17 compatible)
            if (EndsWith(filename, VS_EXTENSION))
            {
                // Extract program name: "gbuffers_basic.vs.hlsl" -> "gbuffers_basic"
                size_t      extLen      = std::string(VS_EXTENSION).length();
                std::string programName = filename.substr(0, filename.length() - extLen);

                // Check if corresponding .ps.hlsl exists
                std::filesystem::path psPath = directory / (programName + PS_EXTENSION);
                if (FileSystemHelper::FileExists(psPath))
                {
                    result.push_back(programName);
                }
            }
        }

        return result;
    }

    //-----------------------------------------------------------------------------------------------
    // FindShaderFiles
    //-----------------------------------------------------------------------------------------------
    std::optional<std::pair<std::filesystem::path, std::filesystem::path>> ShaderScanHelper::FindShaderFiles(
        const std::filesystem::path& directory,
        const std::string&           programName)
    {
        // Construct expected file paths
        std::filesystem::path vsPath = directory / (programName + VS_EXTENSION);
        std::filesystem::path psPath = directory / (programName + PS_EXTENSION);

        // Both files must exist for a valid shader program
        if (!FileSystemHelper::FileExists(vsPath) || !FileSystemHelper::FileExists(psPath))
        {
            return std::nullopt;
        }

        return std::make_pair(vsPath, psPath);
    }

    //-----------------------------------------------------------------------------------------------
    // MatchProgramsByPattern
    //-----------------------------------------------------------------------------------------------
    std::vector<std::string> ShaderScanHelper::MatchProgramsByPattern(
        const std::vector<std::string>& programNames,
        const std::string&              pattern)
    {
        std::vector<std::string> result;

        // Handle invalid/empty pattern gracefully
        if (pattern.empty())
        {
            return result;
        }

        try
        {
            std::regex regex(pattern);

            for (const auto& name : programNames)
            {
                if (std::regex_match(name, regex))
                {
                    result.push_back(name);
                }
            }
        }
        catch (const std::regex_error&)
        {
            // Invalid regex pattern - return empty result
            // No exception thrown to caller (follows Helper class contract)
            return result;
        }

        return result;
    }
} // namespace enigma::graphic
