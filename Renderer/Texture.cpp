#include "Texture.hpp"

#include "Renderer.hpp"

UINT Texture::s_internalID = 0;

Texture::Texture()
{
}

Texture::~Texture()
{
    DX_SAFE_RELEASE(m_texture)
    DX_SAFE_RELEASE(m_shaderResourceView)
    DX_SAFE_RELEASE(m_textureBufferUploadHeap)
    DX_SAFE_RELEASE(m_dx12Texture)
}

Vec2 Texture::GetStandardDimensions() const
{
    float aspectRatio = static_cast<float>(m_dimensions.x) / static_cast<float>(m_dimensions.y);

    Vec2 standardizedDimension;
    if (m_dimensions.x >= m_dimensions.y)
    {
        // If width is greater or equal to height, set width to 1 and scale height accordingly
        standardizedDimension.x = 1.0f;
        standardizedDimension.y = 1.0f / aspectRatio; // dimension.y / dimension.x
    }
    else
    {
        // If height is greater than width, set height to 1 and scale width accordingly
        standardizedDimension.y = 1.0f;
        standardizedDimension.x = aspectRatio; // dimension.x / dimension.y
    }
    return standardizedDimension;
}
