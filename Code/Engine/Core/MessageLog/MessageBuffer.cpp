#include "MessageBuffer.hpp"
#include <algorithm>

namespace enigma::core
{
    MessageBuffer::MessageBuffer(size_t maxSize)
        : m_maxSize(maxSize)
        , m_nextId(1)  // 从1开始，0保留用于无效ID
    {
        // std::deque 不支持 reserve()，它会自动管理内存
        // deque 的设计就是为了高效的双端插入和删除，不需要预分配
    }

    void MessageBuffer::AddMessage(const MessageLogEntry& message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 创建消息副本并分配ID
        MessageLogEntry newMessage = message;
        newMessage.id = m_nextId++;

        // 如果未设置时间戳，使用当前时间
        if (newMessage.timestamp == TimeStamp())
        {
            newMessage.timestamp = std::chrono::system_clock::now();
        }

        // 添加到队列
        m_messages.push_back(newMessage);

        // 超过最大容量时，删除最旧的消息
        if (m_messages.size() > m_maxSize)
        {
            m_messages.pop_front();
        }
    }

    const MessageLogEntry& MessageBuffer::RetrieveMessage(size_t index) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.at(index);
    }

    size_t MessageBuffer::GetMessageCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.size();
    }

    void MessageBuffer::Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.clear();
    }

    void MessageBuffer::SetMaxSize(size_t maxSize)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxSize = maxSize;

        // 如果当前消息数超过新的最大值，删除最旧的消息
        while (m_messages.size() > m_maxSize)
        {
            m_messages.pop_front();
        }
    }

    size_t MessageBuffer::GetMaxSize() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxSize;
    }

    std::deque<MessageLogEntry>::const_iterator MessageBuffer::begin() const
    {
        // 注意：此迭代器在使用期间不能保证线程安全
        // 调用者需要确保在迭代期间不会修改缓冲区
        return m_messages.begin();
    }

    std::deque<MessageLogEntry>::const_iterator MessageBuffer::end() const
    {
        return m_messages.end();
    }
}
