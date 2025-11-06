#pragma once

#include <memory>

namespace enigma::graphic
{
    // 前向声明
    class D12VertexBuffer;
    class D12IndexBuffer;
}

/**
 * @class RendererHelper
 * @brief 渲染器辅助工具类 - 提供Buffer管理等通用功能
 */
class RendererHelper
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
