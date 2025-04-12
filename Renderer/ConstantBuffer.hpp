#pragma once

struct ID3D11Buffer;

class ConstantBuffer
{
    friend class Renderer;

public:
    ConstantBuffer(size_t size);
    ConstantBuffer(const ConstantBuffer& copy) = delete;
    virtual ~ConstantBuffer();

private:
    ID3D11Buffer* m_buffer = nullptr;
    size_t        m_size   = 0;
};
