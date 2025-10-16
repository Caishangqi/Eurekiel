#pragma once
#include <set>
#include <string>
#include "../Logger/LogLevel.hpp"

namespace enigma::core
{
    struct MessageLogEntry;

    // 基础过滤器接口
    // Phase 3将扩展此接口以支持高级过滤（正则表达式、时间范围等）
    class ILogFilter
    {
    public:
        virtual ~ILogFilter() = default;

        // 判断消息是否通过过滤器
        virtual bool PassesFilter(const MessageLogEntry& message) const = 0;
    };

    // 基础过滤器条件
    // 用于简单的LogLevel和Category过滤
    struct BasicFilterCriteria
    {
        std::set<LogLevel> visibleLevels;       // 可见的日志级别
        std::set<std::string> visibleCategories; // 可见的Category

        BasicFilterCriteria()
        {
            // 默认所有级别可见
            visibleLevels = {
                LogLevel::TRACE,
                LogLevel::DEBUG,
                LogLevel::INFO,
                LogLevel::WARNING,
                LogLevel::ERROR_,
                LogLevel::FATAL
            };
        }
    };

    // 基础过滤器实现
    class BasicLogFilter : public ILogFilter
    {
    public:
        BasicLogFilter() = default;
        explicit BasicLogFilter(const BasicFilterCriteria& criteria)
            : m_criteria(criteria)
        {
        }

        void SetCriteria(const BasicFilterCriteria& criteria)
        {
            m_criteria = criteria;
        }

        const BasicFilterCriteria& GetCriteria() const
        {
            return m_criteria;
        }

        bool PassesFilter(const MessageLogEntry& message) const override;

    private:
        BasicFilterCriteria m_criteria;
    };
}
