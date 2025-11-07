#pragma once

#include <memory>

namespace enigma::graphic
{
    // 前向声明
    class D12VertexBuffer;
    class D12IndexBuffer;
}

/**
 * @class BufferHelper
 * @brief Buffer管理辅助工具类 - 提供Buffer延迟创建和动态扩容功能
 * 
 * [REFACTOR] 2025-01-06: 从RendererHelper重命名为BufferHelper
 * - 职责单一化：专注于Buffer管理
 * - 位置优化：移动到Graphic/Resource/Buffer/目录
 * - 架构合规：符合资源管理层架构设计
 */
class BufferHelper
{
public:
    /**
     * @brief 确保VertexBuffer大小足够（不足时扩容）
     * @param buffer 要检查的buffer智能指针（引用传递，可能被重新创建）
     * @param requiredSize 需要的最小大小
     * @param minSize 初始创建时的最小大小
     * @param stride 顶点步长
     * @param debugName 调试名称
     */
    static void EnsureBufferSize(
        std::shared_ptr<enigma::graphic::D12VertexBuffer>& buffer,
        size_t                                             requiredSize,
        size_t                                             minSize,
        size_t                                             stride,
        const char*                                        debugName
    );

    /**
     * @brief 确保IndexBuffer大小足够（不足时扩容）
     * @param buffer 要检查的buffer智能指针（引用传递，可能被重新创建）
     * @param requiredSize 需要的最小大小
     * @param minSize 初始创建时的最小大小
     * @param debugName 调试名称
     */
    static void EnsureBufferSize(
        std::shared_ptr<enigma::graphic::D12IndexBuffer>& buffer,
        size_t                                            requiredSize,
        size_t                                            minSize,
        const char*                                       debugName
    );
};
