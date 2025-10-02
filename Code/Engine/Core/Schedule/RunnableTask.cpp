#include "RunnableTask.hpp"

namespace enigma::core
{
    RunnableTask::RunnableTask(const std::string& typeStr)
        : m_type(typeStr)
        , m_state(TaskState::Queued)
    {
    }
}
