#pragma once

namespace enigma::graphic
{
    /**
     * @brief Player Status Uniforms - 玩家状态数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/status/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Player Status" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过playerStatusBufferIndex访问
     * 3. 使用alignas确保与HLSL对齐要求一致
     * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<PlayerStatusUniforms> playerStatusBuffer =
     *     ResourceDescriptorHeap[playerStatusBufferIndex];
     * int eyeInWater = playerStatusBuffer[0].isEyeInWater;
     * float health = playerStatusBuffer[0].currentPlayerHealth;
     * bool sneaking = playerStatusBuffer[0].is_sneaking;
     * ```
     *
     * @note alignas(4)用于int/float/bool确保4字节对齐（HLSL标准对齐）
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct PlayerStatusUniforms
    {
        /**
         * @brief 眼睛是否在流体中
         * @type int
         * @iris isEyeInWater
         * @range 0, 1, 2, 3
         *
         * 教学要点:
         * - 0: 不在流体中
         * - 1: 在水中
         * - 2: 在熔岩中
         * - 3: 在其他流体中（如粉雪）
         */
        alignas(4) int isEyeInWater;

        /**
         * @brief 是否为旁观模式
         * @type bool
         * @iris isSpectator
         * @tag Iris Exclusive
         *
         * 教学要点: Minecraft旁观者模式检测
         */
        alignas(4) bool isSpectator;

        /**
         * @brief 是否右撇子
         * @type bool
         * @iris isRightHanded
         * @tag Iris Exclusive
         *
         * 教学要点: 主手设置检测（右手为主手=true）
         */
        alignas(4) bool isRightHanded;

        /**
         * @brief 失明效果强度
         * @type float
         * @iris blindness
         * @range [0,1]
         *
         * 教学要点: 失明药水效果的强度，1表示完全失明
         */
        alignas(4) float blindness;

        /**
         * @brief 黑暗效果因子
         * @type float
         * @iris darknessFactor
         * @range [0,1]
         *
         * 教学要点: 黑暗状态效果的强度（1.19+新增）
         */
        alignas(4) float darknessFactor;

        /**
         * @brief 黑暗光照因子
         * @type float
         * @iris darknessLightFactor
         * @range [0,1]
         *
         * 教学要点: 黑暗效果对光照的影响程度
         */
        alignas(4) float darknessLightFactor;

        /**
         * @brief 夜视效果强度
         * @type float
         * @iris nightVision
         * @range [0,1]
         *
         * 教学要点: 夜视药水效果的强度，1表示完全夜视
         */
        alignas(4) float nightVision;

        /**
         * @brief 玩家情绪值
         * @type float
         * @iris playerMood
         * @range [0,1]
         *
         * 教学要点: 玩家情绪系统，影响环境音效和氛围
         */
        alignas(4) float playerMood;

        /**
         * @brief 持续情绪值
         * @type float
         * @iris constantMood
         * @tag Iris Exclusive
         *
         * 教学要点: 稳定的情绪基准值
         */
        alignas(4) float constantMood;

        /**
         * @brief 当前玩家氧气（归一化）
         * @type float
         * @iris currentPlayerAir
         * @range -1, [0,1]
         * @tag Iris Exclusive
         *
         * 教学要点: -1表示不可用，[0,1]表示百分比
         */
        alignas(4) float currentPlayerAir;

        /**
         * @brief 最大玩家氧气
         * @type float
         * @iris maxPlayerAir
         * @tag Iris Exclusive
         *
         * 教学要点: 通常为300（Minecraft默认值）
         */
        alignas(4) float maxPlayerAir;

        /**
         * @brief 当前玩家护甲值（归一化）
         * @type float
         * @iris currentPlayerArmor
         * @range -1, [0,1]
         * @tag Iris Exclusive
         *
         * 教学要点: -1表示不可用，[0,1]表示护甲百分比
         */
        alignas(4) float currentPlayerArmor;

