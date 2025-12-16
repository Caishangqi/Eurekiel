#pragma once
#include <string>
#include <vector>
#include <filesystem>

int FileReadToBuffer(std::vector<uint8_t>& outBuffer, const std::string& filename);
int FileReadToString(std::string& outString, const std::string& filename);

// -----------------------------------------------------------------------------
// [NEW] FileSystemHelper - Pure static, stateless utility class for ShaderBundle
// -----------------------------------------------------------------------------
class FileSystemHelper
{
public:
    FileSystemHelper() = delete;

    // [NEW] Directory scanning - Returns only subdirectories (not files)
    // Returns empty vector if directory doesn't exist (no exception)
    static std::vector<std::filesystem::path> ListSubdirectories(
        const std::filesystem::path& directory);

    // [NEW] Existence checks - Wrappers for std::filesystem operations
    static bool FileExists(const std::filesystem::path& path);
    static bool DirectoryExists(const std::filesystem::path& path);

    // [NEW] Path composition - Combines and normalizes paths
    static std::filesystem::path CombinePath(
        const std::filesystem::path& base,
        const std::string&           relative);
};
