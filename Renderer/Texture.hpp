#pragma once
#include <d3d11.h>
#include <string>

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"


class Texture
{
    friend class Renderer; // Only the Renderer can create new Texture objects!

    Texture(); // can't instantiate directly; must ask Renderer to do it for you
    Texture(const Texture& copy) = delete; // No copying allowed!  This represents GPU memory.
    ~Texture();

public:
    IntVec2            GetDimensions() const { return m_dimensions; }
    Vec2               GetStandardDimensions() const;
    const std::string& GetImageFilePath() const { return m_name; }

protected:
    std::string m_name;
    IntVec2     m_dimensions;

    /// OpenGL
    // #ToDo in SD2: Use #if defined( ENGINE_RENDER_D3D11 ) to do something different for DX11; #else do:
    unsigned int m_openglTextureID = 0xFFFFFFFF;
    ///

    /// DirectX
    ID3D11Texture2D*          m_texture            = nullptr;
    ID3D11ShaderResourceView* m_shaderResourceView = nullptr;
    ///
};
