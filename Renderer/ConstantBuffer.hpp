#pragma once

struct ID3D11Buffer;
struct ID3D12Resource;
struct D3D12_CONSTANT_BUFFER_VIEW_DESC;

class ConstantBuffer
{
    friend class Renderer;
    friend class DX12Renderer;

public:
    ConstantBuffer(size_t size);
    ConstantBuffer(const ConstantBuffer& copy) = delete;
    virtual ~ConstantBuffer();

private:
    ID3D11Buffer* m_buffer = nullptr;
    size_t        m_size   = 0;

    ID3D12Resource*                  m_dx12ConstantBuffer = nullptr;
    D3D12_CONSTANT_BUFFER_VIEW_DESC* m_constantBufferView = nullptr;
};
