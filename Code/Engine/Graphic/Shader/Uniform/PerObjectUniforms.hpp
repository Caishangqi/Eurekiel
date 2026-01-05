#pragma once
#include "Engine/Math/Mat44.hpp"

namespace enigma::graphic
{
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct PerObjectUniforms
    {
        alignas(16) Mat44 modelMatrix;
        alignas(16) Mat44 modelMatrixInverse;
        alignas(16) float modelColor[4]; // 16字节对齐，匹配HLSL float4
        uint8_t           _padding[12]; // 填充12字节（4 + 12 = 16）
    };
}
