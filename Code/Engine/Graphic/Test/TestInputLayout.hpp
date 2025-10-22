/**
 * @file TestInputLayout.hpp
 * @brief Vertex_PCUTBN的D3D12 Input Layout定义
 * @date 2025-10-21
 *
 * TODO(M3): Replace with ShaderProgram system
 * 这是临时实现，正式实现应该由PipelineManager自动生成Input Layout
 *
 * 教学要点:
 * - D3D12_INPUT_ELEMENT_DESC数组定义顶点格式
 * - AlignedByteOffset必须与C++结构体内存布局完全匹配
 * - SemanticName必须与HLSL输入结构体匹配
 *
 * Vertex_PCUTBN内存布局（总60字节）:
 * - Vec3  m_position   (12 bytes, offset 0)
 * - Rgba8 m_color      (4 bytes,  offset 12)
 * - Vec2  m_uvTexCoords (8 bytes,  offset 16)
 * - Vec3  m_tangent    (12 bytes, offset 24)
 * - Vec3  m_bitangent  (12 bytes, offset 36)
 * - Vec3  m_normal     (12 bytes, offset 48)
 */

#pragma once

#include <d3d12.h>
#include <iterator> // for std::size (C++17)

namespace enigma::graphic::test
{
    /**
     * @brief 获取Vertex_PCUTBN的Input Layout定义
     *
     * @param outElementCount [out] 输出元素数量（6个）
     * @return 静态Input Layout数组指针
     *
     * 教学要点:
     * - 静态数组确保生命周期，避免悬空指针
     * - SemanticName字符串必须是静态字面量
     * - Format必须与HLSL类型匹配:
     *   - Vec3 -> DXGI_FORMAT_R32G32B32_FLOAT
     *   - Rgba8 -> DXGI_FORMAT_R8G8B8A8_UNORM (归一化到[0,1])
     *   - Vec2 -> DXGI_FORMAT_R32G32_FLOAT
     */
    inline const D3D12_INPUT_ELEMENT_DESC* GetVertex_PCUTBN_InputLayout(UINT& outElementCount)
    {
        // 静态数组确保生命周期
        static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            // Element 0: Position (Vec3, 12 bytes, offset 0)
            {
                "POSITION", // SemanticName
                0, // SemanticIndex
                DXGI_FORMAT_R32G32B32_FLOAT, // Format (3x float)
                0, // InputSlot (VertexBuffer槽位)
                0, // AlignedByteOffset
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, // InputSlotClass
                0 // InstanceDataStepRate
            },

            // Element 1: Color (Rgba8, 4 bytes, offset 12)
            {
                "COLOR", // SemanticName
                0, // SemanticIndex
                DXGI_FORMAT_R8G8B8A8_UNORM, // Format (4x uint8 归一化到[0,1])
                0, // InputSlot
                12, // AlignedByteOffset (sizeof(Vec3) = 12)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },

            // Element 2: UV (Vec2, 8 bytes, offset 16)
            {
                "TEXCOORD", // SemanticName
                0, // SemanticIndex (TEXCOORD0)
                DXGI_FORMAT_R32G32_FLOAT, // Format (2x float)
                0, // InputSlot
                16, // AlignedByteOffset (12 + 4 = 16)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },

            // Element 3: Tangent (Vec3, 12 bytes, offset 24)
            {
                "TANGENT", // SemanticName
                0, // SemanticIndex
                DXGI_FORMAT_R32G32B32_FLOAT, // Format (3x float)
                0, // InputSlot
                24, // AlignedByteOffset (16 + 8 = 24)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },

            // Element 4: Bitangent (Vec3, 12 bytes, offset 36)
            {
                "BINORMAL", // SemanticName (DirectX传统命名)
                0, // SemanticIndex
                DXGI_FORMAT_R32G32B32_FLOAT, // Format (3x float)
                0, // InputSlot
                36, // AlignedByteOffset (24 + 12 = 36)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },

            // Element 5: Normal (Vec3, 12 bytes, offset 48)
            {
                "NORMAL", // SemanticName
                0, // SemanticIndex
                DXGI_FORMAT_R32G32B32_FLOAT, // Format (3x float)
                0, // InputSlot
                48, // AlignedByteOffset (36 + 12 = 48)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        // 输出元素数量
        outElementCount = static_cast<UINT>(std::size(inputLayout));

        return inputLayout;
    }
} // namespace enigma::graphic::test
