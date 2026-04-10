#pragma once

#include <exception>
#include <string>

namespace enigma::core
{
    //-----------------------------------------------------------------------------------------------
    // ScheduleException
    // Base exception type for schedule and task lifecycle errors.
    //-----------------------------------------------------------------------------------------------
    class ScheduleException : public std::exception
    {
    public:
        explicit ScheduleException(const std::string& message)
            : m_message(message)
        {
        }

        const char* what() const noexcept override
        {
            return m_message.c_str();
        }

    private:
        std::string m_message;
    };

    class InvalidTaskHandleException : public ScheduleException
    {
    public:
        using ScheduleException::ScheduleException;
    };

    class TaskStateTransitionException : public ScheduleException
    {
    public:
        using ScheduleException::ScheduleException;
    };

    class TaskCancellationConflictException : public ScheduleException
    {
    public:
        using ScheduleException::ScheduleException;
    };

    class TaskPublicationConflictException : public ScheduleException
    {
    public:
        using ScheduleException::ScheduleException;
    };

    class UnsupportedTaskPolicyException : public ScheduleException
    {
    public:
        using ScheduleException::ScheduleException;
    };
}
