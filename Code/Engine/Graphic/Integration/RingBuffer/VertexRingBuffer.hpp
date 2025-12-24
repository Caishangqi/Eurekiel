#pragma once
//-----------------------------------------------------------------------------------------------
// VertexRingBuffer.hpp
//
// [NEW] Dedicated wrapper class for immediate mode vertex Ring Buffer operations
//
// Design Goals:
//   - Encapsulate D12VertexBuffer + offset state together (Option D architecture)
//   - RAII resource management in constructor
//   - Stateless helper functions moved to member methods
//   - Clean separation from D12VertexBuffer (pure resource class)
//
// Architecture:
//   - VertexRingBuffer owns the D12VertexBuffer and manages append offset
//   - D12VertexBuffer remains a pure GPU resource class without Ring Buffer logic
//   - RendererSubsystem uses VertexRingBuffer instead of raw buffer + offset
//
// Usage:
//     VertexRingBuffer ringBuffer(initialSize, stride, "ImmediateVBO");
//     auto result = ringBuffer.Append(vertices, vertexCount, stride);
//     D3D12RenderSystem::BindVertexBuffer(result.vbv, 0);
//     Draw(vertexCount, 0);  // startVertex = 0 because VBV has correct BufferLocation
//
// Teaching Points:
//   - RAII ensures buffer is always valid after construction
//   - Encapsulation hides offset management complexity
//   - Single Responsibility: Ring Buffer logic separate from GPU resource
//
//-----------------------------------------------------------------------------------------------

#include <memory>
#include <cstddef>
#include <string>
#include <exception>
#include <d3d12.h>

#include "Engine/Core/LogCategory/LogCategory.hpp"

namespace enigma::graphic
{
    // Forward declarations
    class D12VertexBuffer;

    //-------------------------------------------------------------------------------------------
    // Log Category Declaration
    //
    // [NEW] RingBuffer module log category for consistent logging
    // Use with LogInfo, LogWarn, LogError macros:
    //   LogInfo(LogRingBuffer, "Message with format: %s", arg);
    //
    // Definition in VertexRingBuffer.cpp:
    //   DEFINE_LOG_CATEGORY(LogRingBuffer);
    //-------------------------------------------------------------------------------------------
    DECLARE_LOG_CATEGORY_EXTERN(LogRingBuffer);

    //-------------------------------------------------------------------------------------------
    // RingBufferException
    //
    // Base exception class for Ring Buffer module errors
    // Inherits from std::exception for standard C++ exception handling
    //
    // Error Handling Strategy:
    //   - Throw RingBufferException from Ring Buffer operations
    //   - Caller catches and applies ERROR_AND_DIE or ERROR_RECOVERABLE
    //-------------------------------------------------------------------------------------------
    class RingBufferException : public std::exception
    {
    public:
        explicit RingBufferException(const std::string& message)
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

    //-------------------------------------------------------------------------------------------
    // RingBufferAllocationException
    //
    // Thrown when buffer allocation or resize fails
    // Mapping: ERROR_AND_DIE (cannot continue without buffer)
    //-------------------------------------------------------------------------------------------
    class RingBufferAllocationException : public RingBufferException
    {
    public:
        using RingBufferException::RingBufferException;
    };

    //-------------------------------------------------------------------------------------------
    // RingBufferOverflowException
    //
    // Thrown when Ring Buffer capacity is exceeded and cannot resize
    // Mapping: ERROR_AND_DIE (data loss would occur)
    //-------------------------------------------------------------------------------------------
    class RingBufferOverflowException : public RingBufferException
    {
    public:
        using RingBufferException::RingBufferException;
    };

    //-------------------------------------------------------------------------------------------
    // VertexAppendResult
    //
    // [NEW] Result of Append operation
    // Contains VBV ready for binding with BufferLocation pointing to appended data
    //-------------------------------------------------------------------------------------------
    struct VertexAppendResult
    {
        D3D12_VERTEX_BUFFER_VIEW vbv; // VBV with correct BufferLocation offset
        size_t                   byteOffset; // Byte offset in buffer (for debugging)
        size_t                   byteSize; // Size of appended data in bytes
    };

