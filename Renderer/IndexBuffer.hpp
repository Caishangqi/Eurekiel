#pragma once
#include "VertexBuffer.hpp"

class IndexBuffer
{
    friend class Renderer;
    friend class DX12Renderer;

public:
    IndexBuffer(ID3D11Device* device, unsigned int size);
    IndexBuffer(ID3D12Device* device, unsigned int size);
    IndexBuffer(const IndexBuffer& copy) = delete;
    virtual ~IndexBuffer();

    void Create();
    /// Deprecated, should not use in DirectX12 API
    /// @param indices 
    /// @param count 
    /// @param deviceContext
    void Update(const unsigned int* indices, unsigned int count, ID3D11DeviceContext* deviceContext);
    [[deprecated("Update method in DX12Renderer is deprecated, which will cause buffer overwrite")]]
    void Update(const unsigned int* indices, unsigned int count, ID3D12Device* deviceContext = nullptr);
    void Resize(unsigned int size);

    /// DirectX12
    void ResetCursor();
    bool Allocate(const void* src, size_t size);

    unsigned int GetSize();
    unsigned int GetStride();
    unsigned int GetCount();

private:
    ID3D11Buffer* m_buffer;
    ID3D11Device* m_device = nullptr;

    ID3D12Device*           m_dx12device = nullptr;
    ID3D12Resource*         m_dx12buffer = nullptr; // DX12
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    unsigned int            m_size = 0;

    /// DirectX12
    uint8_t* m_cpuPtr         = nullptr;
    uint64_t m_baseGpuAddress = 0;
    size_t   m_cursor         = 0;
};
