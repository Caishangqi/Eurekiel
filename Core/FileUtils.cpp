#include "FileUtils.hpp"

#include <iostream>

#include "ErrorWarningAssert.hpp"
#include "StringUtils.hpp"

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
