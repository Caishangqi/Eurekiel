#include "FileSystemHelper.hpp"

#include <fstream>
#include <iostream>

#include "ErrorWarningAssert.hpp"
#include "StringUtils.hpp"
#include "Engine/Core/Buffer/BufferExceptions.hpp"

int FileReadToBuffer(std::vector<uint8_t>& outBuffer, const std::string& filename)
{
    int   closeResult = 0;
    FILE* file        = nullptr;
    int   result      = fopen_s(&file, filename.c_str(), "rb");
    if (result != 0 || file == nullptr)
    {
        ERROR_RECOVERABLE(Stringf("Failed to open file %s", filename.c_str()));
        return -1;
    }

    // Move to the end of the file to determine its size
    int  seekResult = fseek(file, 0, SEEK_END);
    long fileSize   = ftell(file);
    if (fileSize < 0)
    {
        ERROR_RECOVERABLE(Stringf("Failed to determining file size %s",filename.c_str()))
        closeResult = fclose(file);
        return -1;
    }

    outBuffer.resize(fileSize);

    // Move back to the start of the file
    seekResult = fseek(file, 0, SEEK_SET);

    size_t bytesRead = fread(outBuffer.data(), 1, fileSize, file);
    if (bytesRead != static_cast<size_t>(fileSize))
    {
        ERROR_RECOVERABLE(Stringf("Failed to read file %s", filename.c_str()));
        closeResult = fclose(file);
        return -1;
    }

    closeResult = fclose(file);
    return static_cast<int>(bytesRead);
}

int FileReadToString(std::string& outString, const std::string& filename)
{
    std::vector<uint8_t> buffer;
    int                  readResult = FileReadToBuffer(buffer, filename);
    if (readResult < 0)
    {
        return readResult;
    }

    buffer.push_back('\0');

    // Construct std::string from the null-terminated C string.
    outString = std::string(reinterpret_cast<const char*>(buffer.data()));
    return static_cast<int>(outString.length());
}

// -----------------------------------------------------------------------------
// [NEW] FileSystemHelper static methods for ShaderBundle directory operations
// -----------------------------------------------------------------------------

std::vector<std::filesystem::path> FileSystemHelper::ListSubdirectories(
    const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;

    // Return empty vector if directory doesn't exist (no exception thrown)
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
    {
        return result;
    }

    // Iterate directory and collect only subdirectories
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_directory())
        {
            result.push_back(entry.path());
        }
    }

    return result;
}

bool FileSystemHelper::FileExists(const std::filesystem::path& path)
{
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool FileSystemHelper::DirectoryExists(const std::filesystem::path& path)
{
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::filesystem::path FileSystemHelper::CombinePath(
    const std::filesystem::path& base,
    const std::string&           relative)
{
    return (base / relative).lexically_normal();
}

// -----------------------------------------------------------------------------
// [NEW] ByteBuffer file I/O
// -----------------------------------------------------------------------------

void FileSystemHelper::WriteBufferToFile(const enigma::core::ByteBuffer& buf,
                                         const std::filesystem::path& filePath)
{
    std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to open file for writing");
    }

    ofs.write(reinterpret_cast<const char*>(buf.Data()), static_cast<std::streamsize>(buf.WrittenBytes()));
    if (!ofs.good())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to write buffer data to file");
    }
}

enigma::core::ByteArray FileSystemHelper::ReadFileToBuffer(const std::filesystem::path& filePath)
{
    std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to open file for reading");
    }

    std::streamsize fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    enigma::core::ByteArray data(static_cast<size_t>(fileSize));
    ifs.read(reinterpret_cast<char*>(data.data()), fileSize);
    if (!ifs.good())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to read file contents");
    }

    return data;
}

void FileSystemHelper::AppendBufferToFile(const enigma::core::ByteBuffer& buf,
                                          const std::filesystem::path& filePath)
{
    std::ofstream ofs(filePath, std::ios::binary | std::ios::app);
    if (!ofs.is_open())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to open file for appending");
    }

    ofs.write(reinterpret_cast<const char*>(buf.Data()), static_cast<std::streamsize>(buf.WrittenBytes()));
    if (!ofs.good())
    {
        throw enigma::core::FileIOException(filePath.string(), "Failed to append buffer data to file");
    }
}
