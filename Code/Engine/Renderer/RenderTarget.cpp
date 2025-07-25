#include "RenderTarget.hpp"

RenderTarget::~RenderTarget()
{
    Release();
}

void RenderTarget::Release()
{
    if (rtv)
    {
        rtv->Release();
        rtv = nullptr;
    }
    delete texture;
    texture = nullptr;
}

IntVec2 RenderTarget::GetDimensions()
{
    return texture->GetDimensions();
}

ID3D11ShaderResourceView* RenderTarget::GetSRV() const
{
    return texture->GetShaderResourceView();
}
