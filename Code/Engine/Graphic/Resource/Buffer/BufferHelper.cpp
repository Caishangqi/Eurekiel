#include "BufferHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp" // Task 4: Stringf for error messages
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

// ================================================================================================
// [NEW] ConstantBuffer辅助方法实现
// ================================================================================================

size_t BufferHelper::CalculateAlignedSize(size_t rawSize)
{
    // D3D12要求ConstantBuffer必须256字节对齐
    // 公式：(rawSize + 255) & ~255
    // 示例：100 → 256, 300 → 512, 256 → 256
    return (rawSize + CONSTANT_BUFFER_ALIGNMENT - 1) & ~(CONSTANT_BUFFER_ALIGNMENT - 1);
}

size_t BufferHelper::CalculateBufferCount(size_t totalSize, size_t elementSize)
{
    // 计算总大小可以容纳多少个元素
    // 教学要点：整数除法，向下取整
    if (elementSize == 0)
    {
        LogWarn(LogRenderer, "BufferHelper::CalculateBufferCount: elementSize is 0, returning 0");
        return 0;
    }
    return totalSize / elementSize;
}

bool BufferHelper::IsEngineReservedSlot(uint32_t slot)
{
    // 引擎保留slot范围：0-14
    return slot <= MAX_ENGINE_RESERVED_SLOT;
}

bool BufferHelper::IsUserSlot(uint32_t slot)
{
    // 用户自定义slot：>=15
    return slot > MAX_ENGINE_RESERVED_SLOT;
}

D3D12_GPU_VIRTUAL_ADDRESS BufferHelper::CalculateRootCBVAddress(ID3D12Resource* resource, size_t offset)
{
    // 计算ConstantBuffer的GPU虚拟地址
    // 教学要点：GPU虚拟地址 = 资源基地址 + 偏移量
    if (resource == nullptr)
    {
        LogError(LogRenderer, "BufferHelper::CalculateRootCBVAddress: resource is nullptr");
        return 0;
    }
    return resource->GetGPUVirtualAddress() + offset;
}

// ================================================================================================
// [EXISTING] VertexBuffer/IndexBuffer管理方法
// ================================================================================================

void BufferHelper::EnsureBufferSize(
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

        // [FIX] Make sure newSize is a multiple of stride (aligned upward)
        //Teaching points: D12VertexBuffer constructor requires size % stride == 0
        // For example: stride=60, newSize=65536 → 65580 (1093 * 60) after alignment
        if (newSize % stride != 0)
        {
            newSize = ((newSize + stride - 1) / stride) * stride;
        }

        // 创建新缓冲区
        buffer = std::make_shared<D12VertexBuffer>(
            newSize,
            stride,
            nullptr,
            debugName
        );

        // [FIX] Task 4 (2025-01-06): Persistent mapping for immediate buffers
        // 教学要点：Upload heap buffer必须在创建后立即persistent map，以支持per-frame append策略
        // 这确保GetPersistentMappedData()返回有效指针，避免DrawVertexArray中的ERROR_RECOVERABLE
        void* mappedPtr = buffer->MapPersistent();
        GUARANTEE_OR_DIE(mappedPtr != nullptr,
                         Stringf("BufferHelper: Failed to persistent map VertexBuffer '%s'", debugName).c_str());

        LogInfo(LogRenderer, "BufferHelper: Created/Resized VertexBuffer '%s' to %d bytes (persistent mapped)", debugName, newSize);
    }
}

void BufferHelper::EnsureBufferSize(
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

        // [FIX] 确保 newSize 是索引大小的倍数（向上对齐）
        // 教学要点：D12IndexBuffer 构造函数要求 size % indexSize == 0
        // Uint32 索引大小为 4 字节
        constexpr size_t indexSize = sizeof(uint32_t); // 4 bytes for Uint32
        if (newSize % indexSize != 0)
        {
            newSize = ((newSize + indexSize - 1) / indexSize) * indexSize;
        }

        // 创建新缓冲区
        buffer = std::make_shared<D12IndexBuffer>(
            newSize,
            D12IndexBuffer::IndexFormat::Uint32,
            nullptr,
            debugName
        );

        // [FIX] Task 4 (2025-01-06): Persistent mapping for immediate buffers
        // 教学要点：Upload heap buffer必须在创建后立即persistent map，以支持per-frame append策略
        // 这确保GetPersistentMappedData()返回有效指针，避免DrawVertexArray中的ERROR_RECOVERABLE
        void* mappedPtr = buffer->MapPersistent();
        GUARANTEE_OR_DIE(mappedPtr != nullptr,
                         Stringf("BufferHelper: Failed to persistent map IndexBuffer '%s'", debugName).c_str());

        LogInfo(LogRenderer, "BufferHelper: Created/Resized IndexBuffer '%s' to %zu bytes (persistent mapped)", debugName, newSize);
    }
}
