#pragma once
#include "Engine/Math/IntVec2.hpp"
#include <d3d11.h>

#include "Texture.hpp"

/**
 * @class RenderTarget
 *
 * @brief Represents a rendering target in DirectX, comprising a texture, render target view,
 *        shader resource view, and dimensions.
 *
 * The RenderTarget class is used to manage resources required for rendering in a DirectX
 * environment. It encapsulates a texture, render target view (RTV), shader resource view (SRV),
 * and their associated dimensions. The class ensures proper cleanup of DirectX resources
 * when the object is destroyed or released.
 */
struct RenderTarget
{
public:
    ~RenderTarget();

    void Release();

    IntVec2                   GetDimensions();
    ID3D11ShaderResourceView* GetSRV() const;

public:
    Texture*                texture = nullptr;
    ID3D11RenderTargetView* rtv     = nullptr;
};
