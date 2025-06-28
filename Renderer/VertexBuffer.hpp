#pragma once
#include <cstdint>
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

    /// DirectX12
    void ResetCursor();
    bool Allocate(const void* scr, size_t size);

private:
    ID3D11Device* m_device = nullptr;
    ID3D11Buffer* m_buffer = nullptr;

    unsigned int m_size   = 0;
    unsigned int m_stride = 0;

    uint8_t*                 m_cpuPtr         = nullptr;    // The CPU address handler that point to the buffer
    uint64_t                 m_baseGpuAddress = 0;  // The base GPU address that set when created
    ///
    /// The Cursor for this vertex buffer
    /// Needed when we want to perform multiple draw in between one frame (Begin to End).
    /// When we call @see DX12Renderer::DrawVertexArray \n 
    size_t                   m_cursor         = 0;
    ID3D12Device*            m_dx12device     = nullptr;
    ID3D12Resource*          m_dx12buffer     = nullptr; // DX12
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView; // DX12 Create the vertex Buffer view, pure cpu sides unlike descriptor heap
};
