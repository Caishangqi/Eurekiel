#pragma once

#include <exception>
#include <string>

namespace enigma::graphic
{
    // Queue-domain exception hierarchy used by the multi-queue runtime.
    //
    // The exception type captures the semantic category of the failure.
    // Call sites remain responsible for choosing the final error macro:
    // - ERROR_AND_DIE for fatal synchronization or retirement violations
    // - ERROR_RECOVERABLE for safe fallback or early-abort paths
    class CommandQueueException : public std::exception
    {
    public:
        explicit CommandQueueException(const std::string& message)
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

    // Thrown when a queue-scoped submission token is missing queue identity or fence value.
    class InvalidQueueSubmissionTokenException : public CommandQueueException
    {
    public:
        using CommandQueueException::CommandQueueException;
    };

    // Thrown when explicit queue-to-queue synchronization cannot be established safely.
    class CrossQueueSynchronizationException : public CommandQueueException
    {
    public:
        using CommandQueueException::CommandQueueException;
    };

    // Thrown when a frame slot is reused before every tracked queue has retired it.
    class FrameQueueRetirementException : public CommandQueueException
    {
    public:
        using CommandQueueException::CommandQueueException;
    };

    // Thrown when a workload cannot legally stay on the requested dedicated queue route.
    class UnsupportedQueueRouteException : public CommandQueueException
    {
    public:
        using CommandQueueException::CommandQueueException;
    };
}
