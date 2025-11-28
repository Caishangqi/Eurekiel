#pragma once

// 基于 Iris 源码: net.irisshaders.iris.uniforms.CommonUniforms.java
// 文档验证: 2025-09-14 通过真实源码确认存在
// 
// 教学要点:
// 1. 这个类直接对应 Iris 的 CommonUniforms 类
// 2. 管理所有通用的着色器uniform变量
// 3. 包含眼部亮度、玩家状态、环境效果等uniform

#include <memory>
#include <functional>

namespace enigma::graphic
{
    /**
     * @brief 通用Uniform管理器
     * 
     * 直接对应 Iris CommonUniforms.java 中定义的通用uniform系统
     * 负责管理所有shader pack的通用uniform变量，包括：
     * - 玩家状态 (潜行、冲刺、受伤等)
     * - 环境效果 (夜视、失明、黑暗等)
     * - 眼部亮度、天空颜色、雨强度等
     * - 渲染阶段状态 (renderStage)
     * 
     * TODO: 基于 Iris 源码实现完整的uniform管理逻辑
     * - 实现 addCommonUniforms() 方法 (对应 Iris 的同名方法)
     * - 实现 addDynamicUniforms() 方法 (动态uniform更新)
     * - 实现 generalCommonUniforms() 方法 (通用uniform设置)
     * - 集成到 UniformHolder 系统中
     */
    class CommonUniforms final
    {
    private:
        // 禁止实例化 - 静态工具类
        CommonUniforms()                                 = delete;
        ~CommonUniforms()                                = delete;
        CommonUniforms(const CommonUniforms&)            = delete;
        CommonUniforms& operator=(const CommonUniforms&) = delete;

    public:
        /**
         * @brief 添加所有通用uniform (对应Iris的addCommonUniforms)
         * @param uniforms Uniform持有器
         * @param idMap ID映射表 (方块/物品ID映射)
         * @param directives 包指令配置
         * @param updateNotifier 帧更新通知器
         * @param fogMode 雾效模式
         * 
         * TODO: 实现uniform注册逻辑，包括：
         * - 相机uniform (CameraUniforms)
         * - 矩阵uniform (MatrixUniforms) 
         * - 时间uniform (WorldTime, SystemTime)
         * - 生物群系uniform (BiomeUniforms)
         * - 天体uniform (CelestialUniforms)
         * - Iris专用uniform
         */
        static void AddCommonUniforms(/* UniformHolder& uniforms, 
                                       IdMap& idMap,
                                       PackDirectives& directives,
                                       FrameUpdateNotifier& updateNotifier,
                                       FogMode fogMode */);

        /**
         * @brief 添加动态uniform (对应Iris的addDynamicUniforms)
         * @param uniforms 动态Uniform持有器
         * @param fogMode 雾效模式
         * 
         * TODO: 实现动态uniform，包括：
         * - atlasSize (纹理图集尺寸)
         * - gtextureSize (当前纹理尺寸)
         * - blendFunc (混合函数状态)
         * - renderStage (当前渲染阶段)
         * - entityId (实体ID，用于fallback)
         */
        static void AddDynamicUniforms(/* DynamicUniformHolder& uniforms,
                                        FogMode fogMode */);

        /**
         * @brief 添加通用uniform变量 (对应Iris的generalCommonUniforms)
         * @param uniforms Uniform持有器
         * @param updateNotifier 帧更新通知器
         * @param directives 包指令配置
         * 
         * TODO: 实现玩家状态uniform，包括：
         * - hideGUI, isRightHanded (UI状态)
         * - isEyeInWater (眼部水中状态: 0=空气,1=水,2=岩浆,3=粉雪)
         * - blindness, darknessFactor, nightVision (视觉效果)
         * - is_sneaking, is_sprinting, is_hurt, is_invisible, is_burning, is_on_ground
         * - screenBrightness, eyeBrightness, rainStrength, wetness
         * - skyColor, playerMood, constantMood
         * - dhFarPlane, dhNearPlane, dhRenderDistance (Distant Horizons集成)
         */
        static void GeneralCommonUniforms(/* UniformHolder& uniforms,
                                           FrameUpdateNotifier& updateNotifier,
                                           PackDirectives& directives */);

        // ========================================================================
        // 状态查询方法 (对应Iris中的私有方法)
        // ========================================================================

        /**
         * @brief 获取失明效果强度 (对应Iris的getBlindness)
         * @return 失明强度 [0.0-1.0]
         * 
         * TODO: 实现基于MobEffects.BLINDNESS的计算
         */
        static float GetBlindness();

        /**
         * @brief 获取黑暗效果因子 (对应Iris的getDarknessFactor)
         * @return 黑暗因子 [0.0-1.0]
         * 
         * TODO: 实现基于MobEffects.DARKNESS的计算
         */
        static float GetDarknessFactor();

        /**
         * @brief 获取夜视效果强度 (对应Iris的getNightVision)
         * @return 夜视强度 [0.0-1.0]
         * 
         * TODO: 实现夜视效果计算，包括：
         * - 夜视药水效果
         * - 潮涌核心水下视觉
         * - 模组兼容 (如Origins模组)
         */
        static float GetNightVision();

        /**
         * @brief 获取雨强度 (对应Iris的getRainStrength)
         * @return 雨强度 [0.0-1.0]
         * 
         * TODO: 实现降雨强度获取，确保值域限制
         */
        static float GetRainStrength();

        /**
         * @brief 获取眼部亮度值 (对应Iris的getEyeBrightness)
         * @return 亮度值 Vector2i(blockLight*16, skyLight*16)
         * 
         * TODO: 实现基于LightLayer的光照计算
         */
        // static Vector2i GetEyeBrightness();

        /**
         * @brief 获取眼部水中状态 (对应Iris的isEyeInWater)
         * @return 0=空气, 1=水, 2=岩浆, 3=粉雪
         * 
         * TODO: 实现基于FogType的流体检测
         */
        static int IsEyeInWater();

        /**
         * @brief 获取天空颜色 (对应Iris的getSkyColor)
         * @return 天空颜色RGB值
         * 
         * TODO: 实现基于世界的天空颜色计算
         */
        // static Vector3d GetSkyColor();

        // ========================================================================
        // 玩家状态检查方法
        // ========================================================================

        static bool IsSneaking(); // 潜行状态
        static bool IsSprinting(); // 冲刺状态
        static bool IsHurt(); // 受伤状态 (hurtTime > 0)
        static bool IsInvisible(); // 隐身状态
        static bool IsBurning(); // 燃烧状态
        static bool IsOnGround(); // 着地状态

        /**
         * @brief 获取玩家心情值 (对应Iris的getPlayerMood)
         * @return 玩家心情 [0.0-1.0]
         */
        static float GetPlayerMood();

        /**
         * @brief 获取恒定心情值 (对应Iris的getConstantMood)
         * @return 恒定心情 [0.0-1.0]
         */
        static float GetConstantMood();
    };
} // namespace enigma::graphic
