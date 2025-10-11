#pragma once

#include "Engine/Math/Vec3.hpp"

namespace enigma::graphic
{
    /**
     * @brief ID Uniforms - 方块/实体/物品ID数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/id/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "ID" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过idBufferIndex访问
     * 3. 使用alignas确保与HLSL对齐要求一致
     * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<IDUniforms> idBuffer =
     *     ResourceDescriptorHeap[idBufferIndex];
     * int entityId = idBuffer[0].entityId;
     * int heldItem = idBuffer[0].heldItemId;
     * ```
     *
     * @note alignas(4)用于int确保4字节对齐（HLSL标准对齐）
     * @note alignas(16)用于vec3确保16字节对齐
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct IDUniforms
    {
        /**
         * @brief 当前实体ID
         * @type int
         * @iris entityId
         *
         * 教学要点:
         * - 从entity.properties读取的实体ID
         * - 值为0表示没有entity.properties文件
         * - 值为65535表示实体不在entity.properties中
         */
        alignas(4) int entityId;

        /**
         * @brief 当前方块实体ID（Tile Entity）
         * @type int
         * @iris blockEntityId
         *
         * 教学要点:
         * - 从block.properties读取的方块实体ID
         * - 值为0表示没有block.properties文件
         * - 值为65535表示方块实体不在block.properties中
         */
        alignas(4) int blockEntityId;

        /**
         * @brief 当前渲染的物品ID
         * @type int
         * @iris currentRenderedItemId
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 基于item.properties检测当前渲染的物品/护甲
         * - 类似heldItemId，但应用于当前渲染的物品而非手持物品
         * - 可检测armor trims（如trim_emerald）
         */
        alignas(4) int currentRenderedItemId;

        /**
         * @brief 玩家选中的方块ID
         * @type int
         * @iris currentSelectedBlockId
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 从block.properties读取玩家准星指向方块的ID
         * - 值为0表示未选中方块或没有block.properties文件
         */
        alignas(4) int currentSelectedBlockId;

        /**
         * @brief 玩家选中方块的位置（玩家空间）
         * @type vec3
         * @iris currentSelectedBlockPos
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 相对于玩家的方块中心坐标
         * - 如果未选中方块，所有分量为-256.0
         */
        alignas(16) Vec3 currentSelectedBlockPos;

        /**
         * @brief 主手物品ID
         * @type int
         * @iris heldItemId
         *
         * 教学要点:
         * - 从item.properties读取主手物品的ID
         * - 值为0表示没有item.properties文件
         * - 值为-1表示手持物品不在item.properties中（包括空手）
         */
        alignas(4) int heldItemId;

        /**
         * @brief 副手物品ID
         * @type int
         * @iris heldItemId2
         *
         * 教学要点:
         * - 从item.properties读取副手物品的ID
         * - 值为0表示没有item.properties文件
         * - 值为-1表示手持物品不在item.properties中（包括空手）
         */
        alignas(4) int heldItemId2;

        /**
         * @brief 主手物品的光照强度
         * @type int
         * @iris heldBlockLightValue
         * @range [0,15]
         *
         * 教学要点:
         * - 主手物品的发光等级（如火把=14）
         * - 原版方块范围0-15，某些模组方块可能更高
         * - 注意：如果oldHandLight未设置为false，此值取双手中光照最高的
         */
        alignas(4) int heldBlockLightValue;

        /**
         * @brief 副手物品的光照强度
         * @type int
         * @iris heldBlockLightValue2
         * @range [0,15]
         *
         * 教学要点:
         * - 副手物品的发光等级
         * - 原版方块范围0-15，某些模组方块可能更高
         */
        alignas(4) int heldBlockLightValue2;

        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        IDUniforms()
            : entityId(0)
              , blockEntityId(0)
              , currentRenderedItemId(0)
              , currentSelectedBlockId(0)
              , currentSelectedBlockPos(Vec3(-256.0f, -256.0f, -256.0f)) // 未选中标志
              , heldItemId(-1) // -1表示空手
              , heldItemId2(-1) // -1表示空手
              , heldBlockLightValue(0)
              , heldBlockLightValue2(0)
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过256字节）
    static_assert(sizeof(IDUniforms) <= 256,
                  "IDUniforms too large, consider optimization");
} // namespace enigma::graphic
