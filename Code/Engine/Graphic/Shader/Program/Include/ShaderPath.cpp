#include "ShaderPath.hpp"
#include <sstream>
#include <vector>
#include <stdexcept>

namespace enigma::graphic
{
    // ========================================================================
    // [INTERNAL] Private Constructor - Only created via factory methods
    // ========================================================================

    ShaderPath::ShaderPath(const std::string& normalizedPath)
        : m_path(normalizedPath)
    {
        // Teaching Point: Private constructor ensures all paths go through normalization
        // Immutable object pattern: m_path never changes after construction
    }

    // ========================================================================
    // [PUBLIC] Factory Method - Create and validate absolute path
    // ========================================================================

    ShaderPath ShaderPath::FromAbsolutePath(const std::string& absolutePath)
    {
        // Teaching Point: Factory Method Pattern - Centralized validation and normalization
        if (absolutePath.empty() || absolutePath[0] != '/')
        {
            throw std::invalid_argument("Path must start with '/': " + absolutePath);
        }

        // Normalize path (handle . and ..)
        std::string normalized = NormalizeAbsolutePath(absolutePath);

        // Create instance using private constructor
        return ShaderPath(normalized);
    }

    // ========================================================================
    // [INTERNAL] Path Normalization Algorithm - Core Logic
    // ========================================================================

    std::string ShaderPath::NormalizeAbsolutePath(const std::string& path)
    {
        /**
         * Teaching Point: Path Normalization Algorithm
         *
         * Steps:
         * 1. Split path by '/' into segments
         * 2. Filter empty segments and '.' segments
         * 3. Handle '..' segments (go to parent directory)
         * 4. Rebuild normalized path
         *
         * Examples:
         * - /shaders/./lib/../common.hlsl -> /shaders/common.hlsl
         * - /shaders//lib/common.hlsl -> /shaders/lib/common.hlsl
         * - / -> /
         *
         * [FIX] Allow '..' to escape virtual root for cross-bundle includes
         * - /shaders/program/../../../../assets/engine/... -> /../../../assets/engine/...
         */

        if (path.empty() || path[0] != '/')
        {
            throw std::invalid_argument("Not an absolute path: " + path);
        }

        // ========================================================================
        // Step 1: Split path into segments
        // ========================================================================

        std::vector<std::string> segments;
        std::string              currentSegment;

        for (char c : path)
        {
            if (c == '/')
            {
                if (!currentSegment.empty())
                {
                    segments.push_back(currentSegment);
                    currentSegment.clear();
                }
                // Ignore empty segments (consecutive '/')
            }
            else
            {
                currentSegment += c;
            }
        }

        // Handle last segment (if path doesn't end with '/')
        if (!currentSegment.empty())
        {
            segments.push_back(currentSegment);
        }

        // ========================================================================
        // Step 2: Normalize segments (handle . and ..)
        // ========================================================================

        std::vector<std::string> normalizedSegments;

        for (const auto& segment : segments)
        {
            if (segment == "." || segment.empty())
            {
                // Ignore current directory symbol and empty segments
                continue;
            }
            else if (segment == "..")
            {
                // [FIX] Allow '..' to escape virtual root for cross-bundle includes
                // Example: /shaders/program/../../../../assets/engine/...
                //       -> /../../../assets/engine/...
                if (!normalizedSegments.empty() && normalizedSegments.back() != "..")
                {
                    // Still have normal segments to pop
                    normalizedSegments.pop_back();
                }
                else
                {
                    // Already at root or have escape '..' - preserve for Resolved() to handle
                    normalizedSegments.push_back("..");
                }
            }
            else
            {
                // Normal segment, add to result
                normalizedSegments.push_back(segment);
            }
        }

        // ========================================================================
        // Step 3: Rebuild normalized path
        // ========================================================================

        if (normalizedSegments.empty())
        {
            // Special case: root path
            return "/";
        }

        std::ostringstream oss;
        for (const auto& segment : normalizedSegments)
        {
            oss << "/" << segment;
        }

        return oss.str();
    }

    // ========================================================================
    // [PUBLIC] Get Parent Path
    // ========================================================================

    std::optional<ShaderPath> ShaderPath::Parent() const
    {
        /**
         * Teaching Point: std::optional usage
         *
         * Business Logic:
         * - /shaders/lib/common.hlsl -> /shaders/lib
         * - /shaders -> /
         * - / -> std::nullopt (root has no parent)
         */

        if (m_path == "/")
        {
            // Root path has no parent
            return std::nullopt;
        }

        // Find position of last '/'
        size_t lastSlash = m_path.find_last_of('/');

        if (lastSlash == 0)
        {
            // Parent is root directory
            return ShaderPath("/");
        }

        // Extract part before last '/'
        std::string parentPath = m_path.substr(0, lastSlash);
        return ShaderPath(parentPath);
    }

    // ========================================================================
    // [PUBLIC] Resolve Relative Path
    // ========================================================================

