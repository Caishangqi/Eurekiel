#pragma once
//-----------------------------------------------------------------------------------------------
// BufferExceptions.hpp
//
// Engine-layer exception hierarchy for ByteBuffer and file I/O operations.
//
// | Exception Type              | Base Class            | Error Macro       | Description                    |
// |-----------------------------|-----------------------|-------------------|--------------------------------|
// | BufferUnderflowException    | std::out_of_range     | ERROR_RECOVERABLE | ByteBuffer read beyond end     |
// | FileIOException             | std::runtime_error    | ERROR_RECOVERABLE | File read/write failure        |
//
// Usage Pattern (Exception + Error Macro two-phase):
//   try {
//       auto val = buffer.ReadInt();
//   } catch (const BufferUnderflowException& e) {
//       ERROR_RECOVERABLE(e.what());
//   }
//
//-----------------------------------------------------------------------------------------------

#include <cstdint>
#include <stdexcept>
#include <string>

namespace enigma::core
{
    //-------------------------------------------------------------------------------------------
    // BufferUnderflowException
    // Thrown when a ByteBuffer read operation exceeds available data.
    //
    // Carries diagnostic fields: cursor position, buffer size, requested byte count.
    //
    // Mapping: ERROR_RECOVERABLE (abort current operation, caller decides recovery)
    //-------------------------------------------------------------------------------------------
    class BufferUnderflowException : public std::out_of_range
    {
    public:
        BufferUnderflowException(size_t cursor, size_t bufferSize, size_t requestedBytes)
            : std::out_of_range(BuildMessage(cursor, bufferSize, requestedBytes))
            , m_cursor(cursor)
            , m_bufferSize(bufferSize)
            , m_requestedBytes(requestedBytes)
        {
        }

        size_t GetCursor()         const { return m_cursor; }
        size_t GetBufferSize()     const { return m_bufferSize; }
        size_t GetRequestedBytes() const { return m_requestedBytes; }

    private:
        size_t m_cursor;
        size_t m_bufferSize;
        size_t m_requestedBytes;

        static std::string BuildMessage(size_t cursor, size_t bufferSize, size_t requestedBytes)
        {
            return "BufferUnderflow: attempted to read " + std::to_string(requestedBytes)
                 + " byte(s) at cursor " + std::to_string(cursor)
                 + ", but buffer size is " + std::to_string(bufferSize);
        }
    };

    //-------------------------------------------------------------------------------------------
    // FileIOException
    // Thrown when a file read or write operation fails.
    //
    // Carries the file path that caused the failure.
    //
    // Mapping: ERROR_RECOVERABLE (file not found, permission denied, etc.)
    //-------------------------------------------------------------------------------------------
    class FileIOException : public std::runtime_error
    {
    public:
        FileIOException(const std::string& filePath, const std::string& reason)
            : std::runtime_error("FileIO error [" + filePath + "]: " + reason)
            , m_filePath(filePath)
        {
        }

        const std::string& GetFilePath() const { return m_filePath; }

    private:
        std::string m_filePath;
    };
} // namespace enigma::core
