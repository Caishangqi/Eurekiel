#include "IndexBuffer.hpp"

#include "Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

IndexBuffer::IndexBuffer(ID3D11Device* device, unsigned int size): m_device(device), m_size(size), m_buffer(nullptr)
{
    Create();
}

IndexBuffer::~IndexBuffer()
{
    DX_SAFE_RELEASE(m_buffer)
}

void IndexBuffer::Create()
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

void IndexBuffer::Resize(unsigned int size)
{
    DX_SAFE_RELEASE(m_buffer)
    m_size = size;
    Create();
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
    return int(m_size / sizeof(unsigned int));
}
