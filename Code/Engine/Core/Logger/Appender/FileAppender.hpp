#pragma once
#include "ILogAppender.hpp"
#include <fstream>
#include <mutex>

namespace enigma::core
{
    class FileAppender : public ILogAppender {
    public:
        FileAppender(const std::string& filePath, bool appendMode = true);
        ~FileAppender();
        
        void Write(const LogMessage& message) override;
        void Flush() override;
        
        bool IsOpen() const { return m_file.is_open(); }
        const std::string& GetFilePath() const { return m_filePath; }

    private:
        std::string FormatLogMessage(const LogMessage& message) const;
        
        std::ofstream m_file;
        std::string m_filePath;
        mutable std::mutex m_fileMutex;
    };
}