    ShaderPath ShaderPath::Resolve(const std::string& relativePath) const
    {
        /**
         * Teaching Point: Relative Path Resolution Algorithm
         *
         * Business Logic:
         * 1. If relativePath starts with '/', return new absolute path
         * 2. Detect if current path is file path or directory path
         * 3. File path: resolve based on parent; Directory path: resolve based on current
         *
         * Example (file path):
         * - Current: /shaders/gbuffers_terrain.hlsl
         * - Resolve: ../lib/common.hlsl
         * - Steps: Get parent /shaders -> concat -> normalize to /shaders/lib/common.hlsl
         *
         * Example (directory path):
         * - Current: /shaders/programs
         * - Resolve: Common.hlsl
         * - Steps: Use /shaders/programs -> concat -> normalize to /shaders/programs/Common.hlsl
         */

        if (relativePath.empty())
        {
            throw std::invalid_argument("Relative path cannot be empty");
        }

        // If absolute path, create new path directly
        if (relativePath[0] == '/')
        {
            return FromAbsolutePath(relativePath);
        }

        // ========================================================================
        // Detect if current path is file path or directory path
        // ========================================================================
        // Strategy: Check if last segment contains '.' (possible file extension)
        //
        // Rules:
        // - /shaders/main.hlsl -> File path (last segment contains '.')
        // - /shaders/programs -> Directory path (no '.' in last segment)
        //
        // Note: This heuristic isn't 100% accurate (directories can contain '.'),
        // but follows common filesystem conventions and works in most cases.
        // ========================================================================

        bool   isFilePath = false;
        size_t lastSlash  = m_path.find_last_of('/');

        if (lastSlash != std::string::npos && lastSlash + 1 < m_path.size())
        {
            // Get last segment (filename or dirname)
            std::string lastSegment = m_path.substr(lastSlash + 1);

            // If last segment contains '.' and doesn't start with '.', likely a file
            size_t dotPos = lastSegment.find('.');
            if (dotPos != std::string::npos && dotPos > 0)
            {
                isFilePath = true;
            }
        }

        // ========================================================================
        // Select base path based on path type
        // ========================================================================

        std::string basePath;
        if (isFilePath)
        {
            // File path: Use parent as base
            // Example: /shaders/main.hlsl -> base is /shaders
            auto parentOpt = Parent();
            basePath       = parentOpt.has_value() ? parentOpt->GetPathString() : "/";
        }
        else
        {
            // Directory path: Use current path as base
            // Example: /shaders/programs -> base is /shaders/programs
            basePath = m_path;
        }

        // Concat paths
        std::string combinedPath = basePath + "/" + relativePath;

        // Normalize and return
        return FromAbsolutePath(combinedPath);
    }

    // ========================================================================
    // [PUBLIC] Convert to Filesystem Path
    // ========================================================================

    std::filesystem::path ShaderPath::Resolved(const std::filesystem::path& root) const
    {
        /**
         * Teaching Point: Virtual Path to Filesystem Path Conversion
         *
         * Business Logic:
         * - Virtual path: /shaders/gbuffers_terrain.hlsl
         * - Root: F:/MyProject/ShaderBundle/
         * - Result: F:/MyProject/ShaderBundle/shaders/gbuffers_terrain.hlsl
         *
         * [FIX] Support escape paths (cross-bundle include engine shaders):
         * - Escape path: /../../../assets/engine/shaders/core/Common.hlsl
         * - Root: .enigma/assets/engine/shaders
         * - Result: .enigma/assets/engine/shaders/core/Common.hlsl
         *
         * Implementation:
         * - Count leading '..' segments
         * - Traverse up from root by that count
         * - Append remaining path
         */

        if (m_path == "/")
        {
            // Root path returns root directly
            return root;
        }

        // [FIX] Handle escape paths (starting with /..)
        // Count leading '..' segments and find remaining path
        std::string pathStr = m_path.substr(1); // Skip leading '/'

        int    escapeCount = 0;
        size_t pos         = 0;

        // Count consecutive '..' segments at start
        while (pathStr.substr(pos, 2) == "..")
        {
            escapeCount++;
            pos += 2;

            // Skip separator '/'
            if (pos < pathStr.size() && pathStr[pos] == '/')
            {
                pos++;
            }
            else
            {
                break;
            }
        }

        if (escapeCount > 0)
        {
            // Has escape '..' segments, traverse up from root
            std::filesystem::path adjustedRoot = root;

            for (int i = 0; i < escapeCount; ++i)
            {
                if (adjustedRoot.has_parent_path() && adjustedRoot != adjustedRoot.root_path())
                {
                    adjustedRoot = adjustedRoot.parent_path();
                }
                // Stop if already at filesystem root
            }

            // Get remaining path (part after all '..' segments)
            std::string remainingPath = pathStr.substr(pos);

            if (remainingPath.empty())
            {
                return adjustedRoot;
            }

            return adjustedRoot / remainingPath;
        }

        // Normal path: direct concat
        return root / pathStr;
    }
} // namespace enigma::graphic
