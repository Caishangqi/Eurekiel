#pragma once
#include "../Logger/Appender/ILogAppender.hpp"
#include <mutex>

namespace enigma::core
{
    // 前向声明
    class MessageLogSubsystem;

    // MessageLogAppender - Logger到MessageLog的桥接
    //
    // 功能：
    // - 将LoggerSubsystem的输出自动转发到MessageLogSubsystem
    // - 处理Logger::LogMessage到MessageLog::LogMessage的转换
    // - 保持Logger和MessageLog解耦
    // - 线程安全设计
    //
    // 使用方式：
    //   auto* logger = GEngine->GetLogger();
    //   auto* msgLog = GEngine->GetMessageLog();
    //   if (logger && msgLog) {
    //       auto appender = std::make_unique<MessageLogAppender>(msgLog);
    //       logger->AddAppender(std::move(appender));
    //   }
    class MessageLogAppender : public ILogAppender
    {
    public:
        // 构造函数
        // 参数：
        //   messageLog - MessageLogSubsystem指针（不负责生命周期管理）
        explicit MessageLogAppender(MessageLogSubsystem* messageLog);

        // 析构函数
        ~MessageLogAppender() override;

        // ILogAppender接口实现
        // 将Logger的日志消息写入MessageLog
        void Write(const enigma::core::LogMessage& message) override;

        // 刷新缓冲区（MessageLog不需要特殊的Flush操作）
        void Flush() override;

    private:
        // MessageLogSubsystem指针
        MessageLogSubsystem* m_messageLog;

        // 线程安全锁（保护Write操作）
        mutable std::mutex m_mutex;

        // 禁用拷贝和赋值
        MessageLogAppender(const MessageLogAppender&) = delete;
        MessageLogAppender& operator=(const MessageLogAppender&) = delete;
    };

} // namespace enigma::core
