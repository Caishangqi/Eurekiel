#pragma once
#include "LogMessage.hpp"
#include <deque>
#include <mutex>

namespace enigma::core
{
    // 消息缓冲区 - 使用循环队列存储消息
    // 线程安全设计，支持高效的消息添加和访问
    class MessageBuffer
    {
    public:
        // 构造函数
        explicit MessageBuffer(size_t maxSize = 10000);

        // 禁止拷贝和赋值
        MessageBuffer(const MessageBuffer&) = delete;
        MessageBuffer& operator=(const MessageBuffer&) = delete;

        // 添加消息（自动分配ID和时间戳）
        void AddMessage(const MessageLogEntry& message);

        // 获取消息（按索引）
        const MessageLogEntry& RetrieveMessage(size_t index) const;

        // 获取消息数量
        size_t GetMessageCount() const;

        // 清空所有消息
        void Clear();

        // 设置最大消息数量
        void SetMaxSize(size_t maxSize);

        // 获取最大消息数量
        size_t GetMaxSize() const;

        // 迭代器支持（用于遍历所有消息）
        std::deque<MessageLogEntry>::const_iterator begin() const;
        std::deque<MessageLogEntry>::const_iterator end() const;

    private:
        std::deque<MessageLogEntry> m_messages;  // 使用deque实现循环缓冲区
        size_t m_maxSize;                   // 最大消息数量
        uint64_t m_nextId;                  // 下一个消息ID
        mutable std::mutex m_mutex;         // 线程安全互斥锁
    };
}
