//-----------------------------------------------------------------------------------------------
// VertexRingBuffer.cpp
//
// [NEW] Implementation of VertexRingBuffer wrapper class
//
// Key Implementation Details:
//   - RAII: Buffer created in constructor, released in destructor
//   - Error Handling: Throws exceptions for fatal errors
//   - Logging: Uses LogRingBuffer category for diagnostics
//   - Auto-resize: Buffer grows when capacity exceeded
//
// Teaching Points:
//   - Exception-based error handling with ERROR_AND_DIE at catch sites
//   - Persistent mapped memory for efficient CPU-GPU data transfer
//   - Ring Buffer pattern for per-frame immediate mode rendering
//
//-----------------------------------------------------------------------------------------------

#include "VertexRingBuffer.hpp"

#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

#include <cstring>

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Log Category Definition
    //
    // [NEW] Define the LogRingBuffer log category
    // This creates the actual storage for the category declared in the header
    //-------------------------------------------------------------------------------------------
    DEFINE_LOG_CATEGORY(LogRingBuffer);

    //-------------------------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------------------------
    VertexRingBuffer::VertexRingBuffer(size_t initialSize, size_t stride, const char* debugName)
        : m_buffer(nullptr)
          , m_currentOffset(0)
          , m_defaultStride(stride)
          , m_debugName(debugName ? debugName : "VertexRingBuffer")
    {
        // [STEP 1] Validate parameters
        if (stride == 0)
        {
            throw RingBufferAllocationException(
                Stringf("VertexRingBuffer:: Invalid stride: 0. Debug name: %s", m_debugName.c_str())
            );
        }

        // [STEP 2] Ensure minimum buffer size
        const size_t requestedSize = (initialSize < MIN_BUFFER_SIZE) ? MIN_BUFFER_SIZE : initialSize;

        // [STEP 2.5] Align size to stride multiple (required by D12VertexBuffer)
        // D12VertexBuffer asserts: size % stride == 0
        const size_t actualSize = (requestedSize / stride) * stride;

        // [STEP 3] Create D12VertexBuffer with persistent mapping
        // Using nullptr for initialData to create Upload Heap buffer (CPU accessible)
        LogInfo(LogRingBuffer, "VertexRingBuffer:: Creating VertexRingBuffer: name=%s, size=%zu, stride=%zu",
                m_debugName.c_str(), actualSize, stride);

        m_buffer = std::make_shared<D12VertexBuffer>(
            actualSize,
            stride,
            nullptr, // No initial data - will be filled via Append
            m_debugName.c_str()
        );

        // [STEP 4] Validate buffer creation
        if (!m_buffer || !m_buffer->GetResource())
        {
            throw RingBufferAllocationException(
                Stringf("VertexRingBuffer:: Failed to create buffer. Name: %s, Size: %zu",
                        m_debugName.c_str(), actualSize)
            );
        }

        // [STEP 5] Validate persistent mapping
        void* mappedData = m_buffer->GetPersistentMappedData();
        if (!mappedData)
        {
            throw RingBufferAllocationException(
                Stringf("VertexRingBuffer:: Buffer not persistently mapped. Name: %s",
                        m_debugName.c_str())
            );
        }

        LogInfo(LogRingBuffer, "VertexRingBuffer:: Created successfully: name=%s, capacity=%zu bytes",
                m_debugName.c_str(), GetCapacity());
    }

    //-------------------------------------------------------------------------------------------
    // Destructor
    //-------------------------------------------------------------------------------------------
    VertexRingBuffer::~VertexRingBuffer()
    {
        if (m_buffer)
        {
            LogInfo(LogRingBuffer, "VertexRingBuffer:: Releasing VertexRingBuffer: name=%s", m_debugName.c_str());
        }
        // shared_ptr automatically releases the buffer
    }

    //-------------------------------------------------------------------------------------------
    // Move Constructor
    //-------------------------------------------------------------------------------------------
    VertexRingBuffer::VertexRingBuffer(VertexRingBuffer&& other) noexcept
        : m_buffer(std::move(other.m_buffer))
          , m_currentOffset(other.m_currentOffset)
          , m_defaultStride(other.m_defaultStride)
          , m_debugName(std::move(other.m_debugName))
    {
        other.m_currentOffset = 0;
        other.m_defaultStride = 0;
    }

    //-------------------------------------------------------------------------------------------
    // Move Assignment
    //-------------------------------------------------------------------------------------------
    VertexRingBuffer& VertexRingBuffer::operator=(VertexRingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            m_buffer        = std::move(other.m_buffer);
            m_currentOffset = other.m_currentOffset;
            m_defaultStride = other.m_defaultStride;
            m_debugName     = std::move(other.m_debugName);

            other.m_currentOffset = 0;
            other.m_defaultStride = 0;
        }
        return *this;
    }

    //-------------------------------------------------------------------------------------------
    // Append
    //-------------------------------------------------------------------------------------------
    VertexAppendResult VertexRingBuffer::Append(const void* vertices, size_t vertexCount, size_t stride)
    {
        // [STEP 1] Validate input parameters
        if (!vertices)
        {
            ERROR_AND_DIE("VertexRingBuffer::Append:: Null vertex data pointer");
        }
        if (vertexCount == 0)
        {
            ERROR_AND_DIE("VertexRingBuffer::Append:: Zero vertex count");
        }
        if (stride == 0)
        {
            ERROR_AND_DIE("VertexRingBuffer::Append:: Zero stride");
        }

        // [STEP 2] Calculate required size
        const size_t dataSize     = vertexCount * stride;
        const size_t requiredSize = m_currentOffset + dataSize;

        // [STEP 3] Ensure buffer has sufficient capacity
        EnsureCapacity(requiredSize);

        // [FIX] Record byte offset BEFORE modifying m_currentOffset
        // This is critical for mixed-stride Ring Buffer scenarios
        const size_t dataByteOffset = m_currentOffset;

        // [STEP 4] Copy vertex data to mapped memory
        CopyToBuffer(vertices, dataSize);

        // [STEP 5] Update offset for next append
        m_currentOffset += dataSize;

        // [STEP 6] Create VBV with BufferLocation pointing directly to data position
        // This avoids the offset/stride integer division problem (e.g., 3024/60=50.4)
        VertexAppendResult result = {};
        result.vbv.BufferLocation = m_buffer->GetResource()->GetGPUVirtualAddress() + dataByteOffset;
        result.vbv.SizeInBytes    = static_cast<UINT>(dataSize);
        result.vbv.StrideInBytes  = static_cast<UINT>(stride);
        result.byteOffset         = dataByteOffset;
        result.byteSize           = dataSize;

        return result;
    }

    //-------------------------------------------------------------------------------------------
    // Append (from existing VertexBuffer)
    //-------------------------------------------------------------------------------------------
    VertexAppendResult VertexRingBuffer::Append(const D12VertexBuffer* sourceVBO)
    {
        // [STEP 1] Validate source VBO
        if (!sourceVBO)
        {
            throw RingBufferAllocationException(
                "VertexRingBuffer::Append:: Source VBO is null"
            );
        }

        // [STEP 2] Get vertex count and stride from source
        const size_t vertexCount = sourceVBO->GetVertexCount();
        const size_t stride      = sourceVBO->GetStride();

        if (vertexCount == 0)
        {
            throw RingBufferAllocationException(
                "VertexRingBuffer::Append:: Source VBO has zero vertex count"
            );
        }

        // [STEP 3] Get persistent mapped data from source
        void* sourceData = sourceVBO->GetPersistentMappedData();
        if (!sourceData)
        {
            throw RingBufferAllocationException(
                Stringf("VertexRingBuffer::Append:: Source VBO has no mapped data. "
                    "Ensure source VBO was created with persistent mapping.")
            );
        }

        // [STEP 4] Delegate to base Append implementation
        return Append(sourceData, vertexCount, stride);
    }

    //-------------------------------------------------------------------------------------------
    // Reset
    //-------------------------------------------------------------------------------------------
    void VertexRingBuffer::Reset()
    {
        // Per-Frame Ring Buffer Strategy:
        // Reset offset to beginning at frame start
        // Previous frame data is still valid on GPU (fence synchronized)
        m_currentOffset = 0;
    }

    //-------------------------------------------------------------------------------------------
    // GetCapacity
    //-------------------------------------------------------------------------------------------
    size_t VertexRingBuffer::GetCapacity() const
    {
        return m_buffer ? m_buffer->GetSize() : 0;
    }

    //-------------------------------------------------------------------------------------------
    // EnsureCapacity
    //-------------------------------------------------------------------------------------------
    void VertexRingBuffer::EnsureCapacity(size_t requiredSize)
    {
        const size_t currentCapacity = GetCapacity();

        if (requiredSize <= currentCapacity)
        {
            return; // Sufficient capacity
        }

        // [AUTO-RESIZE] Calculate new size with growth factor
        size_t newSize = static_cast<size_t>(static_cast<float>(requiredSize) * GROWTH_FACTOR);
        if (newSize < MIN_BUFFER_SIZE)
        {
            newSize = MIN_BUFFER_SIZE;
        }

        LogWarn(LogRingBuffer, "VertexRingBuffer::Resize:: Capacity exceeded: name=%s, required=%zu, current=%zu, new=%zu",
                m_debugName.c_str(), requiredSize, currentCapacity, newSize);

        // Use BufferHelper for resize operation
        BufferHelper::EnsureBufferSize(
            m_buffer,
            requiredSize,
            MIN_BUFFER_SIZE,
            m_defaultStride,
            m_debugName.c_str()
        );

        // Validate resize succeeded
        if (!m_buffer || !m_buffer->GetResource())
        {
            throw RingBufferOverflowException(
                Stringf("VertexRingBuffer:: Resize failed. Name: %s, Required: %zu",
                        m_debugName.c_str(), requiredSize)
            );
        }

        // Validate persistent mapping still valid
        if (!m_buffer->GetPersistentMappedData())
        {
            throw RingBufferOverflowException(
                Stringf("VertexRingBuffer:: Buffer not mapped after resize. Name: %s",
                        m_debugName.c_str())
            );
        }

        LogInfo(LogRingBuffer, "VertexRingBuffer:: Resized: name=%s, newCapacity=%zu",
                m_debugName.c_str(), GetCapacity());
    }

    //-------------------------------------------------------------------------------------------
    // CopyToBuffer
    //-------------------------------------------------------------------------------------------
    void VertexRingBuffer::CopyToBuffer(const void* data, size_t dataSize)
    {
        void* mappedData = m_buffer->GetPersistentMappedData();

        // This should not happen if EnsureCapacity succeeded
        GUARANTEE_OR_DIE(mappedData != nullptr,
                         "VertexRingBuffer::CopyToBuffer:: Buffer not persistently mapped");

        // Calculate destination pointer
        void* destPtr = static_cast<char*>(mappedData) + m_currentOffset;

        // Copy data to mapped memory
        std::memcpy(destPtr, data, dataSize);
    }
} // namespace enigma::graphic
