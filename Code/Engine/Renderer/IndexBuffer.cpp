#include "IndexBuffer.hpp"

#include <ThirdParty/d3dx12/d3dx12.h>

#include "GraphicsError.hpp"
#include "Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

IndexBuffer::IndexBuffer(ID3D11Device* device, unsigned int size) : m_buffer(nullptr), m_device(device), m_size(size)
{
    Create();
}

IndexBuffer::IndexBuffer(ID3D12Device* device, unsigned int size) : m_buffer(nullptr), m_dx12device(device), m_size(size)
{
    Create();
}

IndexBuffer::~IndexBuffer()
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12buffer)
    m_dx12device = nullptr;
    m_device     = nullptr;
}

void IndexBuffer::Create()
{
    if (m_device)
    {
        UINT              indexBufferSize = m_size;
        D3D11_BUFFER_DESC bufferDesc      = {};
        bufferDesc.Usage                  = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth              = indexBufferSize;
        bufferDesc.BindFlags              = D3D11_BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags         = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateBuffer(&bufferDesc, nullptr, &m_buffer);
        if (!SUCCEEDED(hr))
        {
            ERROR_AND_DIE("Could not create index buffer.")
        }
        return;
    }
    // Create commited resource for index buffer
    if (m_dx12device)
    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC   resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_size);

        m_dx12device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE, &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_dx12buffer)
        ) >> chk;

        // Mapping CPU pointer
        m_dx12buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cpuPtr)) >> chk;
        m_baseGpuAddress = m_dx12buffer->GetGPUVirtualAddress();

        /// Create the view for index buffer
        {
            m_indexBufferView.BufferLocation = m_dx12buffer->GetGPUVirtualAddress();
            m_indexBufferView.SizeInBytes    = m_size;
            // Need be R32 not R16 because we use unsigned int which is 4 byte - 32bit
            m_indexBufferView.Format = DXGI_FORMAT_R32_UINT; // R is stand by red channel
            /// R32_UINT        R       32-bit          unsigned int    4B
            /// R32G32_FLOAT    R + G   32-bit each     float           8B
            /// R16_UINT        R       16-bit  	    unsigned int    2B
            /// R8G8B8A8_UNORM  RGBA    8-bit each      normalized      4B
        }
    }
}

void IndexBuffer::Update(const unsigned int* indices, unsigned int count, ID3D11DeviceContext* deviceContext)
{
    // 计算需要的数据大小（字节）
    unsigned int dataSize = count * sizeof(unsigned int); // 如果当前缓冲区大小不足，则重新分配
    if (dataSize > m_size) { Resize(dataSize); }
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT                  hr = deviceContext->Map(m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        ERROR_AND_DIE("Failed to map index buffer for update.")
    }

    memcpy(mappedResource.pData, indices, dataSize);

    // 解除映射
    deviceContext->Unmap(m_buffer, 0);
}

void IndexBuffer::Update(const unsigned int* indices, unsigned int count, ID3D12Device* deviceContext)
{
    UNUSED(deviceContext)
    unsigned int dataSize = count * sizeof(unsigned int);
    if (dataSize > m_size) { Resize(dataSize); }
    CD3DX12_RANGE rng(0, 0);
    uint8_t*      dst = nullptr;
    m_dx12buffer->Map(0, &rng, reinterpret_cast<void**>(&dst)) >> chk;
    memcpy(dst, indices, dataSize);
    m_dx12buffer->Unmap(0, nullptr);
}

void IndexBuffer::Resize(unsigned int size)
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12buffer)
    m_size = size;
    Create();
}

void IndexBuffer::ResetCursor()
{
    m_cursor                         = 0;
    m_indexBufferView.BufferLocation = m_baseGpuAddress;
}

bool IndexBuffer::Allocate(const void* src, size_t size)
{
    size_t aligned = AlignUp<size_t>(size, 16);
    if (m_cursor + aligned > m_size)
    {
        ERROR_AND_DIE("Exceed the Index buffer max amount")
    }

    memcpy(m_cpuPtr + m_cursor, src, size);

    m_indexBufferView.BufferLocation = m_baseGpuAddress + m_cursor;
    m_indexBufferView.SizeInBytes    = (UINT)size;
    m_indexBufferView.Format         = DXGI_FORMAT_R32_UINT;

    m_cursor += aligned;
    return true;
}

unsigned int IndexBuffer::GetSize()
{
    return m_size;
}

unsigned int IndexBuffer::GetStride()
{
    return 0;
}

unsigned int IndexBuffer::GetCount()
{
    return static_cast<int>(m_size / sizeof(unsigned int));
}
