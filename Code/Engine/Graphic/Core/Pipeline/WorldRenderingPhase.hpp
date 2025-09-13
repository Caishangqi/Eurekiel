#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic {

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
    enum class WorldRenderingPhase : uint32_t {
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
         */
        VOID,

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
    // 辅助函数声明
    // ===========================================

    /**
     * @brief 将枚举值转换为字符串
     * 
     * 用于调试输出和日志记录，返回枚举值对应的
     * 人类可读的字符串表示。
     * 
     * @param phase 渲染阶段枚举值
     * @return 阶段名称字符串
     * 
     * 教学价值：枚举反射和调试友好的设计
     */
    const char* ToString(WorldRenderingPhase phase);

    /**
     * @brief 从字符串解析枚举值
     * 
     * 用于配置文件加载和命令行参数解析，
     * 将字符串转换回对应的枚举值。
     * 
     * @param str 阶段名称字符串
     * @return 对应的枚举值，无效时返回NONE
     */
    WorldRenderingPhase FromString(const std::string& str);

    /**
     * @brief 检查阶段是否为地形渲染相关
     * 
     * 判断给定的渲染阶段是否涉及地形渲染，
     * 用于优化和特殊处理逻辑。
     * 
     * @param phase 渲染阶段
     * @return true如果是地形相关阶段
     */
    bool IsTerrainPhase(WorldRenderingPhase phase);

    /**
     * @brief 检查阶段是否需要透明度处理
     * 
     * 判断给定的渲染阶段是否需要Alpha混合
     * 或其他透明度相关的特殊处理。
     * 
     * @param phase 渲染阶段
     * @return true如果需要透明度处理
     * 
     * 教学重点：透明度渲染的复杂性
     */
    bool RequiresTransparency(WorldRenderingPhase phase);

    /**
     * @brief 获取阶段的默认深度测试模式
     * 
     * 返回特定渲染阶段建议使用的深度测试配置，
     * 用于自动化的渲染状态管理。
     * 
     * @param phase 渲染阶段
     * @return 建议的深度测试模式字符串
     */
    const char* GetDefaultDepthMode(WorldRenderingPhase phase);

} // namespace enigma::graphic