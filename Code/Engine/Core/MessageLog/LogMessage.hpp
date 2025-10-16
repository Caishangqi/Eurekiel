#pragma once
#include <string>
#include <chrono>
#include "../Logger/LogLevel.hpp"
#include "../Rgba8.hpp"

namespace enigma::core
{
    // 时间戳类型定义
    using TimeStamp = std::chrono::system_clock::time_point;

    // MessageLog消息数据结构
    // 注意：这与Logger的LogMessage是不同的结构体
    // 这个结构体用于MessageLog系统的消息存储和显示
    struct MessageLogEntry
    {
        uint64_t id = 0;                    // 唯一ID
        TimeStamp timestamp;                // 时间戳
        uint32_t frameNumber = 0;           // 帧号
        LogLevel level = LogLevel::INFO;    // 日志级别
        std::string category;               // Category名称
        Rgba8 color = Rgba8::WHITE;         // 显示颜色
        std::string message;                // 消息内容

        // 默认构造函数
        MessageLogEntry() = default;

        // 完整构造函数
        MessageLogEntry(uint64_t id,
                   const TimeStamp& timestamp,
                   uint32_t frameNumber,
                   LogLevel level,
                   const std::string& category,
                   const Rgba8& color,
                   const std::string& message)
            : id(id)
            , timestamp(timestamp)
            , frameNumber(frameNumber)
            , level(level)
            , category(category)
            , color(color)
            , message(message)
        {
        }

        // 简化构造函数（用于常见场景）
        MessageLogEntry(LogLevel level,
                   const std::string& category,
                   const std::string& message,
                   const Rgba8& color = Rgba8::WHITE)
            : level(level)
            , category(category)
            , color(color)
            , message(message)
        {
            timestamp = std::chrono::system_clock::now();
        }
    };
}
