#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic
{
    /**
     * @brief Iris兼容的世界渲染阶段枚举
     * 
     * 这个枚举直接对应Iris源码中的WorldRenderingPhase.java，定义了
     * 完整的渲染管线中的所有可能阶段。每个阶段对应特定的渲染任务
     * 和着色器程序类型。
     * 
     * 基于Iris源码的完整24个枚举值：
     * NONE, SKY, SUNSET, CUSTOM_SKY, SUN, MOON, STARS, VOID, TERRAIN_SOLID,
     * TERRAIN_CUTOUT_MIPPED, TERRAIN_CUTOUT, ENTITIES, BLOCK_ENTITIES, DESTROY,
     * OUTLINE, DEBUG, HAND_SOLID, TERRAIN_TRANSLUCENT, TRIPWIRE, PARTICLES,
     * CLOUDS, RAIN_SNOW, WORLD_BORDER, HAND_TRANSLUCENT
     * 
     * 教学目标：
     * - 理解现代渲染管线的阶段划分
     * - 学习Minecraft渲染的特殊需求
     * - 掌握枚举在状态机设计中的应用
     * 
     * Iris架构对应：
     * Java: public enum WorldRenderingPhase
     * C++:  enum class WorldRenderingPhase : uint32_t
     */
    enum class WorldRenderingPhase : uint32_t
    {
        /**
         * @brief 无效或未初始化状态
         * 管线的默认状态，表示当前没有进行任何渲染操作
         */
        NONE = 0,

        // ===========================================
        // 天空盒渲染阶段 (Sky Rendering Phases)
        // ===========================================

        /**
         * @brief 天空盒基础渲染
         * 渲染基础的天空盒，包括天空颜色和基本大气效果
         * 对应着色器：gbuffers_skybasic, gbuffers_skytextured
         */
        SKY,

        /**
         * @brief 日落/日出效果渲染
         * 专门处理太阳升起和落下时的特殊光照效果
         * 包括地平线的颜色渐变和大气散射
         */
        SUNSET,

        /**
         * @brief 自定义天空效果
         * 着色器包可以定义的自定义天空渲染效果
         * 允许完全重写天空的外观
         */
        CUSTOM_SKY,

        /**
         * @brief 太阳渲染
         * 专门渲染太阳的几何体和光晕效果
         * 通常使用简单的四边形加纹理实现
         */
        SUN,

        /**
         * @brief 月亮渲染
         * 渲染月亮的几何体，包括月相变化效果
         * 支持不同的月亮纹理和光照
         */
        MOON,

        /**
         * @brief 星空渲染
         * 渲染夜空中的星星点阵
         * 可以包括星座和动态的星空效果
         */
        STARS,

        /**
         * @brief 虚空渲染
         * 在Minecraft的虚空维度中使用的特殊渲染
         * 通常是纯黑或自定义的虚空效果
         * 注：C++中使用VOID_ENV避免与void关键字冲突
         */
        VOID_ENV,

        // ===========================================
        // 地形渲染阶段 (Terrain Rendering Phases)
        // ===========================================

        /**
         * @brief 不透明地形渲染
         * 渲染所有不透明的方块，如石头、泥土等
         * 对应着色器：gbuffers_basic, gbuffers_textured, gbuffers_terrain
         * 
         * 教学重点：这是G-Buffer填充的主要阶段
         */
        TERRAIN_SOLID,

        /**
         * @brief 带Mipmap的镂空地形渲染
         * 渲染带有透明部分但需要Mipmap的方块，如树叶
         * 支持距离相关的细节级别(LOD)
         */
        TERRAIN_CUTOUT_MIPPED,

        /**
         * @brief 镂空地形渲染
         * 渲染有透明部分的方块，如栅栏、花朵
         * 使用Alpha测试进行透明度处理
         */
        TERRAIN_CUTOUT,

        /**
         * @brief 半透明地形渲染
         * 渲染半透明方块，如水、冰块
         * 对应着色器：gbuffers_water, gbuffers_water_translucent
         * 
         * 教学价值：透明度排序和混合的挑战
         */
        TERRAIN_TRANSLUCENT,

        /**
         * @brief 绊线渲染
         * 专门渲染红石绊线等细小的透明物体
         * 需要特殊的深度和混合处理
         */
        TRIPWIRE,

        // ===========================================
        // 实体渲染阶段 (Entity Rendering Phases)
        // ===========================================

        /**
         * @brief 实体渲染
         * 渲染所有的生物和物体实体
         * 对应着色器：gbuffers_entities, gbuffers_entities_glowing
         * 
         * 教学重点：动态物体的渲染管理
         */
        ENTITIES,

        /**
         * @brief 方块实体渲染
         * 渲染箱子、熔炉等特殊方块实体
         * 这些方块有复杂的几何体和动画
         */
        BLOCK_ENTITIES,

        /**
         * @brief 破坏效果渲染
         * 渲染方块被破坏时的裂纹和碎片效果
         * 需要与地形混合的特殊处理
         */
        DESTROY,

        // ===========================================
        // 玩家手部渲染阶段 (Hand Rendering Phases)
        // * ===========================================

        /**
         * @brief 手部不透明物体渲染
         * 渲染玩家手中的不透明物品
         * 对应着色器：gbuffers_hand
         * 需要特殊的深度处理以正确显示在前景
         */
        HAND_SOLID,

        /**
         * @brief 手部半透明物体渲染
         * 渲染玩家手中的半透明物品，如药水瓶
         * 对应着色器：gbuffers_hand_water, gbuffers_hand_water_translucent
         */
        HAND_TRANSLUCENT,

        // ===========================================
        // 特效和调试渲染阶段 (Effects & Debug Phases)
        // ===========================================

        /**
         * @brief 选中物体轮廓渲染
         * 渲染玩家指向的方块或实体的选择框
         * 通常使用线框渲染模式
         */
        OUTLINE,

        /**
         * @brief 调试信息渲染
         * 渲染各种调试信息，如碰撞箱、光照调试等
         * 主要用于开发和调试阶段
         */
        DEBUG,

        /**
         * @brief 粒子效果渲染
         * 渲染所有的粒子效果，如烟雾、火花、魔法效果
         * 需要大量的实例化渲染和Alpha混合
         * 
         * 教学价值：大规模粒子系统的渲染优化
         */
        PARTICLES,

        /**
         * @brief 云层渲染
         * 渲染天空中的云朵效果
         * 对应着色器：gbuffers_clouds
         * 可以是2D或3D体积云
         */
        CLOUDS,

        /**
         * @brief 雨雪天气效果渲染
         * 渲染降水效果，包括雨滴和雪花
         * 需要与地形交互的复杂效果处理
         */
        RAIN_SNOW,

        /**
         * @brief 世界边界渲染
         * 渲染世界边界的视觉效果
         * 通常是半透明的屏障效果
         */
        WORLD_BORDER,

        // ===========================================
        // 计数和验证
        // ===========================================

        /**
         * @brief 枚举值总数
         * 用于数组大小分配和循环边界检查
         * 必须始终是最后一个值
         */
        COUNT
    };

    // ===========================================
    // 内联实现
    // ===========================================

    /**
     * @brief 内联实现：将枚举值转换为字符串
     */
    inline const char* ToString(WorldRenderingPhase phase)
    {
        switch (phase)
        {
            case WorldRenderingPhase::NONE:                return "NONE";
            case WorldRenderingPhase::SKY:                 return "SKY";
            case WorldRenderingPhase::SUNSET:              return "SUNSET";
            case WorldRenderingPhase::CUSTOM_SKY:          return "CUSTOM_SKY";
            case WorldRenderingPhase::SUN:                 return "SUN";
            case WorldRenderingPhase::MOON:                return "MOON";
            case WorldRenderingPhase::STARS:               return "STARS";
            case WorldRenderingPhase::VOID_ENV:            return "VOID_ENV";
            case WorldRenderingPhase::TERRAIN_SOLID:       return "TERRAIN_SOLID";
            case WorldRenderingPhase::TERRAIN_CUTOUT_MIPPED: return "TERRAIN_CUTOUT_MIPPED";
            case WorldRenderingPhase::TERRAIN_CUTOUT:      return "TERRAIN_CUTOUT";
            case WorldRenderingPhase::TERRAIN_TRANSLUCENT: return "TERRAIN_TRANSLUCENT";
            case WorldRenderingPhase::TRIPWIRE:            return "TRIPWIRE";
            case WorldRenderingPhase::ENTITIES:            return "ENTITIES";
            case WorldRenderingPhase::BLOCK_ENTITIES:      return "BLOCK_ENTITIES";
            case WorldRenderingPhase::DESTROY:             return "DESTROY";
            case WorldRenderingPhase::HAND_SOLID:          return "HAND_SOLID";
            case WorldRenderingPhase::HAND_TRANSLUCENT:    return "HAND_TRANSLUCENT";
            case WorldRenderingPhase::OUTLINE:             return "OUTLINE";
            case WorldRenderingPhase::DEBUG:               return "DEBUG";
            case WorldRenderingPhase::PARTICLES:           return "PARTICLES";
            case WorldRenderingPhase::CLOUDS:              return "CLOUDS";
            case WorldRenderingPhase::RAIN_SNOW:           return "RAIN_SNOW";
            case WorldRenderingPhase::WORLD_BORDER:        return "WORLD_BORDER";
            case WorldRenderingPhase::COUNT:               return "COUNT";
            default:                                        return "UNKNOWN";
        }
    }

    /**
     * @brief 内联实现：从字符串解析枚举值
     */
    inline WorldRenderingPhase FromString(const std::string& str)
    {
        if (str == "NONE")                return WorldRenderingPhase::NONE;
        if (str == "SKY")                 return WorldRenderingPhase::SKY;
        if (str == "SUNSET")              return WorldRenderingPhase::SUNSET;
        if (str == "CUSTOM_SKY")          return WorldRenderingPhase::CUSTOM_SKY;
        if (str == "SUN")                 return WorldRenderingPhase::SUN;
        if (str == "MOON")                return WorldRenderingPhase::MOON;
        if (str == "STARS")               return WorldRenderingPhase::STARS;
        if (str == "VOID_ENV")            return WorldRenderingPhase::VOID_ENV;
        if (str == "TERRAIN_SOLID")       return WorldRenderingPhase::TERRAIN_SOLID;
        if (str == "TERRAIN_CUTOUT_MIPPED") return WorldRenderingPhase::TERRAIN_CUTOUT_MIPPED;
        if (str == "TERRAIN_CUTOUT")      return WorldRenderingPhase::TERRAIN_CUTOUT;
        if (str == "TERRAIN_TRANSLUCENT") return WorldRenderingPhase::TERRAIN_TRANSLUCENT;
        if (str == "TRIPWIRE")            return WorldRenderingPhase::TRIPWIRE;
        if (str == "ENTITIES")            return WorldRenderingPhase::ENTITIES;
        if (str == "BLOCK_ENTITIES")      return WorldRenderingPhase::BLOCK_ENTITIES;
        if (str == "DESTROY")             return WorldRenderingPhase::DESTROY;
        if (str == "HAND_SOLID")          return WorldRenderingPhase::HAND_SOLID;
        if (str == "HAND_TRANSLUCENT")    return WorldRenderingPhase::HAND_TRANSLUCENT;
        if (str == "OUTLINE")             return WorldRenderingPhase::OUTLINE;
        if (str == "DEBUG")               return WorldRenderingPhase::DEBUG;
        if (str == "PARTICLES")           return WorldRenderingPhase::PARTICLES;
        if (str == "CLOUDS")              return WorldRenderingPhase::CLOUDS;
        if (str == "RAIN_SNOW")           return WorldRenderingPhase::RAIN_SNOW;
        if (str == "WORLD_BORDER")        return WorldRenderingPhase::WORLD_BORDER;

        return WorldRenderingPhase::NONE; // 默认值
    }

    /**
     * @brief 内联实现：检查阶段是否为地形渲染相关
     */
    inline bool IsTerrainPhase(WorldRenderingPhase phase)
    {
        switch (phase)
        {
            case WorldRenderingPhase::TERRAIN_SOLID:
            case WorldRenderingPhase::TERRAIN_CUTOUT_MIPPED:
            case WorldRenderingPhase::TERRAIN_CUTOUT:
            case WorldRenderingPhase::TERRAIN_TRANSLUCENT:
                return true;
            default:
                return false;
        }
    }

    /**
     * @brief 内联实现：检查阶段是否需要透明度处理
     */
    inline bool RequiresTransparency(WorldRenderingPhase phase)
    {
        switch (phase)
        {
            case WorldRenderingPhase::TERRAIN_TRANSLUCENT:
            case WorldRenderingPhase::HAND_TRANSLUCENT:
            case WorldRenderingPhase::PARTICLES:
            case WorldRenderingPhase::CLOUDS:
            case WorldRenderingPhase::RAIN_SNOW:
            case WorldRenderingPhase::WORLD_BORDER:
                return true;
            default:
                return false;
        }
    }

    /**
     * @brief 内联实现：获取阶段的默认深度测试模式
     */
    inline const char* GetDefaultDepthMode(WorldRenderingPhase phase)
    {
        switch (phase)
        {
            case WorldRenderingPhase::SKY:
            case WorldRenderingPhase::SUNSET:
            case WorldRenderingPhase::CUSTOM_SKY:
                return "LEQUAL"; // 天空盒使用LEQUAL

            case WorldRenderingPhase::TERRAIN_SOLID:
            case WorldRenderingPhase::TERRAIN_CUTOUT_MIPPED:
            case WorldRenderingPhase::TERRAIN_CUTOUT:
            case WorldRenderingPhase::ENTITIES:
            case WorldRenderingPhase::BLOCK_ENTITIES:
                return "LESS"; // 不透明物体使用标准深度测试

            case WorldRenderingPhase::TERRAIN_TRANSLUCENT:
            case WorldRenderingPhase::HAND_TRANSLUCENT:
            case WorldRenderingPhase::PARTICLES:
                return "LEQUAL"; // 透明物体使用LEQUAL以正确处理排序

            case WorldRenderingPhase::DEBUG:
            case WorldRenderingPhase::OUTLINE:
                return "ALWAYS"; // 调试信息总是可见

            default:
                return "LESS"; // 默认深度测试
        }
    }
} // namespace enigma::graphic
