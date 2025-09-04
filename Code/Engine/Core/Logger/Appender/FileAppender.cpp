#include "FileAppender.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace enigma::core
{
    FileAppender::FileAppender(const std::string& filePath, bool appendMode)
        : m_filePath(filePath)
    {
        std::ios_base::openmode mode = std::ios_base::out;
        if (appendMode)
        {
            mode |= std::ios_base::app;
        }
        else
        {
            mode |= std::ios_base::trunc;
        }

        m_file.open(filePath, mode);

        if (!m_file.is_open())
        {
            std::cerr << "Failed to open log file: " << filePath << std::endl;
        }
        else
        {
            // Write a startup marker
            if (!appendMode || m_file.tellp() == 0)
            {
                m_file << "=== Log Session Started ===" << std::endl;
            }
        }
    }

    FileAppender::~FileAppender()
    {
        if (m_file.is_open())
        {
            m_file << "=== Log Session Ended ===" << std::endl;
            m_file.close();
        }
    }

    void FileAppender::Write(const LogMessage& message)
    {
        if (!IsEnabled() || !m_file.is_open())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_fileMutex);

        std::string formattedMessage = FormatLogMessage(message);
        m_file << formattedMessage << std::endl;

        // Auto-flush for important messages to ensure they're written to disk
        if (message.level >= LogLevel::ERROR_)
        {
            m_file.flush();
        }
    }

    void FileAppender::Flush()
    {
        std::lock_guard<std::mutex> lock(m_fileMutex);
        if (m_file.is_open())
        {
            m_file.flush();
        }
    }

    std::string FileAppender::FormatLogMessage(const LogMessage& message) const
    {
        std::stringstream ss;

        // Format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [Thread:ID] [Category] Message (Frame: 123)
        auto time_t = std::chrono::system_clock::to_time_t(message.timestamp);
        auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()) % 1000;

#ifdef _WIN32
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_t);
        ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
#else
        ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
#endif
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

        ss << "[" << std::setw(5) << std::left << LogLevelToString(message.level) << "] ";
        ss << "[Thread:" << message.GetThreadIdString() << "] ";
        ss << "[" << message.category << "] ";
        ss << message.message;

        if (message.frameNumber > 0)
        {
            ss << " (Frame: " << message.frameNumber << ")";
        }

        return ss.str();
    }
}
