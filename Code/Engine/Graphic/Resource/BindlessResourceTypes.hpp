#pragma once

namespace enigma::graphic
{
    /**
     * @brief Bindless资源类型枚举
     *
     * 教学要点: 不同类型的资源需要不同的描述符视图和绑定方式
     */
    enum class BindlessResourceType
    {
        Texture2D,       // 2D纹理资源 (SRV)
        Texture3D,       // 3D纹理资源 (SRV)
        TextureCube,     // 立方体纹理 (SRV)
        Buffer,          // 结构化缓冲区 (SRV)
        ConstantBuffer,  // 常量缓冲区 (CBV)
        RWTexture2D,     // 可读写2D纹理 (UAV)
        RWBuffer         // 可读写缓冲区 (UAV)
    };
} // namespace enigma::graphic