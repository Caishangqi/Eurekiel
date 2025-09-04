#include "ConstantBuffer.hpp"

#include <d3d12.h>

#include "Renderer.hpp"

ConstantBuffer::ConstantBuffer(size_t size) : m_size(size)
{
}

ConstantBuffer::~ConstantBuffer()
{
    DX_SAFE_RELEASE(m_buffer)
    DX_SAFE_RELEASE(m_dx12ConstantBuffer)
}
