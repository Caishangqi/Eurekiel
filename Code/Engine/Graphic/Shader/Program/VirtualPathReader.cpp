/**
 * @file VirtualPathReader.cpp
 * @brief Virtual Path File Reader Implementation
 * @date 2025-11-04
 * @author Caizii
 */

#include "VirtualPathReader.hpp"
#include <fstream>
#include <sstream>

#include "Include/ShaderPath.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // Constructor
    // ========================================================================

    VirtualPathReader::VirtualPathReader(const std::filesystem::path& rootPath)
        : m_rootPath(rootPath)
    {
        // [NOTE] Uses initializer list for direct member initialization
        // [NOTE] Does not validate root path validity (deferred to first use)
    }

    // ========================================================================
    // IFileReader Interface Implementation
    // ========================================================================

    std::optional<std::string> VirtualPathReader::ReadFile(const ShaderPath& path)
    {
        // Step 1: Convert to filesystem path
        auto fullPath = path.Resolved(m_rootPath);

        // Step 2: Check file existence
        if (!std::filesystem::exists(fullPath))
        {
            return std::nullopt;
        }

        // Step 3: Open and read file
        std::ifstream file(fullPath);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        // Read file content to stringstream
        std::stringstream buffer;
        buffer << file.rdbuf();

        // Step 4: Return file content
        return buffer.str();
    }

    bool VirtualPathReader::FileExists(const ShaderPath& path) const
    {
        // Convert to filesystem path and check existence
        auto fullPath = path.Resolved(m_rootPath);
        return std::filesystem::exists(fullPath);
    }

    std::filesystem::path VirtualPathReader::GetRootPath() const
    {
        return m_rootPath;
    }
} // namespace enigma::graphic
