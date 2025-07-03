#include "VertexBuffer.hpp"

#include <ThirdParty/d3dx12/d3dx12.h>

#include "GraphicsError.hpp"
#include "Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

VertexBuffer::VertexBuffer(ID3D11Device* device, unsigned int size, unsigned int stride) : m_device(device), m_size(size), m_stride(stride)
{
    Create();
}

VertexBuffer::VertexBuffer(ID3D12Device* device, unsigned int size, unsigned int stride) : m_size(size), m_stride(stride), m_dx12device(device)
{
    Create();
}

VertexBuffer::~VertexBuffer()
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12buffer)
}

void VertexBuffer::Create()
{
    if (m_device)
    {
        UINT              vertexBufferSize = m_size;
        D3D11_BUFFER_DESC bufferDesc       = {};
        bufferDesc.Usage                   = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth               = vertexBufferSize;
        bufferDesc.BindFlags               = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags          = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateBuffer(&bufferDesc, nullptr, &m_buffer);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create vertex buffer.")
        }
        return;
    }
    else if (m_dx12device)
    {
        // Create commited resource for vertex buffer
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC   resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_size);

        /// The step you create resource in DirectX12
        /// 1. You create the big heap on GPU (Chunk of memory)
        /// 2. Once you have the big chunk of memory you can choose where to place all

        /// Commited resource is a special case
        /// Direct 3D will create a heap of the same size for you for that resource
        /// and it will tie it to the resource. And it called implicity heap (you can not see it) because
        /// it all managed for you behind scenes by Direct3D
        HRESULT hr = m_dx12device->CreateCommittedResource(
            &heapProps, // Create heap (need to know the things that describe the heap)
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc, // Create Also resource ( need to know the things that describe the resource
            D3D12_RESOURCE_STATE_GENERIC_READ, // What is the state of the resource after we created it
            nullptr,
            IID_PPV_ARGS(&m_dx12buffer));
        GUARANTEE_OR_DIE(SUCCEEDED(hr), "DX12: CreateVertexBuffer failed")

        /// Map the buffer to GPU and save the CPU pointer
        m_dx12buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cpuPtr)) >> chk;

        /// Create the view for vertex buffer
        {
            m_baseGpuAddress                  = m_dx12buffer->GetGPUVirtualAddress(); // Where is the buffer located in GPU memory
            m_vertexBufferView.BufferLocation = m_baseGpuAddress;
            m_vertexBufferView.SizeInBytes    = m_size;
            m_vertexBufferView.StrideInBytes  = m_stride; // the distance between vertices
        }
    }
    else
    {
        ERROR_AND_DIE("No render device specify.")
    }
}

void VertexBuffer::Resize(unsigned int size)
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12buffer)
    m_size = size;
    Create();
}

unsigned int VertexBuffer::GetSize()
{
    return m_size;
}

unsigned int VertexBuffer::GetStride()
{
    return m_stride;
}

void VertexBuffer::ResetCursor()
{
    m_cursor                          = 0;
    m_vertexBufferView.BufferLocation = m_baseGpuAddress;
}

bool VertexBuffer::Allocate(const void* scr, size_t size)
{
    size_t aligned = AlignUp<size_t>(size, 16);
    if (m_cursor + aligned > m_size)
    {
        ERROR_AND_DIE("Exceed the Vertex buffer max amount")
    }

    /// The reason why m_cpuPtr + m_cursor can be used to locate a specific byte address in the buffer is mainly based on
    /// the following points
    ///
    /// 1. The type of m_cpuPtr is a byte pointer:
    /// m_cpuPtr is defined as uint8_t* (or BYTE*), which is a "byte pointer" VertexBuffer. C/C++ stipulates that pointer
    /// addition offsets the memory address according to the size of the element type pointed to by the pointer. Because
    /// the element type here is 1 byte (uint8_t), m_cpuPtr + N moves the address backward by N bytes.
    ///
    /// 2. m_cursor records the number of bytes currently written
    /// In Allocate, each call first calculates the number of aligned bytes to be written according to the alignment rule,
    /// and then accumulates this value in m_cursor. In this way, m_cursor always represents the byte offset of the next
    /// writable block relative to the beginning of the buffer.
    ///
    /// 3. memcpy(m_cpuPtr + m_cursor, scr, aligned) is to copy data from the beginning of the buffer plus the byte offset
    /// - m_cpuPtr points to the starting location where the GPU upload heap is mapped into the CPU address space.
    /// - m_cpuPtr + m_cursor points to the place where the current frame has not been filled with data.
    ///
    /// Because the pointer type is one byte wide, adding the number of bytes of m_cursor can accurately move to the target
    /// memory location, thereby realizing the cyclic filling of the "ring" buffer and multiple DrawCalls.
    memcpy(m_cpuPtr + m_cursor, scr, aligned);
    // Get the current view buffer position, After this manipulation, IASetVertexBuffers(0, 1, &vbo->m_vertexBufferView);
    // will interpolate the vertexBuffer from the position
    m_vertexBufferView.BufferLocation = m_baseGpuAddress + m_cursor;
    m_vertexBufferView.SizeInBytes    = (UINT)size; // the view size increase but the actual buffer size not change.
    m_vertexBufferView.StrideInBytes  = m_stride;

    m_cursor += aligned;
    return true;
}
