#pragma once
#include "VertexBuffer.hpp"

class IndexBuffer
{
    friend class Renderer;

public:
    IndexBuffer(ID3D11Device* device, unsigned int size);
    IndexBuffer(const IndexBuffer& copy) = delete;
    virtual ~IndexBuffer();

    void Create();
    /// Deprecated
    /// @param indices 
    /// @param count 
    /// @param deviceContext 
    void Update(const unsigned int* indices, unsigned int count, ID3D11DeviceContext* deviceContext);
    void Resize(unsigned int size);

    unsigned int GetSize();
    unsigned int GetStride();
    unsigned int GetCount();

private:
    ID3D11Buffer* m_buffer;
    ID3D11Device* m_device = nullptr;
    unsigned int  m_size   = 0;
};
