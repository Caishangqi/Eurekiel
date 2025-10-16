#include "MessageLogAppender.hpp"
#include "MessageLogSubsystem.hpp"
#include "LogMessage.hpp"  // MessageLog的LogMessage
#include "../Logger/LogLevel.hpp"
#include <mutex>

// 注意：这里只包含MessageLog的LogMessage.hpp
// Logger的LogMessage通过ILogAppender的参数传入，无需包含其头文件

namespace enigma::core
{
    //---------------------------------------------------------------------
    // 构造函数
    //---------------------------------------------------------------------

    MessageLogAppender::MessageLogAppender(MessageLogSubsystem* messageLog)
        : m_messageLog(messageLog)
    {
        // 不需要额外的初始化
        // ILogAppender基类会默认设置m_enabled = true
    }

    //---------------------------------------------------------------------
    // 析构函数
    //---------------------------------------------------------------------

    MessageLogAppender::~MessageLogAppender()
    {
        // 在销毁前刷新所有待处理的消息
        Flush();
    }

    //---------------------------------------------------------------------
    // Write实现 - 将Logger的LogMessage转换为MessageLog的LogMessage
    //---------------------------------------------------------------------

    void MessageLogAppender::Write(const enigma::core::LogMessage& loggerMessage)
    {
        // 检查是否启用
        if (!IsEnabled())
        {
            return;
        }

        // 检查MessageLogSubsystem是否有效
        if (!m_messageLog)
        {
            // 静默失败，避免崩溃
            return;
        }

        // 线程安全
        std::lock_guard<std::mutex> lock(m_mutex);

        // 转换Logger::LogMessage到MessageLog::MessageLogEntry
        //
        // Logger::LogMessage包含：
        //   - LogLevel level
        //   - std::string category
        //   - std::string message
        //   - std::chrono::system_clock::time_point timestamp
        //   - std::thread::id threadId
        //   - int frameNumber
        //
        // MessageLog::MessageLogEntry包含：
        //   - uint64_t id (由MessageLogSubsystem自动生成)
        //   - TimeStamp timestamp (std::chrono::system_clock::time_point)
        //   - uint32_t frameNumber
        //   - LogLevel level
        //   - std::string category
        //   - Rgba8 color (基于LogLevel或Category)
        //   - std::string message

        // 获取Category的颜色
        // 如果Category已在MessageLog中注册，则使用其默认颜色
        // 否则使用LogLevel对应的颜色
        Rgba8 color = GetColorForLogLevel(loggerMessage.level);

        // 如果MessageLog已经注册了该Category，可以获取其颜色
        // 但这需要调用MessageLogSubsystem::GetCategoryMetadata
        // 为了简化实现，这里直接使用LogLevel的颜色
        // Phase 2可以优化为优先使用Category颜色

        // 调用MessageLogSubsystem的AddMessage
        // 使用简化版本的AddMessage，它接受：
        //   - LogLevel level
        //   - const std::string& category
        //   - const std::string& message
        // MessageLogSubsystem会自动设置timestamp, frameNumber, id等字段
        m_messageLog->AddMessage(
            loggerMessage.level,
            loggerMessage.category,
            loggerMessage.message
        );

        // 注意：这里没有传递Logger的frameNumber和timestamp
        // MessageLogSubsystem会使用自己的时间戳和帧号
        // 如果需要保留Logger的时间戳和帧号，需要使用完整版本的AddMessage
        // 并手动构造MessageLog::LogMessage
        //
        // 完整版本示例（未启用）：
        /*
        enigma::core::MessageLogEntry msgLogEntry;
        msgLogEntry.level = loggerMessage.level;
        msgLogEntry.category = loggerMessage.category;
        msgLogEntry.message = loggerMessage.message;
        msgLogEntry.timestamp = loggerMessage.timestamp;
        msgLogEntry.frameNumber = static_cast<uint32_t>(loggerMessage.frameNumber);
        msgLogEntry.color = color;
        // msgLogEntry.id 由MessageLogSubsystem设置
        m_messageLog->AddMessage(msgLogEntry);
        */
    }

    //---------------------------------------------------------------------
    // Flush实现 - MessageLog不需要特殊的Flush操作
    //---------------------------------------------------------------------

    void MessageLogAppender::Flush()
    {
        // MessageLog的消息是立即写入的，不需要缓冲区刷新
        // 这里提供空实现
        // 如果未来MessageLog引入了异步写入或缓冲机制，可以在这里调用相应的Flush方法
    }

} // namespace enigma::core
