#pragma once
#include <string>
#include <vector>
#include <deque>

namespace enigma::core
{
    // ============================================================================
    // Command history manager (header-only implementation)
    // ============================================================================
    class CommandHistory
    {
    public:
        CommandHistory(size_t maxSize = 1000)
            : m_maxSize(maxSize)
        {}

        // Add a command to history
        void Add(const std::string& command)
        {
            // Don't add empty commands
            if (command.empty())
                return;

            // Don't add duplicate consecutive commands
            if (!m_history.empty() && m_history.back() == command)
                return;

            m_history.push_back(command);

            // Maintain max size
            if (m_history.size() > m_maxSize)
                m_history.pop_front();

            // Reset navigation position
            m_navigationIndex = static_cast<int>(m_history.size());
        }

        // Clear all history
        void Clear()
        {
            m_history.clear();
            m_navigationIndex = 0;
        }

        // Get all history as vector (for display/export)
        std::vector<std::string> GetAll() const
        {
            return std::vector<std::string>(m_history.begin(), m_history.end());
        }

        // Get history size
        size_t GetSize() const
        {
            return m_history.size();
        }

        // Check if history is empty
        bool IsEmpty() const
        {
            return m_history.empty();
        }

        // Navigation methods (for arrow key navigation in console)
        // Returns empty string if no more history
        std::string NavigatePrevious()
        {
            if (m_history.empty())
                return "";

            if (m_navigationIndex > 0)
                m_navigationIndex--;

            if (m_navigationIndex >= 0 && m_navigationIndex < static_cast<int>(m_history.size()))
                return m_history[m_navigationIndex];

            return "";
        }

        std::string NavigateNext()
        {
            if (m_history.empty())
                return "";

            if (m_navigationIndex < static_cast<int>(m_history.size()) - 1)
                m_navigationIndex++;
            else
            {
                // At the end, return empty to allow new input
                m_navigationIndex = static_cast<int>(m_history.size());
                return "";
            }

            if (m_navigationIndex >= 0 && m_navigationIndex < static_cast<int>(m_history.size()))
                return m_history[m_navigationIndex];

            return "";
        }

        // Reset navigation to the end (after executing a command)
        void ResetNavigation()
        {
            m_navigationIndex = static_cast<int>(m_history.size());
        }

        // Get specific history entry by index (0 = oldest)
        std::string GetEntry(size_t index) const
        {
            if (index >= m_history.size())
                return "";
            return m_history[index];
        }

        // Get the most recent N entries
        std::vector<std::string> GetRecent(size_t count) const
        {
            std::vector<std::string> result;
            size_t startIndex = m_history.size() > count ? m_history.size() - count : 0;

            for (size_t i = startIndex; i < m_history.size(); ++i)
            {
                result.push_back(m_history[i]);
            }

            return result;
        }

        // Set maximum history size
        void SetMaxSize(size_t maxSize)
        {
            m_maxSize = maxSize;

            // Trim history if necessary
            while (m_history.size() > m_maxSize)
                m_history.pop_front();

            // Adjust navigation index if needed
            if (m_navigationIndex > static_cast<int>(m_history.size()))
                m_navigationIndex = static_cast<int>(m_history.size());
        }

        size_t GetMaxSize() const
        {
            return m_maxSize;
        }

    private:
        std::deque<std::string> m_history;
        size_t                  m_maxSize;
        int                     m_navigationIndex = 0; // Current position in history navigation
    };
}
