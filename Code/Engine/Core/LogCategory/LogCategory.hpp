#pragma once

#include <cstdint>
#include "../Logger/LogLevel.hpp"

namespace enigma::core
{
    /**
     * @brief Base class for log categories providing compile-time type safety
     *
     * Log categories enable filtering and organization of log messages.
     * Each category has a unique name and ID for efficient lookup.
     *
     * Usage:
     *   1. Declare in header: DECLARE_LOG_CATEGORY_EXTERN(LogMyModule);
     *   2. Define in cpp: DEFINE_LOG_CATEGORY(LogMyModule);
     *   3. Use with Logger: LogInfo(LogMyModule, "Message");
     */
    class LogCategoryBase
    {
    public:
        constexpr LogCategoryBase(const char* categoryName, uint32_t categoryId,
                                  LogLevel defaultLevel = LogLevel::TRACE)
            : m_name(categoryName)
            , m_id(categoryId)
            , m_defaultLevel(defaultLevel)
        {}

        constexpr const char* GetName() const { return m_name; }
        constexpr uint32_t GetId() const { return m_id; }
        constexpr LogLevel GetDefaultLevel() const { return m_defaultLevel; }

        // Implicit conversion to const char* for convenient string usage
        operator const char*() const { return m_name; }

    private:
        const char* m_name;
        uint32_t m_id;
        LogLevel m_defaultLevel;
    };

    /**
     * @brief Declare a log category in a header file (extern declaration)
     *
     * Example:
     *   // MyModule.hpp
     *   DECLARE_LOG_CATEGORY_EXTERN(LogMyModule);
     */
    #define DECLARE_LOG_CATEGORY_EXTERN(CategoryName) \
        extern const enigma::core::LogCategoryBase CategoryName;

    /**
     * @brief Define a log category in a cpp file
     *
     * Example:
     *   // MyModule.cpp
     *   DEFINE_LOG_CATEGORY(LogMyModule);  // Defaults to TRACE level
     *   DEFINE_LOG_CATEGORY(LogMyModule, LogLevel::INFO);  // Specify INFO level
     */
    #define DEFINE_LOG_CATEGORY(CategoryName, ...) \
        const enigma::core::LogCategoryBase CategoryName(#CategoryName, __COUNTER__, ##__VA_ARGS__);
}