        /**
         * @brief 最大玩家护甲值
         * @type float
         * @iris maxPlayerArmor
         * @tag Iris Exclusive
         *
         * 教学要点: Minecraft最大护甲值为20
         */
        alignas(4) float maxPlayerArmor;

        /**
         * @brief 当前玩家血量（归一化）
         * @type float
         * @iris currentPlayerHealth
         * @range -1, [0,1]
         * @tag Iris Exclusive
         *
         * 教学要点: -1表示不可用，[0,1]表示生命值百分比
         */
        alignas(4) float currentPlayerHealth;

        /**
         * @brief 最大玩家血量
         * @type float
         * @iris maxPlayerHealth
         * @tag Iris Exclusive
         *
         * 教学要点: Minecraft默认最大血量为20
         */
        alignas(4) float maxPlayerHealth;

        /**
         * @brief 当前玩家饥饿值（归一化）
         * @type float
         * @iris currentPlayerHunger
         * @range -1, [0,1]
         * @tag Iris Exclusive
         *
         * 教学要点: -1表示不可用，[0,1]表示饥饿度百分比
         */
        alignas(4) float currentPlayerHunger;

        /**
         * @brief 最大玩家饥饿值
         * @type float
         * @iris maxPlayerHunger
         * @tag Iris Exclusive
         *
         * 教学要点: Minecraft最大饥饿值通常为20
         */
        alignas(4) float maxPlayerHunger;

        /**
         * @brief 是否着火
         * @type bool
         * @iris is_burning
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否处于燃烧状态
         */
        alignas(4) bool is_burning;

        /**
         * @brief 是否受伤
         * @type bool
         * @iris is_hurt
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否在受伤动画中
         */
        alignas(4) bool is_hurt;

        /**
         * @brief 是否隐身
         * @type bool
         * @iris is_invisible
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否处于隐身状态（药水效果）
         */
        alignas(4) bool is_invisible;

        /**
         * @brief 是否在地面
         * @type bool
         * @iris is_on_ground
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否站在地面上（非跳跃/下落状态）
         */
        alignas(4) bool is_on_ground;

        /**
         * @brief 是否潜行
         * @type bool
         * @iris is_sneaking
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否按下潜行键
         */
        alignas(4) bool is_sneaking;

        /**
         * @brief 是否疾跑
         * @type bool
         * @iris is_sprinting
         * @tag OptiFine Custom/Iris
         *
         * 教学要点: 玩家是否处于疾跑状态
         */
        alignas(4) bool is_sprinting;

        /**
         * @brief 是否隐藏GUI
         * @type bool
         * @iris hideGUI
         *
         * 教学要点: GUI是否被隐藏（F1键切换）
         */
        alignas(4) bool hideGUI;

        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        PlayerStatusUniforms()
            : isEyeInWater(0)
              , isSpectator(false)
              , isRightHanded(true) // 默认右撇子
              , blindness(0.0f)
              , darknessFactor(0.0f)
              , darknessLightFactor(0.0f)
              , nightVision(0.0f)
              , playerMood(0.5f) // 默认中性情绪
              , constantMood(0.5f) // 默认中性基准
              , currentPlayerAir(1.0f) // 默认满氧气
              , maxPlayerAir(300.0f) // Minecraft默认最大氧气
              , currentPlayerArmor(0.0f) // 默认无护甲
              , maxPlayerArmor(20.0f) // Minecraft最大护甲值
              , currentPlayerHealth(1.0f) // 默认满血
              , maxPlayerHealth(20.0f) // Minecraft默认最大血量
              , currentPlayerHunger(1.0f) // 默认满饥饿值
              , maxPlayerHunger(20.0f) // Minecraft最大饥饿值
              , is_burning(false)
              , is_hurt(false)
              , is_invisible(false)
              , is_on_ground(true) // 默认在地面
              , is_sneaking(false)
              , is_sprinting(false)
              , hideGUI(false)
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过512字节）
    static_assert(sizeof(PlayerStatusUniforms) <= 512,
                  "PlayerStatusUniforms too large, consider optimization");
} // namespace enigma::graphic
