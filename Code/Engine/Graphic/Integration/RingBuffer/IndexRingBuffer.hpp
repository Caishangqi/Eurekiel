#pragma once
//-----------------------------------------------------------------------------------------------
// IndexRingBuffer.hpp
//
// [NEW] Dedicated wrapper class for immediate mode index Ring Buffer operations
//
// Design Goals:
//   - Encapsulate D12IndexBuffer + offset state together (Option D architecture)
//   - RAII resource management in constructor
//   - Clean separation from D12IndexBuffer (pure resource class)
//   - Paired with VertexRingBuffer for complete immediate mode solution
//
// Architecture:
//   - IndexRingBuffer owns the D12IndexBuffer and manages append offset
//   - D12IndexBuffer remains a pure GPU resource class without Ring Buffer logic
//   - RendererSubsystem uses IndexRingBuffer instead of raw buffer + offset
//
// Usage:
//     IndexRingBuffer ringBuffer(initialSize, "ImmediateIBO");
//     auto result = ringBuffer.Append(indices, indexCount);
//     SetIndexBuffer(ringBuffer.GetBuffer());
//     DrawIndexed(indexCount, result.startIndex, 0);
//
// Teaching Points:
//   - RAII ensures buffer is always valid after construction
//   - Index size is fixed (4 bytes for uint32_t)
//   - Unlike VertexRingBuffer, no stride parameter needed
//
//-----------------------------------------------------------------------------------------------

#include <memory>
#include <cstddef>
#include <string>
#include <d3d12.h>

#include "VertexRingBuffer.hpp"  // For shared exceptions and log category

namespace enigma::graphic
{
    // Forward declarations
    class D12IndexBuffer;

    //-------------------------------------------------------------------------------------------
    // IndexAppendResult
    //
    // [NEW] Result of Append operation
    // Contains start index for DrawIndexed call
    //-------------------------------------------------------------------------------------------
    struct IndexAppendResult
    {
        uint32_t startIndex; // Start index for DrawIndexed
        size_t   byteOffset; // Byte offset in buffer (for debugging)
        size_t   byteSize; // Size of appended data in bytes
    };

    //-------------------------------------------------------------------------------------------
    // IndexRingBuffer
    //
    // [NEW] Dedicated wrapper class for immediate mode index Ring Buffer operations
    //
    // Encapsulates:
    //   - D12IndexBuffer (GPU resource)
    //   - Current append offset (state)
    //   - Auto-resize logic
    //   - Start index calculation
    //
    // RAII Guarantees:
    //   - Buffer is valid after construction or exception is thrown
    //   - Resources are released in destructor
    //
    // Thread Safety:
    //   - Not thread-safe (single-threaded immediate mode rendering)
    //   - Frame synchronization handled by caller
    //-------------------------------------------------------------------------------------------
    class IndexRingBuffer
    {
    public:
        //-----------------------------------------------------------------------------------
        // Constructor: Initialize Ring Buffer with specified size
        //
        // @param initialSize Initial buffer size in bytes
        // @param debugName Debug name for GPU resource
        //
        // RAII: Buffer is created and mapped in constructor
        // Throws: RingBufferAllocationException if allocation fails
        //-----------------------------------------------------------------------------------
        IndexRingBuffer(size_t initialSize, const char* debugName = "IndexRingBuffer");

        //-----------------------------------------------------------------------------------
        // Destructor: Release resources
        //
        // RAII: D12IndexBuffer released via shared_ptr
        //-----------------------------------------------------------------------------------
        ~IndexRingBuffer();

        // Non-copyable, movable
        IndexRingBuffer(const IndexRingBuffer&)            = delete;
        IndexRingBuffer& operator=(const IndexRingBuffer&) = delete;
        IndexRingBuffer(IndexRingBuffer&&) noexcept;
        IndexRingBuffer& operator=(IndexRingBuffer&&) noexcept;

        //-----------------------------------------------------------------------------------
        // Append index data to Ring Buffer
        //
        // @param indices Pointer to index data (uint32_t array)
        // @param indexCount Number of indices to append
        // @return IndexAppendResult with startIndex for DrawIndexed
        //
        // Index Type: Always uint32_t (4 bytes per index)
        // Auto-resize: Buffer grows if capacity exceeded
        // Throws: RingBufferOverflowException if resize fails
        //-----------------------------------------------------------------------------------
        IndexAppendResult Append(const unsigned* indices, size_t indexCount);

        //-----------------------------------------------------------------------------------
        // Append index data from existing D12IndexBuffer
        //
        // @param sourceIBO Pointer to source D12IndexBuffer
        // @return IndexAppendResult with startIndex for DrawIndexed
        //
        // Convenience overload for internal use (e.g., DrawVertexBuffer with IBO)
        // Reads from source buffer's persistent mapped data
        // Throws: RingBufferAllocationException if source is invalid
        //-----------------------------------------------------------------------------------
        IndexAppendResult Append(const D12IndexBuffer* sourceIBO);

        //-----------------------------------------------------------------------------------
        // Reset offset to beginning (call at frame start)
        //
        // Per-Frame Ring Buffer Strategy:
        //   - Reset at BeginFrame()
        //   - Append during frame
        //   - GPU consumes data before next frame reset
        //-----------------------------------------------------------------------------------
        void Reset();

        //-----------------------------------------------------------------------------------
        // Accessors
        //-----------------------------------------------------------------------------------

        // Get underlying D12IndexBuffer (for SetIndexBuffer binding)
        D12IndexBuffer* GetBuffer() const { return m_buffer.get(); }

        // Get current append offset in bytes
        size_t GetCurrentOffset() const { return m_currentOffset; }

        // Get buffer capacity in bytes
        size_t GetCapacity() const;

        // Get debug name
        const std::string& GetDebugName() const { return m_debugName; }

        // Check if buffer is valid
        bool IsValid() const { return m_buffer != nullptr; }

    private:
        //-----------------------------------------------------------------------------------
        // Private Helper Methods
        //-----------------------------------------------------------------------------------

        // Ensure buffer has sufficient capacity, resize if needed
        void EnsureCapacity(size_t requiredSize);

        // Copy data to mapped memory at current offset
        void CopyToBuffer(const void* data, size_t dataSize);

        //-----------------------------------------------------------------------------------
        // Member Variables
        //-----------------------------------------------------------------------------------

        std::shared_ptr<D12IndexBuffer> m_buffer; // GPU index buffer resource
        size_t                          m_currentOffset; // Current append offset in bytes
        std::string                     m_debugName; // Debug name for logging

        //-----------------------------------------------------------------------------------
        // Constants
        //-----------------------------------------------------------------------------------

        // [SIMPLIFIED] Use D12IndexBuffer::INDEX_SIZE instead of local constant
        static constexpr size_t MIN_BUFFER_SIZE = 640 * 1024 * 1024; // 640MB minimum
        static constexpr float  GROWTH_FACTOR   = 1.5f; // Buffer growth multiplier
    };
} // namespace enigma::graphic
