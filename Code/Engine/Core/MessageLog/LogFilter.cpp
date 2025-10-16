#include "LogFilter.hpp"
#include "LogMessage.hpp"

namespace enigma::core
{
    bool BasicLogFilter::PassesFilter(const MessageLogEntry& message) const
    {
        // 检查LogLevel过滤
        if (!m_criteria.visibleLevels.empty())
        {
            if (m_criteria.visibleLevels.find(message.level) == m_criteria.visibleLevels.end())
            {
                return false;
            }
        }

        // 检查Category过滤
        if (!m_criteria.visibleCategories.empty())
        {
            if (m_criteria.visibleCategories.find(message.category) == m_criteria.visibleCategories.end())
            {
                return false;
            }
        }

        return true;
    }
}
