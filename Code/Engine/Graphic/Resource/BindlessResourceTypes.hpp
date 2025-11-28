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
        Texture2D,              // 2D纹理资源 (SRV)
        Texture3D,              // 3D纹理资源 (SRV)
        TextureCube,            // 立方体纹理 (SRV)
        TextureArray,           // 纹理数组 (SRV)

        ConstantBuffer,         // 常量缓冲区 (CBV)
        StructuredBuffer,       // 结构化缓冲区 (SRV)
        RawBuffer,              // 原始缓冲区 (SRV)

        RWTexture2D,            // 可读写2D纹理 (UAV)
        RWTexture3D,            // 可读写3D纹理 (UAV)
        RWStructuredBuffer,     // 可读写结构化缓冲区 (UAV)
        RWRawBuffer,            // 可读写原始缓冲区 (UAV)

        Sampler,                // 采样器

        Count                   // 类型总数
    };
} // namespace enigma::graphic