#include "RenderTarget.hpp"

RenderTarget::~RenderTarget()
{
    Release();
}

void RenderTarget::Release()
{
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }
    if (rtv)
    {
        rtv->Release();
        rtv = nullptr;
    }
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}
