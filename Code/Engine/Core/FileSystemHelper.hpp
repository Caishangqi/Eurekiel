#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include "Engine/Core/Buffer/ByteBuffer.hpp"

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

    // [NEW] ByteBuffer file I/O - Binary read/write with FileIOException on failure
    static void WriteBufferToFile(const enigma::core::ByteBuffer& buf,
                                  const std::filesystem::path& filePath);
    static enigma::core::ByteArray ReadFileToBuffer(const std::filesystem::path& filePath);
    static void AppendBufferToFile(const enigma::core::ByteBuffer& buf,
                                   const std::filesystem::path& filePath);
};
