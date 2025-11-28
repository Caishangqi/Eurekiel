#include "D12Framebuffer.hpp"

using namespace enigma::graphic;

BindlessResourceType D12Framebuffer::GetDefaultBindlessResourceType() const
{
    // Framebuffer作为渲染管线组件，其附件纹理使用Texture2D类型
    // 保持与纹理系统的一致性，便于后续着色器访问
    return BindlessResourceType::Texture2D;
}
