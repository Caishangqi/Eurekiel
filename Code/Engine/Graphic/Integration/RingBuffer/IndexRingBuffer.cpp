//-----------------------------------------------------------------------------------------------
// IndexRingBuffer.cpp
//
// [NEW] Implementation of IndexRingBuffer wrapper class
//
// Key Implementation Details:
//   - RAII: Buffer created in constructor, released in destructor
//   - Error Handling: Throws exceptions for fatal errors
//   - Logging: Uses LogRingBuffer category (shared with VertexRingBuffer)
//   - Auto-resize: Buffer grows when capacity exceeded
//   - Fixed index size: 4 bytes (uint32_t)
//
// Teaching Points:
//   - Exception-based error handling with ERROR_AND_DIE at catch sites
//   - Persistent mapped memory for efficient CPU-GPU data transfer
//   - Ring Buffer pattern for per-frame immediate mode rendering
//
//-----------------------------------------------------------------------------------------------

#include "IndexRingBuffer.hpp"

#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

#include <cstring>

#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // Constructor
    //-------------------------------------------------------------------------------------------
    IndexRingBuffer::IndexRingBuffer(size_t initialSize, const char* debugName)
        : m_buffer(nullptr)
          , m_currentOffset(0)
          , m_debugName(debugName ? debugName : "IndexRingBuffer")
    {
        // [STEP 1] Ensure minimum buffer size
        const size_t actualSize = (initialSize < MIN_BUFFER_SIZE) ? MIN_BUFFER_SIZE : initialSize;

        // [STEP 2] Create D12IndexBuffer with persistent mapping
        // Using nullptr for initialData to create Upload Heap buffer (CPU accessible)
        LogInfo(LogRingBuffer, "IndexRingBuffer:: Creating IndexRingBuffer: name=%s, size=%zu",
                m_debugName.c_str(), actualSize);

        m_buffer = std::make_shared<D12IndexBuffer>(
            actualSize,
            nullptr, // No initial data - will be filled via Append
            m_debugName.c_str()
        ); // [SIMPLIFIED] Always Uint32

        // [STEP 3] Validate buffer creation
        if (!m_buffer || !m_buffer->GetResource())
        {
            throw RingBufferAllocationException(
                Stringf("IndexRingBuffer:: Failed to create buffer. Name: %s, Size: %zu",
                        m_debugName.c_str(), actualSize)
            );
        }

        // [STEP 4] Validate persistent mapping
        void* mappedData = m_buffer->GetPersistentMappedData();
        if (!mappedData)
        {
            throw RingBufferAllocationException(
                Stringf("IndexRingBuffer:: Buffer not persistently mapped. Name: %s",
                        m_debugName.c_str())
            );
        }

        LogInfo(LogRingBuffer, "IndexRingBuffer:: IndexRingBuffer created successfully: name=%s, capacity=%zu bytes",
                m_debugName.c_str(), GetCapacity());
    }

    //-------------------------------------------------------------------------------------------
    // Destructor
    //-------------------------------------------------------------------------------------------
    IndexRingBuffer::~IndexRingBuffer()
    {
        if (m_buffer)
        {
            LogInfo(LogRingBuffer, "IndexRingBuffer:: Releasing IndexRingBuffer: name=%s", m_debugName.c_str());
        }
        // shared_ptr automatically releases the buffer
    }

    //-------------------------------------------------------------------------------------------
    // Move Constructor
    //-------------------------------------------------------------------------------------------
    IndexRingBuffer::IndexRingBuffer(IndexRingBuffer&& other) noexcept
        : m_buffer(std::move(other.m_buffer))
          , m_currentOffset(other.m_currentOffset)
          , m_debugName(std::move(other.m_debugName))
    {
        other.m_currentOffset = 0;
    }

    //-------------------------------------------------------------------------------------------
    // Move Assignment
    //-------------------------------------------------------------------------------------------
    IndexRingBuffer& IndexRingBuffer::operator=(IndexRingBuffer&& other) noexcept
    {
        if (this != &other)
        {
            m_buffer        = std::move(other.m_buffer);
            m_currentOffset = other.m_currentOffset;
            m_debugName     = std::move(other.m_debugName);

            other.m_currentOffset = 0;
        }
        return *this;
    }

    //-------------------------------------------------------------------------------------------
    // Append
    //-------------------------------------------------------------------------------------------
    IndexAppendResult IndexRingBuffer::Append(const unsigned* indices, size_t indexCount)
    {
        // [STEP 1] Validate input parameters
        if (!indices)
        {
            ERROR_AND_DIE("IndexRingBuffer::Append:: Null index data pointer")
        }
        if (indexCount == 0)
        {
            ERROR_AND_DIE("IndexRingBuffer::Append:: Zero index count")
        }

        // [STEP 2] Calculate required size
        const size_t dataSize     = indexCount * D12IndexBuffer::INDEX_SIZE;
        const size_t requiredSize = m_currentOffset + dataSize;

        // [STEP 3] Ensure buffer has sufficient capacity
        EnsureCapacity(requiredSize);

        // [STEP 4] Record byte offset for start index calculation
        const size_t dataByteOffset = m_currentOffset;

        // [STEP 5] Copy index data to mapped memory
        CopyToBuffer(indices, dataSize);

        // [STEP 6] Update offset for next append
        m_currentOffset += dataSize;

        // [STEP 7] Calculate start index (offset in indices, not bytes)
        IndexAppendResult result = {};
        result.startIndex        = static_cast<uint32_t>(dataByteOffset / D12IndexBuffer::INDEX_SIZE);
        result.byteOffset        = dataByteOffset;
        result.byteSize          = dataSize;

        return result;
    }

    //-------------------------------------------------------------------------------------------
    // Append (D12IndexBuffer* overload)
    //-------------------------------------------------------------------------------------------
    IndexAppendResult IndexRingBuffer::Append(const D12IndexBuffer* sourceIBO)
    {
        // [STEP 1] Validate source buffer
        if (!sourceIBO)
        {
            throw RingBufferAllocationException(
                "IndexRingBuffer::Append:: Null source D12IndexBuffer pointer"
            );
        }

        // [STEP 2] Get index count from source buffer
        const size_t indexCount = sourceIBO->GetIndexCount();
        if (indexCount == 0)
        {
            throw RingBufferAllocationException(
                Stringf("IndexRingBuffer::Append:: Source D12IndexBuffer has zero indices. Name: %s",
                        m_debugName.c_str())
            );
        }

        // [STEP 3] Get persistent mapped data from source
        void* sourceData = sourceIBO->GetPersistentMappedData();
        if (!sourceData)
        {
            throw RingBufferAllocationException(
                Stringf("IndexRingBuffer::Append:: Source D12IndexBuffer not persistently mapped. Name: %s",
                        m_debugName.c_str())
            );
        }

        // [STEP 4] Delegate to raw data Append
        return Append(static_cast<const unsigned*>(sourceData), indexCount);
    }

    //-------------------------------------------------------------------------------------------
    // Reset
    //-------------------------------------------------------------------------------------------
    void IndexRingBuffer::Reset()
    {
        // Per-Frame Ring Buffer Strategy:
        // Reset offset to beginning at frame start
        // Previous frame data is still valid on GPU (fence synchronized)
        m_currentOffset = 0;
    }

    //-------------------------------------------------------------------------------------------
    // GetCapacity
    //-------------------------------------------------------------------------------------------
    size_t IndexRingBuffer::GetCapacity() const
    {
        return m_buffer ? m_buffer->GetSize() : 0;
    }

    //-------------------------------------------------------------------------------------------
    // EnsureCapacity
    //-------------------------------------------------------------------------------------------
    void IndexRingBuffer::EnsureCapacity(size_t requiredSize)
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

        LogWarn(LogRingBuffer, "IndexRingBuffer::Resize:: IndexRingBuffer capacity exceeded: name=%s, required=%zu, current=%zu, new=%zu",
                m_debugName.c_str(), requiredSize, currentCapacity, newSize);

        // Use BufferHelper for resize operation
        BufferHelper::EnsureBufferSize(
            m_buffer,
            requiredSize,
            MIN_BUFFER_SIZE,
            m_debugName.c_str()
        );

        // Validate resize succeeded
        if (!m_buffer || !m_buffer->GetResource())
        {
            throw RingBufferOverflowException(
                Stringf("IndexRingBuffer::Resize:: failed. Name: %s, Required: %zu",
                        m_debugName.c_str(), requiredSize)
            );
        }

        // Validate persistent mapping still valid
        if (!m_buffer->GetPersistentMappedData())
        {
            throw RingBufferOverflowException(
                Stringf("IndexRingBuffer::Buffer not mapped after resize. Name: %s",
                        m_debugName.c_str())
            );
        }

        LogInfo(LogRingBuffer, "IndexRingBuffer::Resize:: resized: name=%s, newCapacity=%zu",
                m_debugName.c_str(), GetCapacity());
    }

    //-------------------------------------------------------------------------------------------
    // CopyToBuffer
    //-------------------------------------------------------------------------------------------
    void IndexRingBuffer::CopyToBuffer(const void* data, size_t dataSize)
    {
        void* mappedData = m_buffer->GetPersistentMappedData();

        // This should not happen if EnsureCapacity succeeded
        GUARANTEE_OR_DIE(mappedData != nullptr,
                         "IndexRingBuffer::CopyToBuffer:: Buffer not persistently mapped");

        // Calculate destination pointer
        void* destPtr = static_cast<char*>(mappedData) + m_currentOffset;

        // Copy data to mapped memory
        std::memcpy(destPtr, data, dataSize);
    }
} // namespace enigma::graphic
