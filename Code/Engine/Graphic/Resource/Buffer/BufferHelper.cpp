#include "BufferHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp" // Task 4: Stringf for error messages
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Integration/RendererSubsystemConfig.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp" // For UpdateFrequency
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp" // For MAX_CUSTOM_BUFFERS

#include <cstring> // For std::memcpy

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
    return (rawSize + CONSTANT_BUFFER_ALIGNMENT - 1) & ~(CONSTANT_BUFFER_ALIGNMENT - 1);
}

size_t BufferHelper::CalculateBufferCount(size_t totalSize, size_t elementSize)
{
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
    if (resource == nullptr)
    {
        LogError(LogRenderer, "BufferHelper::CalculateRootCBVAddress: resource is nullptr");
        return 0;
    }
    return resource->GetGPUVirtualAddress() + offset;
}

void BufferHelper::CopyBufferData(void* dest, const void* src, size_t size)
{
    if (!dest || !src || size == 0)
    {
        LogError(LogRenderer, "BufferHelper::CopyBufferData: Invalid parameters (dest=%p, src=%p, size=%zu)",
                 dest, src, size);
        return;
    }
    std::memcpy(dest, src, size);
}

bool BufferHelper::ValidateBufferConfig(uint32_t slotId, size_t size, enigma::graphic::UpdateFrequency freq)
{
    // 验证Slot范围
    if (!ValidateSlotRange(slotId))
    {
        return false;
    }

    // 验证Buffer大小
    if (!ValidateBufferSize(size))
    {
        return false;
    }

    // 验证更新频率
    if (freq == enigma::graphic::UpdateFrequency::PerObject && size > 64 * 1024)
    {
        LogWarn(LogRenderer, "Large PerObject buffer (%zu bytes) may impact performance", size);
    }

    return true;
}

bool BufferHelper::ValidateSlotRange(uint32_t slotId)
{
    using enigma::graphic::BindlessRootSignature;

    uint32_t maxSlot = BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM +
        BindlessRootSignature::MAX_CUSTOM_BUFFERS - 1;

    if (slotId > maxSlot)
    {
        LogError(LogRenderer, "Slot %u out of range (max: %u)", slotId, maxSlot);
        return false;
    }
    return true;
}

bool BufferHelper::ValidateBufferSize(size_t size)
{
    if (size == 0)
    {
        LogError(LogRenderer, "Buffer size cannot be 0");
        return false;
    }

    if (size > 64 * 1024)
    {
        LogWarn(LogRenderer, "Buffer size (%zu) exceeds 64KB, may impact performance", size);
    }

    return true;
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
        LogWarn(LogRenderer, "BufferHelper::Resize:: VBO resize triggered! old=%p, oldSize=%zu, requiredSize=%zu", buffer.get(), buffer ? buffer->GetSize() : 0, requiredSize);


        // 计算新大小：2倍增长策略，最小minSize，最大16MB
        size_t newSize = buffer
                             ? (std::min)((std::max)(requiredSize, buffer->GetSize() * 2), RendererSubsystemConfig::MAX_IMMEDIATE_BUFFER_SIZE)
                             : (std::max)(requiredSize, minSize);

        // 确保newSize是stride的倍数（向上对齐）
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

        // Persistent mapping supports per-frame append strategy
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

        // 确保newSize是索引大小的倍数（向上对齐）
        constexpr size_t indexSize = sizeof(uint32_t);
        if (newSize % indexSize != 0)
        {
            newSize = ((newSize + indexSize - 1) / indexSize) * indexSize;
        }

        //Create new buffer
        buffer = std::make_shared<D12IndexBuffer>(newSize, nullptr, debugName);

        // Persistent mapping supports per-frame append strategy
        void* mappedPtr = buffer->MapPersistent();
        GUARANTEE_OR_DIE(mappedPtr != nullptr,
                         Stringf("BufferHelper: Failed to persistent map IndexBuffer '%s'", debugName).c_str());

        LogInfo(LogRenderer, "BufferHelper: Created/Resized IndexBuffer '%s' to %zu bytes (persistent mapped)", debugName, newSize);
    }
}
