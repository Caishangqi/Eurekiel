#include "RendererHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Integration/RendererSubsystemConfig.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"

// 使用命名空间中的类型
using enigma::graphic::D12VertexBuffer;
using enigma::graphic::D12IndexBuffer;
using enigma::graphic::RendererSubsystemConfig;
using namespace enigma::core;

void RendererHelper::EnsureBufferSize(
    std::shared_ptr<D12VertexBuffer>& buffer,
    size_t                            requiredSize,
    size_t                            minSize,
    size_t                            stride,
    const char*                       debugName
)
{
    // 延迟创建或动态扩展
    if (!buffer || buffer->GetSize() < requiredSize)
    {
        // 计算新大小：2倍增长策略，最小minSize，最大16MB
        size_t newSize = buffer
                             ? (std::min)((std::max)(requiredSize, buffer->GetSize() * 2), RendererSubsystemConfig::MAX_IMMEDIATE_BUFFER_SIZE)
                             : (std::max)(requiredSize, minSize);

        // 创建新缓冲区
        buffer = std::make_shared<D12VertexBuffer>(
            newSize,
            stride,
            nullptr,
            debugName
        );

        LogInfo(LogRenderer, "RendererHelper: Created/Resized VertexBuffer '{}' to {} bytes", debugName, newSize);
    }
}

void RendererHelper::EnsureBufferSize(
    std::shared_ptr<D12IndexBuffer>& buffer,
    size_t                           requiredSize,
    size_t                           minSize,
    const char*                      debugName
)
{
    // 延迟创建或动态扩展
    if (!buffer || buffer->GetSize() < requiredSize)
    {
        // 计算新大小：2倍增长策略，最小minSize，最大16MB
        size_t newSize = buffer
                             ? (std::min)((std::max)(requiredSize, buffer->GetSize() * 2), RendererSubsystemConfig::MAX_IMMEDIATE_BUFFER_SIZE)
                             : (std::max)(requiredSize, minSize);

        // 创建新缓冲区
        buffer = std::make_shared<D12IndexBuffer>(
            newSize,
            D12IndexBuffer::IndexFormat::Uint32,
            nullptr,
            debugName
        );

        LogInfo(LogRenderer, "RendererHelper: Created/Resized IndexBuffer '{}' to {} bytes", debugName, newSize);
    }
}
