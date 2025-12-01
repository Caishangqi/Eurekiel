#include "Engine/Graphic/Integration/ImmediateDrawHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <cstring>

using namespace enigma::graphic;

// [NEW] Implementation of AppendVertexData method
uint32_t ImmediateDrawHelper::AppendVertexData(
    std::shared_ptr<D12VertexBuffer>& buffer,
    const void*                       vertices,
    size_t                            vertexCount,
    size_t&                           currentOffset,
    size_t                            stride)
{
    // [STEP 1] Calculate required buffer size
    const size_t dataSize     = vertexCount * stride;
    const size_t requiredSize = currentOffset + dataSize;

    // [STEP 2] Ensure buffer has sufficient capacity (auto-resize if needed)
    BufferHelper::EnsureBufferSize(
        buffer,
        requiredSize,
        MIN_VBO_SIZE,
        stride,
        "ImmediateVBO"
    );

    // [STEP 3] Get persistent mapped CPU pointer
    void* mappedData = buffer->GetPersistentMappedData();
    GUARANTEE_OR_DIE(mappedData != nullptr, "ImmediateDrawHelper: VertexBuffer not persistently mapped");

    // [STEP 4] Copy vertex data to mapped memory at current offset
    void* destPtr = static_cast<char*>(mappedData) + currentOffset;
    std::memcpy(destPtr, vertices, dataSize);

    // [STEP 5] Calculate start vertex index (offset in vertices, not bytes)
    const uint32_t startVertex = static_cast<uint32_t>(currentOffset / stride);

    // [STEP 6] Update offset for next append
    currentOffset += dataSize;

    return startVertex;
}

// [NEW] Implementation of AppendIndexData method
uint32_t ImmediateDrawHelper::AppendIndexData(
    std::shared_ptr<D12IndexBuffer>& buffer,
    const unsigned*                  indices,
    size_t                           indexCount,
    size_t&                          currentOffset)
{
    // [STEP 1] Calculate required buffer size (index size = 4 bytes for uint32)
    constexpr size_t INDEX_SIZE   = sizeof(unsigned);
    const size_t     dataSize     = indexCount * INDEX_SIZE;
    const size_t     requiredSize = currentOffset + dataSize;

    // [STEP 2] Ensure buffer has sufficient capacity (auto-resize if needed)
    BufferHelper::EnsureBufferSize(
        buffer,
        requiredSize,
        MIN_IBO_SIZE,
        "ImmediateIBO"
    );

    // [STEP 3] Get persistent mapped CPU pointer
    void* mappedData = buffer->GetPersistentMappedData();
    GUARANTEE_OR_DIE(mappedData != nullptr, "ImmediateDrawHelper: IndexBuffer not persistently mapped");

    // [STEP 4] Copy index data to mapped memory at current offset
    void* destPtr = static_cast<char*>(mappedData) + currentOffset;
    std::memcpy(destPtr, indices, dataSize);

    // [STEP 5] Calculate start index (offset in indices, not bytes)
    const uint32_t startIndex = static_cast<uint32_t>(currentOffset / INDEX_SIZE);

    // [STEP 6] Update offset for next append
    currentOffset += dataSize;

    return startIndex;
}
