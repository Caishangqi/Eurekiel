#include "D12RenderTarget.hpp"
using namespace enigma::graphic;

BindlessResourceType D12RenderTarget::GetDefaultBindlessResourceType() const
{
    // RenderTarget作为可采样纹理资源使用Texture2D类型
    // 对应Iris中将RenderTarget主纹理绑定为GL_TEXTURE_2D供着色器采样
    return BindlessResourceType::Texture2D;
}