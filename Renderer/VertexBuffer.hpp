#pragma once
#include <d3d11.h>
#include <d3d12.h>

struct ID3D11Device;

class VertexBuffer
{
    friend class Renderer;
    friend class DX12Renderer;
public:
    VertexBuffer(ID3D11Device* device, unsigned int size, unsigned int stride);
    VertexBuffer(ID3D12Device* device, unsigned int size, unsigned int stride);
    VertexBuffer(const VertexBuffer& copy) = delete;
    virtual ~VertexBuffer();

    void Create();
    
    void Resize(unsigned int size);

    unsigned int GetSize();
    unsigned int GetStride();

private:
    ID3D11Device* m_device = nullptr;
    ID3D11Buffer* m_buffer = nullptr;
    unsigned int  m_size   = 0;
    unsigned int  m_stride = 0;

    ID3D12Device*             m_dx12device       = nullptr;
    ID3D12Resource*           m_dx12buffer = nullptr; // DX12
    D3D12_VERTEX_BUFFER_VIEW* m_vertexBufferView = nullptr; // DX12 Create the vertex Buffer view, pure cpu sides unlike descriptor heap
};
