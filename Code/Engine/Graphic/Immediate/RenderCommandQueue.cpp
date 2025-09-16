#include "RenderCommandQueue.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    RenderCommandQueue::RenderCommandQueue(const QueueConfig& config)
    {
        m_config = config;
    }

    RenderCommandQueue::~RenderCommandQueue()
    {
        Clear();
        enigma::core::LogInfo("RenderCommandQueue", "Destroyed");
    }

    void RenderCommandQueue::Clear()
    {
        // 桩实现
    }
} // namespace enigma::graphic
