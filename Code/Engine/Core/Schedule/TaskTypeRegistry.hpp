#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // Task Type Registry
    // Manages registration of task types and their thread count allocations
    //-----------------------------------------------------------------------------------------------
    class TaskTypeRegistry
    {
    public:
        // Register a task type with specified thread count
        void RegisterType(const std::string& typeStr, int threadCount);

        // Check if a type is registered
        bool IsTypeRegistered(const std::string& typeStr) const;

        // Get thread count for a type (returns 0 if not registered)
        int GetThreadCount(const std::string& typeStr) const;

        // Get all registered type names
        std::vector<std::string> GetAllTypes() const;

        // Get total thread count across all types
        int GetTotalThreadCount() const;

    private:
        // Validate type name (alphanumeric + underscore only)
        bool IsValidTypeName(const std::string& typeStr) const;

    private:
        std::map<std::string, int> m_typeThreadCounts;   // Type -> ThreadCount mapping
        std::set<std::string> m_registeredTypes;         // Fast lookup set
    };
}
