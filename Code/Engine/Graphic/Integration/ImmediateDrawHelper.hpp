#pragma once

#include <memory>
#include <cstddef>
#include <d3d12.h>

namespace enigma::graphic
{
    // Forward declarations
    class D12VertexBuffer;
    class D12IndexBuffer;
}

/**
 * @struct VertexAppendResult
 * @brief Result of AppendVertexDataWithVBV operation
 * 
 * Contains the VBV ready for binding, with BufferLocation pointing
 * directly to the appended data position in the Ring Buffer.
 */
struct VertexAppendResult
{
    D3D12_VERTEX_BUFFER_VIEW vbv;
};

/**
 * @class ImmediateDrawHelper
 * @brief Static helper class for immediate mode drawing Ring Buffer operations
 * 
 * [NEW] 2025-01-06: Extract duplicate append logic from RendererSubsystem
 * - Encapsulates vertex and index data append operations
 * - Eliminates 200+ lines of duplicate code across DrawVertexArray overloads
 * - Follows shrimp-rules.md section 10: all methods static, no state
 * - Reuses BufferHelper::EnsureBufferSize() for buffer management
 * 
 * Architecture:
 * - Completely stateless (no member variables)
 * - All methods are static
 * - Constructor is deleted (cannot instantiate)
 * - Per-Frame Append strategy with Ring Buffer
 * 
 * Usage:
 *     uint32_t startVertex = ImmediateDrawHelper::AppendVertexData(
 *         m_immediateVBO, vertices, vertexCount, m_currentVertexOffset, sizeof(Vertex));
 */
using namespace enigma::graphic;

class ImmediateDrawHelper
{
public:
    // [REQUIRED] Delete constructor - no instantiation allowed
    ImmediateDrawHelper() = delete;

    /**
     * @brief Append vertex data to Ring Buffer VBO
     * @param buffer VertexBuffer to append to (auto-resizes if needed)
     * @param vertices Vertex data to append
     * @param vertexCount Number of vertices
     * @param currentOffset Current offset in buffer (updated after append)
     * @param stride Size of each vertex in bytes
     * @return Start vertex index for this draw call
     * 
     * Teaching Note: 
     * - Automatically ensures buffer capacity via BufferHelper::EnsureBufferSize
     * - Uses persistent mapped data (CPU-accessible GPU memory)
     * - Updates currentOffset for next append
     */
    static uint32_t AppendVertexData(
        std::shared_ptr<D12VertexBuffer>& buffer,
        const void*                       vertices,
        size_t                            vertexCount,
        size_t&                           currentOffset,
        size_t                            stride
    );

    /**
     * @brief Append index data to Ring Buffer IBO
     * @param buffer IndexBuffer to append to (auto-resizes if needed)
     * @param indices Index data to append
     * @param indexCount Number of indices
     * @param currentOffset Current offset in buffer (updated after append)
     * @return Start index for this draw call
     * 
     * Teaching Note:
     * - Automatically ensures buffer capacity via BufferHelper::EnsureBufferSize
     * - Uses persistent mapped data (CPU-accessible GPU memory)
     * - Updates currentOffset for next append
     */
    static uint32_t AppendIndexData(
        std::shared_ptr<D12IndexBuffer>& buffer,
        const unsigned*                  indices,
        size_t                           indexCount,
        size_t&                          currentOffset
    );

    /**
     * @brief Append vertex data and create VBV with correct BufferLocation
     * @param buffer VertexBuffer to append to (auto-resizes if needed)
     * @param vertices Vertex data to append
     * @param vertexCount Number of vertices
     * @param currentOffset Current offset in buffer (updated after append)
     * @param stride Size of each vertex in bytes
     * @return VertexAppendResult containing VBV ready for binding
     * 
     * [FIX] Mixed-stride Ring Buffer solution:
     * - Records byte offset before append
     * - Sets VBV.BufferLocation to point directly to data position
     * - Caller should use startVertex=0 since BufferLocation is already offset
     */
    static VertexAppendResult AppendVertexDataWithVBV(
        std::shared_ptr<D12VertexBuffer>& buffer,
        const void*                       vertices,
        size_t                            vertexCount,
        size_t&                           currentOffset,
        size_t                            stride
    );

private:
    // [CRITICAL] Buffer size constants (must match RendererSubsystemConfig)
    static constexpr size_t MIN_VBO_SIZE = 640 * 1024 * 1024; // 640MB (same as INITIAL_IMMEDIATE_BUFFER_SIZE)
    static constexpr size_t MIN_IBO_SIZE = 640 * 1024 * 1024; // 640MB (same as INITIAL_IMMEDIATE_BUFFER_SIZE)
};
