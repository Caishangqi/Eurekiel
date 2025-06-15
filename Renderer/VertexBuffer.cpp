#include "VertexBuffer.hpp"

#include <ThirdParty/d3dx12/d3dx12.h>

#include "Renderer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

VertexBuffer::VertexBuffer(ID3D11Device* device, unsigned int size, unsigned int stride): m_device(device), m_size(size), m_stride(stride)
{
    Create();
}

VertexBuffer::VertexBuffer(ID3D12Device* device, unsigned int size, unsigned int stride): m_dx12device(device), m_size(size), m_stride(stride)
{
    m_vertexBufferView = new D3D12_VERTEX_BUFFER_VIEW();
    Create();
}

VertexBuffer::~VertexBuffer()
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12buffer)
    delete m_vertexBufferView;
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
    if (m_dx12device)
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

        /// Create the view for vertex buffer
        {
            m_vertexBufferView->BufferLocation = m_dx12buffer->GetGPUVirtualAddress(); // Where is the buffer located in GPU memory
            m_vertexBufferView->SizeInBytes    = static_cast<UINT>(m_size);
            m_vertexBufferView->StrideInBytes  = m_stride; // the distance between vertices
        }
    }
}

void VertexBuffer::Resize(unsigned int size)
{
    DX_SAFE_RELEASE(m_buffer)
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