    //-------------------------------------------------------------------------------------------
    // VertexRingBuffer
    //
    // [NEW] Dedicated wrapper class for immediate mode vertex Ring Buffer operations
    //
    // Encapsulates:
    //   - D12VertexBuffer (GPU resource)
    //   - Current append offset (state)
    //   - Auto-resize logic
    //   - VBV creation with correct BufferLocation
    //
    // RAII Guarantees:
    //   - Buffer is valid after construction or exception is thrown
    //   - Resources are released in destructor
    //
    // Thread Safety:
    //   - Not thread-safe (single-threaded immediate mode rendering)
    //   - Frame synchronization handled by caller
    //-------------------------------------------------------------------------------------------
    class VertexRingBuffer
    {
    public:
        //-----------------------------------------------------------------------------------
        // Constructor: Initialize Ring Buffer with specified size
        //
        // @param initialSize Initial buffer size in bytes
        // @param stride Default stride for vertices (can be overridden per-append)
        // @param debugName Debug name for GPU resource
        //
        // RAII: Buffer is created and mapped in constructor
        // Throws: RingBufferAllocationException if allocation fails
        //-----------------------------------------------------------------------------------
        VertexRingBuffer(size_t initialSize, size_t stride, const char* debugName = "VertexRingBuffer");

        //-----------------------------------------------------------------------------------
        // Destructor: Release resources
        //
        // RAII: D12VertexBuffer released via shared_ptr
        //-----------------------------------------------------------------------------------
        ~VertexRingBuffer();

        // Non-copyable, movable
        VertexRingBuffer(const VertexRingBuffer&)            = delete;
        VertexRingBuffer& operator=(const VertexRingBuffer&) = delete;
        VertexRingBuffer(VertexRingBuffer&&) noexcept;
        VertexRingBuffer& operator=(VertexRingBuffer&&) noexcept;

        //-----------------------------------------------------------------------------------
        // Append vertex data and create VBV with correct BufferLocation
        //
        // @param vertices Pointer to vertex data
        // @param vertexCount Number of vertices to append
        // @param stride Size of each vertex in bytes
        // @return VertexAppendResult with VBV ready for binding
        //
        // [FIX] Mixed-stride Ring Buffer solution:
        //   - Records byte offset BEFORE append
        //   - Sets VBV.BufferLocation to point directly to data position
        //   - Caller uses startVertex=0 since BufferLocation is already offset
        //
        // Auto-resize: Buffer grows if capacity exceeded
        // Throws: RingBufferOverflowException if resize fails
        //-----------------------------------------------------------------------------------
        VertexAppendResult Append(const void* vertices, size_t vertexCount, size_t stride);

        //-----------------------------------------------------------------------------------
        // Append vertex data from existing VertexBuffer
        //
        // @param sourceVBO Source VertexBuffer to copy data from
        // @return VertexAppendResult with VBV ready for binding
        //
        // [NEW] Convenience overload for copying from external VBO:
        //   - Automatically extracts mapped data, vertex count, and stride
        //   - Reduces boilerplate code at call sites
        //   - Validates source VBO before copying
        //
        // Throws: RingBufferAllocationException if sourceVBO is null or has no mapped data
        //-----------------------------------------------------------------------------------
        VertexAppendResult Append(const D12VertexBuffer* sourceVBO);

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

        // Get underlying D12VertexBuffer (for low-level operations if needed)
        D12VertexBuffer* GetBuffer() const { return m_buffer.get(); }

        // Get current append offset in bytes
        size_t GetCurrentOffset() const { return m_currentOffset; }

        // Get buffer capacity in bytes
        size_t GetCapacity() const;

        // Get default stride
        size_t GetDefaultStride() const { return m_defaultStride; }

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

        std::shared_ptr<D12VertexBuffer> m_buffer; // GPU vertex buffer resource
        size_t                           m_currentOffset; // Current append offset in bytes
        size_t                           m_defaultStride; // Default vertex stride
        std::string                      m_debugName; // Debug name for logging

        //-----------------------------------------------------------------------------------
        // Constants
        //-----------------------------------------------------------------------------------

        static constexpr size_t MIN_BUFFER_SIZE = 640 * 1024 * 1024; // 640MB minimum
        static constexpr float  GROWTH_FACTOR   = 1.5f; // Buffer growth multiplier
    };
} // namespace enigma::graphic
