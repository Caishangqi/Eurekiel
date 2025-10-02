#include "TaskTypeRegistry.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "ScheduleSubsystem.hpp"
#include <algorithm>
#include <cctype>

namespace enigma::core
{
    void TaskTypeRegistry::RegisterType(const std::string& typeStr, int threadCount)
    {
        if (!IsValidTypeName(typeStr))
        {
            LogWarn(ScheduleSubsystem::GetStaticSubsystemName(), 
                    "Invalid task type name '%s'", typeStr.c_str());
            return;
        }

        if (threadCount <= 0)
        {
            LogWarn(ScheduleSubsystem::GetStaticSubsystemName(),
                    "Invalid thread count %d for type '%s'",
                    threadCount, typeStr.c_str());
            return;
        }

        m_typeThreadCounts[typeStr] = threadCount;
        m_registeredTypes.insert(typeStr);

        LogInfo(ScheduleSubsystem::GetStaticSubsystemName(),
                "Registered task type: %s -> %d threads",
                typeStr.c_str(), threadCount);
    }

    bool TaskTypeRegistry::IsTypeRegistered(const std::string& typeStr) const
    {
        return m_registeredTypes.find(typeStr) != m_registeredTypes.end();
    }

    int TaskTypeRegistry::GetThreadCount(const std::string& typeStr) const
    {
        auto it = m_typeThreadCounts.find(typeStr);
        return (it != m_typeThreadCounts.end()) ? it->second : 0;
    }

    std::vector<std::string> TaskTypeRegistry::GetAllTypes() const
    {
        return std::vector<std::string>(m_registeredTypes.begin(),
                                         m_registeredTypes.end());
    }

    int TaskTypeRegistry::GetTotalThreadCount() const
    {
        int total = 0;
        for (const auto& pair : m_typeThreadCounts)
            total += pair.second;
        return total;
    }

    bool TaskTypeRegistry::IsValidTypeName(const std::string& typeStr) const
    {
        if (typeStr.empty())
            return false;

        // Only allow alphanumeric characters and underscore
        for (char c : typeStr)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                return false;
        }

        return true;
    }
}
