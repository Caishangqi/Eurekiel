#pragma once

#include <memory>

#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UpdateFrequency.hpp"

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
using namespace enigma::graphic;

class BufferHelper
{
public:
    // [NEW] ConstantBuffer对齐和slot管理常量
    static constexpr size_t   CONSTANT_BUFFER_ALIGNMENT = 256; // D3D12 ConstantBuffer必须256字节对齐
    static constexpr uint32_t MAX_ENGINE_RESERVED_SLOT  = 14; // 引擎保留slot范围：0-14

    // [NEW] ConstantBuffer辅助方法
    /**
     * @brief 计算256字节对齐后的大小
     * @param rawSize 原始大小
     * @return 对齐后的大小（向上对齐到256字节边界）
     */
    static size_t CalculateAlignedSize(size_t rawSize);

    /**
     * @brief 计算给定总大小可以容纳多少个元素
     * @param totalSize 总Buffer大小
     * @param elementSize 单个元素大小
     * @return 可容纳的元素数量
     */
    static size_t CalculateBufferCount(size_t totalSize, size_t elementSize);

    /**
     * @brief 检查slot是否为引擎保留slot（0-14）
     * @param slot 要检查的slot编号
     * @return true表示为引擎保留slot
     */
    static bool IsEngineReservedSlot(uint32_t slot);

    /**
     * @brief 检查slot是否为用户自定义slot（>=15）
     * @param slot 要检查的slot编号
     * @return true表示为用户自定义slot
     */
    static bool IsUserSlot(uint32_t slot);

    /**
     * @brief 计算ConstantBuffer的GPU虚拟地址
     * @param resource D3D12资源指针
     * @param offset 偏移量（字节）
     * @return GPU虚拟地址
     */
    static D3D12_GPU_VIRTUAL_ADDRESS CalculateRootCBVAddress(ID3D12Resource* resource, size_t offset);

    /**
     * @brief 复制Buffer数据（内存安全版本）
     * @param dest 目标指针
     * @param src 源指针
     * @param size 复制大小（字节）
     */
    static void CopyBufferData(void* dest, const void* src, size_t size);

    /**
     * @brief 验证Buffer配置合法性
     * @param slotId Slot编号
     * @param size Buffer大小
     * @param freq 更新频率
     * @return 验证通过返回true
     */
    static bool ValidateBufferConfig(uint32_t slotId, size_t size, UpdateFrequency freq);

    /**
     * @brief 验证Slot范围合法性
     * @param slotId Slot编号
     * @return 验证通过返回true
     */
    static bool ValidateSlotRange(uint32_t slotId);

    /**
     * @brief 验证Buffer大小合法性
     * @param size Buffer大小
     * @return 验证通过返回true
     */
    static bool ValidateBufferSize(size_t size);

    // [EXISTING] VertexBuffer/IndexBuffer管理方法
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
