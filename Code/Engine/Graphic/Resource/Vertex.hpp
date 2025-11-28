/**
 * @file Vertex.hpp
 * @brief Graphic 模块标准顶点定义
 * @date 2025-10-03
 *
 * 职责:
 * - 提供 Graphic 模块统一的顶点格式
 * - 基于 Engine/Core 的 Vertex_PCUTBN
 * - 用于固定 Input Layout 设计
 *
 * 教学要点:
 * - typedef 简化命名
 * - 跨模块类型复用
 * - 符合 KISS 原则 - 不创建新类型，直接复用
 */

#pragma once

#include "Engine/Core/Vertex_PCU.hpp"

namespace enigma::graphic
{
    /**
     * @brief Graphic 模块标准顶点格式
     *
     * 教学要点:
     * - 使用 Engine/Core 的 Vertex_PCUTBN (Position, Color, UV, Tangent, Bitangent, Normal)
     * - 这是我们的全局固定 Input Layout
     * - 所有着色器都使用这个统一的顶点格式
     * - 避免每个着色器都有不同的输入布局
     *
     * 顶点布局:
     * - Vec3 position  (12 bytes)
     * - Rgba8 color    (4 bytes)
     * - Vec2 uvTexCoords (8 bytes)
     * - Vec3 tangent   (12 bytes)
     * - Vec3 bitangent (12 bytes)
     * - Vec3 normal    (12 bytes)
     * 总计: 60 bytes
     */
    using Vertex = ::Vertex_PCUTBN;
} // namespace enigma::graphic
