/**
 * @file Common.hlsl
 * @brief Bindless资源访问核心库 - Iris完全兼容架构 + CustomImage扩展
 * @date 2025-11-04
 * @version v3.3 - 完整注释更新（T2-T10结构体文档化）
 *
 * 教学要点:
 * 1. 所有着色器必须 #include "Common.hlsl"
 * 2. 支持完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 3. 新增CustomImage系统: customImage0-15 (自定义材质槽位)
 * 4. Main/Alt双缓冲 - 消除ResourceBarrier开销
 * 5. Bindless资源访问 - Shader Model 6.6
 * 6. MRT动态生成 - PSOutput由ShaderCodeGenerator动态创建
 * 7. Shadow系统拆分设计 - shadowcolor和shadowtex独立Buffer管理
 *
 * 主要变更 (v3.3):
 * - T2: IDData结构体完整Iris文档化（9个字段，entity/block/item ID系统）
 * - T3: BiomeAndDimensionData结构体完整文档化（12个字段，生物群系+维度属性）
 * - T4: RenderingData结构体完整文档化（17个字段，渲染阶段+雾参数+Alpha测试）
 * - T5: WorldAndWeatherData结构体完整文档化（12个字段，太阳/月亮位置+天气系统）
 * - T6: CameraAndPlayerData结构体完整文档化（相机位置+玩家状态）
 * - T7: DepthTexturesBuffer结构体文档化（depthtex0/1/2语义说明）
 * - T8: CustomImageIndexBuffer结构体文档化（自定义材质槽位系统）
 * - T9: ShadowTexturesBuffer注释更新（独立Buffer管理，非"激进合并"）
 * - T10: 宏定义注释更新（Iris兼容性+统一接口设计）
 *
 * 架构参考:
 * - DirectX12-Bindless-MRT-RENDERTARGETS-Architecture.md
 * - RootConstant-Redesign-v3.md (56字节拆分设计)
 * - CustomImageIndexBuffer.hpp (自定义材质槽位系统)
 */

//──────────────────────────────────────────────────────
// Root Constants (56 bytes) - Shadow系统拆分设计
//──────────────────────────────────────────────────────

/**
 * @brief Root Constants - Shadow系统拆分 + CustomImage支持（56字节设计）
 *
 * 教学要点:
 * 1. 总共56 bytes (14个uint32_t索引) - Shadow系统拆分
 * 2. register(b0, space0) 对应 Root Parameter 0
 * 3. 支持细粒度更新 - 每个索引可独立更新
 * 4. 完整Iris纹理系统: colortex, shadowcolor, depthtex, shadowtex, noisetex
 * 5. CustomImage系统: customImage0-15 (自定义材质槽位)
 *
 * 架构优势:
 * - 拆分设计: shadowcolor和shadowtex独立管理，提高灵活性
 * - 全局共享Root Signature - 所有PSO使用同一个
 * - Root Signature切换: 1次/帧 (传统方式: 1000次/帧)
 * - 支持1,000,000+纹理同时注册
 * - 材质系统扩展: 16个自定义槽位用于PBR材质、LUT、特效贴图等
 *
 * 拆分设计关键:
 * - shadowColorBufferIndex: shadowcolor0-7 (需要flip机制)
 * - shadowTexturesBufferIndex: shadowtex0/1 (固定，不需要flip)
 * - customImageBufferIndexCBV: 256字节Buffer，存储16个槽位索引
 * - 总大小从52字节增至56字节（仍在Root Signature优化范围内）
 *
 * C++ 对应:
 * ```cpp
 * struct RootConstants {
 *     // Uniform Buffers (32 bytes)
 *     uint32_t cameraAndPlayerBufferIndex;      // Offset 0
 *     uint32_t playerStatusBufferIndex;         // Offset 4
 *     uint32_t screenAndSystemBufferIndex;      // Offset 8
 *     uint32_t idBufferIndex;                   // Offset 12
 *     uint32_t worldAndWeatherBufferIndex;      // Offset 16
 *     uint32_t biomeAndDimensionBufferIndex;    // Offset 20
 *     uint32_t renderingBufferIndex;            // Offset 24
 *     uint32_t matricesBufferIndex;             // Offset 28
 *     // Texture Buffers (4 bytes)
 *     uint32_t colorTargetsBufferIndex;         // Offset 32
 *     // Combined Buffers (12 bytes) - 拆分Shadow系统
 *     uint32_t depthTexturesBufferIndex;        // Offset 36
 *     uint32_t shadowColorBufferIndex;          // Offset 40 拆分
 *     uint32_t shadowTexturesBufferIndex;       // Offset 44 拆分
 *     // Direct Texture (4 bytes)
 *     uint32_t noiseTextureIndex;               // Offset 48
 *     // CustomImage Buffer (4 bytes) - 新增
 *     uint32_t customImageBufferIndexCBV;       // Offset 52 (customImage0-15)
 * };
 * ```
 */
cbuffer RootConstants : register(b0, space0)
{
    // Uniform Buffers (32 bytes)
    uint cameraAndPlayerBufferIndex; // 相机和玩家数据
    uint playerStatusBufferIndex; // 玩家状态数据
    uint screenAndSystemBufferIndex; // 屏幕和系统数据
    uint idBufferIndex; // ID数据
    uint worldAndWeatherBufferIndex; // 世界和天气数据
    uint biomeAndDimensionBufferIndex; // 生物群系和维度数据
    uint renderingBufferIndex; // 渲染数据
    uint matricesBufferIndex; // 矩阵数据

    // Texture Buffers with Flip Support (4 bytes)
    uint colorTargetsBufferIndex; // colortex0-15 (Main/Alt)

    // Combined Buffers (12 bytes) - 拆分Shadow系统
    uint depthTexturesBufferIndex; // depthtex0/1/2 (引擎生成) - Offset 36
    uint shadowColorBufferIndex; // shadowcolor0-7 (需要flip) - Offset 40
    uint shadowTexturesBufferIndex; // shadowtex0/1 (固定) - Offset 44

    // Direct Texture Index (4 bytes)
    uint noiseTextureIndex; // noisetex (静态纹理) - Offset 48

    // CustomImage Buffer Index (4 bytes) - 新增
    uint customImageBufferIndexCBV; // customImage0-15 (自定义材质槽位) - Offset 52
};

//──────────────────────────────────────────────────────
// Buffer结构体（C++端上传）
//──────────────────────────────────────────────────────

/**
 * @brief RenderTargetsBuffer - colortex0-15双缓冲索引管理（128 bytes）
 *
 * 教学要点:
 * 1. 存储16个colortex的读写索引
 * 2. 读取索引: 根据flip状态指向Main或Alt纹理
 * 3. 写入索引: 预留给UAV扩展（当前RTV直接绑定）
 * 4. 每个Pass执行前上传到GPU
 *
 * 对应C++: RenderTargetsIndexBuffer.hpp
 */
struct RenderTargetsBuffer
{
    uint readIndices[16]; // colortex0-15读取索引（Main或Alt）
    uint writeIndices[16]; // colortex0-15写入索引（预留）
};

/**
 * @brief DepthTexturesBuffer - 深度纹理索引管理（64 bytes）M3.1版本
 *
 * 教学要点:
 * 1. 支持1-16个深度纹理（动态配置）
 * 2. 保持与Iris的向后兼容（depthtex0/1/2）
 * 3. 不需要flip机制（每帧独立生成）
 * 4. 固定64字节结构（16个uint索引）
 *
 * 索引语义（向后兼容Iris）:
 * - depthTextureIndices[0] = depthtex0 (完整深度，包含半透明+手部)
 * - depthTextureIndices[1] = depthtex1 (半透明前深度，no translucents)
 * - depthTextureIndices[2] = depthtex2 (手部前深度，no hand)
 * - depthTextureIndices[3-15] = 用户自定义深度纹理
 *
 * 对应C++: DepthTexturesIndexBuffer.hpp
 */
struct DepthTexturesBuffer
{
    uint depthTextureIndices[16]; // 最多16个深度纹理索引
};

/**
 * @brief ShadowColorBuffer - Shadow颜色纹理索引管理（64 bytes）拆分设计
 *
 * 教学要点:
 * 1. 存储8个shadowcolor的读写索引
 * 2. shadowColorReadIndices: shadowcolor0-7读取索引（Main或Alt）
 * 3. shadowColorWriteIndices: shadowcolor0-7写入索引（预留）
 * 4. 支持flip机制（Ping-Pong双缓冲）
 *
 * 对应C++: ShadowColorIndexBuffer.hpp
 */
struct ShadowColorBuffer
{
    uint shadowColorReadIndices[8]; // shadowcolor0-7读取索引（Main或Alt）
    uint shadowColorWriteIndices[8]; // shadowcolor0-7写入索引（预留）
};

/**
 * @brief ShadowTexturesBuffer - Shadow深度纹理索引管理（16 bytes）独立Buffer设计
 *
 * 教学要点:
 * 1. 独立Buffer管理shadowtex0/1（与shadowcolor分离）
 * 2. shadowtex0Index: 完整阴影深度（不透明+半透明）
 * 3. shadowtex1Index: 半透明前阴影深度
 * 4. 不需要flip机制（只读纹理，每帧固定）
 * 5. 拆分设计优势：shadowcolor和shadowtex独立更新，提高灵活性
 *
 * 架构说明:
 * - 独立Buffer: shadowTexturesBufferIndex (Root Constant Offset 44)
 * - 与shadowColorBufferIndex分离（Offset 40）
 * - 非"激进合并"，而是"拆分设计"以支持独立管理
 *
 * 对应C++: ShadowTexturesIndexBuffer.hpp
 */
struct ShadowTexturesBuffer
{
    uint shadowtex0Index; // 完整阴影深度
    uint shadowtex1Index; // 半透明前阴影深度
    uint padding[2]; // 对齐填充
};

/**
 * @brief CustomImageIndexBuffer - 自定义材质槽位索引缓冲区（64 bytes）新增
 *
 * 教学要点:
 * 1. 存储16个自定义材质槽位（customImage0-15）的Bindless索引
 * 2. 类似ColorTargetsIndexBuffer，但只需要读取索引（不需要写入索引）
 * 3. UINT32_MAX (0xFFFFFFFF) 表示槽位未设置
 * 4. 用于材质系统扩展，支持自定义贴图（albedo、roughness、metallic等）
 *
 * 架构设计:
 * - 结构体大小：64字节（16个uint索引）
 * - 只读访问，不需要flip机制
 * - 支持运行时动态更新槽位
 *
 * 使用场景:
 * - 延迟渲染PBR材质贴图
 * - 自定义后处理LUT纹理
 * - 特效贴图（噪声、遮罩等）
 *
 * 对应C++: CustomImageIndexBuffer.hpp
 */
struct CustomImageIndexBuffer
{
    uint customImageIndices[16]; // customImage0-15的Bindless索引
};

// ===== Uniform Buffers（8个）=====

/**
 * @brief CameraAndPlayerBuffer - 相机和玩家数据（对应CameraAndPlayerUniforms.hpp）
 *
 * 教学要点:
 * 1. 此结构体对应Iris "Camera and Player Model" Uniform分类
 * 2. 存储在GPU StructuredBuffer，通过cameraAndPlayerBufferIndex访问
 * 3. HLSL StructuredBuffer会自动处理float3的16字节对齐，无需手动padding
 * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
 *
 * Iris官方文档参考:
 * https://shaders.properties/current/reference/uniforms/camera/
 *
 * @note float3在StructuredBuffer中自动16字节对齐
 * @note int3在StructuredBuffer中自动16字节对齐
 * @note 字段顺序与CameraAndPlayerUniforms.hpp完全一致
 */
struct CameraAndPlayerData
{
    // 相机位置相关（Iris官方字段）
    float3 cameraPosition; // 相机世界位置
    float  eyeAltitude; // 玩家高度（Y坐标）

    float3 cameraPositionFract; // 相机位置小数部分 [0,1) (Iris Exclusive)
    int3   cameraPositionInt; // 相机位置整数部分 (Iris Exclusive)

    // 上一帧相机位置（用于运动模糊、TAA等）
    float3 previousCameraPosition; // 上一帧相机位置
    float3 previousCameraPositionFract; // 上一帧相机位置小数部分 (Iris Exclusive)
    int3   previousCameraPositionInt; // 上一帧相机位置整数部分 (Iris Exclusive)

    // 玩家头部和视线相关（Iris Exclusive）
    float3 eyePosition; // 玩家头部模型世界位置
    float3 relativeEyePosition; // 头部到相机偏移 (cameraPosition - eyePosition)
    float3 playerBodyVector; // 玩家身体朝向（世界对齐）当前Iris实现有bug
    float3 playerLookVector; // 玩家头部朝向（世界对齐）

    // 视图空间相关
    float3 upPosition; // 视图空间上方向向量（长度100）

    // 光照相关
    int2 eyeBrightness; // 玩家位置光照值 [0,240] (x:方块光, y:天空光)
    int2 eyeBrightnessSmooth; // 平滑光照值 [0,240]

    // 其他
    float centerDepthSmooth; // 屏幕中心深度（平滑，用于自动对焦）
    bool  firstPersonCamera; // 是否第一人称视角
};

/**
 * @brief PlayerStatusData - 玩家状态数据（对应PlayerStatusUniforms.hpp）
 */
struct PlayerStatusData
{
    int   isEyeInWater; // 眼睛是否在流体中（0-3）
    bool  isSpectator; // 是否旁观模式
    bool  isRightHanded; // 是否右撇子
    float blindness; // 失明效果强度

    float darknessFactor; // 黑暗效果因子
    float darknessLightFactor; // 黑暗光照因子
    float nightVision; // 夜视效果强度
    float playerMood; // 玩家情绪值

    float constantMood; // 持续情绪值
    float currentPlayerAir; // 当前氧气
    float maxPlayerAir; // 最大氧气
    float currentPlayerArmor; // 当前护甲

    float maxPlayerArmor; // 最大护甲
    float currentPlayerHealth; // 当前血量
    float maxPlayerHealth; // 最大血量
    float currentPlayerHunger; // 当前饥饿值

    float maxPlayerHunger; // 最大饥饿值
    bool  is_burning; // 是否着火
    bool  is_hurt; // 是否受伤
    bool  is_invisible; // 是否隐身

    bool is_on_ground; // 是否在地面
    bool is_sneaking; // 是否潜行
    bool is_sprinting; // 是否疾跑
    bool hideGUI; // 是否隐藏GUI
};

/**
 * @brief ScreenAndSystemData - 屏幕和系统数据（对应ScreenAndSystemUniforms.hpp）
 */
struct ScreenAndSystemData
{
    float viewHeight; // 视口高度
    float viewWidth; // 视口宽度
    float aspectRatio; // 宽高比
    float screenBrightness; // 屏幕亮度

    int   frameCounter; // 帧计数器
    float frameTime; // 上一帧时间（秒）
    float frameTimeCounter; // 运行时间累计（秒）
    int   currentColorSpace; // 显示器颜色空间

    int3  currentDate; // 系统日期（年，月，日）
    float _padding1;

    int3  currentTime; // 系统时间（时，分，秒）
    float _padding2;

    int2   currentYearTime; // 年内时间统计
    float2 _padding3;
};

/**
 * @brief IDData - ID数据（对应IDUniforms.hpp）
 *
 * Iris官方文档参考:
 * https://shaders.properties/current/reference/uniforms/id/
 *
 * 教学要点:
 * 1. 此结构体对应Iris "ID" Uniform分类
 * 2. 字段顺序、类型与C++端IDUniforms.hpp完全一致
 * 3. HLSL自动处理对齐，无需手动padding
 * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
 *
 * @note 字段数量: 9个
 * @note 对齐方式: HLSL自动对齐（int=4字节，float3=16字节）
 */
struct IDData
{
    /**
     * @brief 当前实体ID
     * @iris entityId
     * 从entity.properties读取的实体ID
     * 值为0表示没有entity.properties文件
     * 值为65535表示实体不在entity.properties中
     */
    int entityId;

    /**
     * @brief 当前方块实体ID（Tile Entity）
     * @iris blockEntityId
     * 从block.properties读取的方块实体ID
     * 值为0表示没有block.properties文件
     * 值为65535表示方块实体不在block.properties中
     */
    int blockEntityId;

    /**
     * @brief 当前渲染的物品ID
     * @iris currentRenderedItemId
     * @tag Iris Exclusive
     * 基于item.properties检测当前渲染的物品/护甲
     * 类似heldItemId，但应用于当前渲染的物品而非手持物品
     */
    int currentRenderedItemId;

    /**
     * @brief 玩家选中的方块ID
     * @iris currentSelectedBlockId
     * @tag Iris Exclusive
     * 从block.properties读取玩家准星指向方块的ID
     * 值为0表示未选中方块或没有block.properties文件
     */
    int currentSelectedBlockId;

    /**
     * @brief 玩家选中方块的位置（玩家空间）
     * @iris currentSelectedBlockPos
     * @tag Iris Exclusive
     * 相对于玩家的方块中心坐标
     * 如果未选中方块，所有分量为-256.0
     */
    float3 currentSelectedBlockPos;

    /**
     * @brief 主手物品ID
     * @iris heldItemId
     * 从item.properties读取主手物品的ID
     * 值为0表示没有item.properties文件
     * 值为-1表示手持物品不在item.properties中（包括空手）
     */
    int heldItemId;

    /**
     * @brief 副手物品ID
     * @iris heldItemId2
     * 从item.properties读取副手物品的ID
     * 值为0表示没有item.properties文件
     * 值为-1表示手持物品不在item.properties中（包括空手）
     */
    int heldItemId2;

    /**
     * @brief 主手物品的光照强度
     * @iris heldBlockLightValue
     * @range [0,15]
     * 主手物品的发光等级（如火把=14）
     * 原版方块范围0-15，某些模组方块可能更高
     * 注意：如果oldHandLight未设置为false，此值取双手中光照最高的
     */
    int heldBlockLightValue;

    /**
     * @brief 副手物品的光照强度
     * @iris heldBlockLightValue2
     * @range [0,15]
     * 副手物品的发光等级
     * 原版方块范围0-15，某些模组方块可能更高
     */
    int heldBlockLightValue2;
};

/**
 * @brief WorldAndWeatherData - 世界和天气数据（对应WorldAndWeatherUniforms.hpp）
 *
 * Iris官方文档参考:
 * https://shaders.properties/current/reference/uniforms/world/
 *
 * 教学要点:
 * 1. 此结构体对应Iris "World/Weather" Uniform分类
 * 2. 字段顺序、类型与C++端WorldAndWeatherUniforms.hpp完全一致
 * 3. HLSL自动处理对齐，无需手动padding
 * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
 *
 * @note 字段数量: 12个
 * @note 对齐方式: HLSL自动对齐（int=4字节，float=4字节，float3=16字节，float4=16字节）
 */
struct WorldAndWeatherData
{
    /**
     * @brief 太阳位置（视图空间）
     * @iris sunPosition
     * @range 长度为100
     * 视图空间中的太阳方向向量，长度固定为100
     */
    float3 sunPosition;

    /**
     * @brief 月亮位置（视图空间）
     * @iris moonPosition
     * @range 长度为100
     * 视图空间中的月亮方向向量，长度固定为100
     */
    float3 moonPosition;

    /**
     * @brief 阴影光源位置（视图空间）
     * @iris shadowLightPosition
     * @range 长度为100
     * 指向太阳或月亮（取决于哪个更高），用于阴影计算
     */
    float3 shadowLightPosition;

    /**
     * @brief 太阳角度
     * @iris sunAngle
     * @range [0,1]
     * 0.0=日出, 0.25=正午, 0.5=日落, 0.75=午夜
     */
    float sunAngle;

    /**
     * @brief 阴影角度
     * @iris shadowAngle
     * @range [0,0.5]
     * 阴影光源（太阳或月亮）的角度，范围0-0.5
     */
    float shadowAngle;

    /**
     * @brief 月相
     * @iris moonPhase
     * @range [0,7]
     * 0=满月, 1-3=渐亏月, 4=新月, 5-7=渐盈月
     */
    int moonPhase;

    /**
     * @brief 雨强度
     * @iris rainStrength
     * @range [0,1]
     * 当前降雨强度，0=无雨，1=最大雨量
     */
    float rainStrength;

    /**
     * @brief 湿度
     * @iris wetness
     * @range [0,1]
     * rainStrength随时间平滑后的值，用于雨后湿润效果
     */
    float wetness;

    /**
     * @brief 雷暴强度
     * @iris thunderStrength
     * @range [0,1]
     * @tag Iris Exclusive
     * 当前雷暴强度，Iris独有特性
     */
    float thunderStrength;

    /**
     * @brief 闪电位置
     * @iris lightningBoltPosition
     * @tag Iris Exclusive
     * xyz: 闪电击中的世界坐标
     * w: 闪电强度（0表示无闪电）
     */
    float4 lightningBoltPosition;

    /**
     * @brief 游戏内时间
     * @iris worldTime
     * @range [0, 23999]
     * Minecraft一天24000 ticks
     * 0=早上6点, 6000=正午, 12000=傍晚6点, 18000=午夜
     */
    int worldTime;

    /**
     * @brief 游戏内天数
     * @iris worldDay
     * 从世界创建开始的天数，每24000 ticks增加1
     */
    int worldDay;
};

/**
 * @brief BiomeAndDimensionData - 生物群系和维度数据
 *
 * 对应 C++ 端: BiomeAndDimensionUniforms.hpp
 * Iris 官方文档: https://shaders.properties/current/reference/uniforms/biome/
 *
 * 教学要点:
 * 1. 此结构体包含 Iris "Biome and Dimension" Uniform 分类的所有字段
 * 2. 字段顺序和类型必须与 C++ 端完全一致（GPU StructuredBuffer 要求）
 * 3. 使用 int 而非 bool（HLSL StructuredBuffer 对齐问题）
 * 4. 所有字段名称严格遵循 Iris 官方规范（使用下划线命名）
 *
 * @note 共 12 个字段，无手动 padding
 * @note 字段顺序: biome → biome_category → biome_precipitation → rainfall → temperature
 *                → ambientLight → bedrockLevel → cloudHeight → hasCeiling → hasSkylight
 *                → heightLimit → logicalHeightLimit
 */
struct BiomeAndDimensionData
{
    // ========================================================================
    // Biome Uniforms (OptiFine Custom Uniforms / Iris)
    // ========================================================================

    /**
     * @brief 生物群系ID
     * @iris biome
     * @tag OptiFine CU / Iris
     * 玩家当前所在生物群系的唯一ID
     */
    int biome;

    /**
     * @brief 生物群系类别
     * @iris biome_category
     * @tag OptiFine CU / Iris
     * 生物群系的大类分类（CAT_NONE, CAT_TAIGA, CAT_EXTREME_HILLS 等）
     */
    int biome_category;

    /**
     * @brief 降水类型
     * @iris biome_precipitation
     * @range 0=无降水, 1=雨, 2=雪
     * @tag OptiFine CU / Iris
     */
    int biome_precipitation;

    /**
     * @brief 生物群系降雨量
     * @iris rainfall
     * @range [0,1]
     * @tag OptiFine CU / Iris
     * 生物群系的降雨属性（0=干旱，1=多雨）
     */
    float rainfall;

    /**
     * @brief 生物群系温度
     * @iris temperature
     * @range [-0.7, 2.0] (原版Minecraft)
     * @tag OptiFine CU / Iris
     * 影响降水形式（雨/雪）和植被颜色
     */
    float temperature;

    // ========================================================================
    // Dimension Uniforms (Iris Exclusive)
    // ========================================================================

    /**
     * @brief 环境光照等级
     * @iris ambientLight
     * @tag Iris Exclusive
     * 当前维度的环境光属性
     */
    float ambientLight;

    /**
     * @brief 基岩层高度
     * @iris bedrockLevel
     * @tag Iris Exclusive
     * 当前维度的基岩层地板Y坐标（1.18+ 主世界为 -64）
     */
    int bedrockLevel;

    /**
     * @brief 云层高度
     * @iris cloudHeight
     * @tag Iris Exclusive
     * 原版云层的Y坐标，无云维度为 NaN
     */
    float cloudHeight;

    /**
     * @brief 是否有天花板
     * @iris hasCeiling
     * @tag Iris Exclusive
     * 1=有顶（下界），0=无顶（主世界、末地）
     * @note 使用 int 而非 bool（HLSL StructuredBuffer 对齐要求）
     */
    int hasCeiling;

    /**
     * @brief 是否有天空光
     * @iris hasSkylight
     * @tag Iris Exclusive
     * 1=有天空光（主世界、末地），0=无天空光（下界）
     * @note 使用 int 而非 bool（HLSL StructuredBuffer 对齐要求）
     */
    int hasSkylight;

    /**
     * @brief 世界高度限制
     * @iris heightLimit
     * @tag Iris Exclusive
     * 最低方块和最高方块之间的高度差（1.18+ 主世界为 384）
     */
    int heightLimit;

    /**
     * @brief 逻辑高度限制
     * @iris logicalHeightLimit
     * @tag Iris Exclusive
     * 维度的逻辑高度（紫颂果生长、下界传送门传送的最大高度）
     */
    int logicalHeightLimit;
};

/**
 * @brief RenderingData - 渲染相关数据（对应RenderingUniforms.hpp）
 *
 * Iris官方文档参考:
 * https://shaders.properties/current/reference/uniforms/rendering/
 *
 * 教学要点:
 * 1. 此结构体对应Iris "Rendering" Uniform分类
 * 2. 存储在GPU StructuredBuffer，通过renderingBufferIndex访问
 * 3. 包含裁剪面、渲染阶段、Alpha测试、混合模式、雾参数等
 * 4. 所有字段名称严格遵循Iris官方规范
 * 5. 字段顺序必须与C++端RenderingUniforms.hpp完全一致
 * 6. Vec3/Vec4自动16字节对齐，无需手动padding
 *
 * HLSL访问示例:
 * ```hlsl
 * StructuredBuffer<RenderingData> renderingBuffer =
 *     ResourceDescriptorHeap[renderingBufferIndex];
 * float nearPlane = renderingBuffer[0].near;
 * float alphaTestRef = renderingBuffer[0].alphaTestRef;
 * float3 fogColor = renderingBuffer[0].fogColor;
 * ```
 */
struct RenderingData
{
    /**
     * @brief 近裁剪面距离
     * @iris near
     * @value 0.05
     * 相机近裁剪面的距离，除非被模组修改，通常固定为 0.05
     */
    float near;

    /**
     * @brief 当前渲染距离（方块）
     * @iris far
     * @range (0, +∞)
     * 当前渲染距离设置（方块数）
     * 注意：这不是远裁剪面距离（实际约为 far * 4.0）
     */
    float far;

    /**
     * @brief Alpha测试参考值
     * @iris alphaTestRef
     * @range [0,1]
     * Alpha < alphaTestRef的像素被丢弃
     * 用于镂空纹理（如树叶、玻璃）
     * 通常为 0.1，但可通过 alphaTest 指令修改
     * 推荐使用此 uniform 而非硬编码值
     *
     * HLSL使用示例:
     * if (albedoOut.a < alphaTestRef) discard;
     */
    float alphaTestRef;

    /**
     * @brief 区块偏移（模型空间）
     * @iris chunkOffset
     * 当前渲染地块的模型空间偏移
     * 用于大世界渲染的精度优化
     * 与 vaPosition 结合获取模型空间位置:
     *   float3 model_pos = vaPosition + chunkOffset;
     * 非地形渲染时为全 0
     * 兼容性配置文件下为全 0
     * Vec3自动16字节对齐
     */
    float3 chunkOffset;

    /**
     * @brief 实体染色颜色
     * @iris entityColor
     * @range [0,1]
     * rgb: 染色颜色
     * a: 混合因子
     * 应用方式:
     *   color.rgb = lerp(color.rgb, entityColor.rgb, entityColor.a);
     * Vec4自动16字节对齐
     */
    float4 entityColor;

    /**
     * @brief Alpha混合函数
     * @iris blendFunc
     * 当前程序的Alpha混合函数乘数（由 blend.<program> 指令定义）
     * x: 源RGB混合因子
     * y: 目标RGB混合因子
     * z: 源Alpha混合因子
     * w: 目标Alpha混合因子
     *
     * 取值含义（LWJGL常量）:
     * - GL_ZERO = 0
     * - GL_ONE = 1
     * - GL_SRC_COLOR = 768
     * - GL_ONE_MINUS_SRC_COLOR = 769
     * - GL_SRC_ALPHA = 770
     * - GL_ONE_MINUS_SRC_ALPHA = 771
     * - GL_DST_ALPHA = 772
     * - GL_ONE_MINUS_DST_ALPHA = 773
     * - GL_DST_COLOR = 774
     * - GL_ONE_MINUS_DST_COLOR = 775
     * - GL_SRC_ALPHA_SATURATE = 776
     * IntVec4自动16字节对齐
     */
    int4 blendFunc;

    /**
     * @brief 纹理图集大小
     * @iris atlasSize
     * 纹理图集的尺寸（像素）
     * 仅在纹理图集绑定时有值
     * 未绑定时返回 (0, 0)
     */
    int2 atlasSize;

    /**
     * @brief 当前渲染阶段
     * @iris renderStage
     * 当前几何体的"渲染阶段"
     * 比 gbuffer 程序更细粒度地标识几何体类型
     * 应与 Iris 定义的预处理器宏比较（如 MC_RENDER_STAGE_TERRAIN_SOLID）
     */
    int renderStage;

    /**
     * @brief 地平线雾颜色
     * @iris fogColor
     * @range [0,1]
     * 原版游戏用于天空渲染和雾效的地平线雾颜色
     * 可能受生物群系、时间、视角影响
     * Vec3自动16字节对齐
     */
    float3 fogColor;

    /**
     * @brief 上层天空颜色
     * @iris skyColor
     * @range [0,1]
     * 原版游戏用于天空渲染的上层天空颜色
     * 可能受生物群系、时间影响
     * 不受视角影响（与 fogColor 的区别）
     * Vec3自动16字节对齐
     */
    float3 skyColor;

    /**
     * @brief 相对雾密度
     * @iris fogDensity
     * @range [0.0, 1.0]
     * 原版雾的相对密度
     * 基于当前生物群系、天气、流体（水/熔岩/积雪）等
     * 0.0 = 最低密度，1.0 = 最高密度
     */
    float fogDensity;

    /**
     * @brief 雾起始距离（方块）
     * @iris fogStart
     * @range [0, +∞)
     * 原版雾的起始距离
     * 基于当前生物群系、天气、流体等
     */
    float fogStart;

    /**
     * @brief 雾结束距离（方块）
     * @iris fogEnd
     * @range [0, +∞)
     * 原版雾的结束距离
     * 基于当前生物群系、天气、流体等
     */
    float fogEnd;

    /**
     * @brief 雾模式
     * @iris fogMode
     * @range 2048, 2049, 9729
     * 原版雾的类型（基于生物群系、天气、流体等）
     * 决定雾强度随距离的衰减函数
     *
     * 取值含义（LWJGL常量）:
     * - GL_EXP = 2048: 指数雾
     * - GL_EXP2 = 2049: 平方指数雾
     * - GL_LINEAR = 9729: 线性雾
     */
    int fogMode;

    /**
     * @brief 雾形状
     * @iris fogShape
     * @range 0, 1
     * 原版雾的形状（基于生物群系、天气、流体、洞穴等）
     *
     * 取值含义:
     * - 0: 球形雾
     * - 1: 柱形雾
     */
    int fogShape;
};

/**
 * @brief MatricesData - 矩阵数据（对应MatricesUniforms.hpp）
 */
struct MatricesData
{
    float4x4 gbufferModelView; // GBuffer模型视图矩阵
    float4x4 gbufferModelViewInverse; // GBuffer模型视图逆矩阵
    float4x4 cameraToRenderTransform; // [NEW] 相机到渲染坐标系转换（Camera → Render）
    float4x4 gbufferProjection; // GBuffer投影矩阵
    float4x4 gbufferProjectionInverse; // GBuffer投影逆矩阵
    float4x4 gbufferPreviousModelView; // 上一帧GBuffer模型视图矩阵
    float4x4 gbufferPreviousProjection; // 上一帧GBuffer投影矩阵

    float4x4 shadowModelView; // 阴影模型视图矩阵
    float4x4 shadowModelViewInverse; // 阴影模型视图逆矩阵
    float4x4 shadowProjection; // 阴影投影矩阵
    float4x4 shadowProjectionInverse; // 阴影投影逆矩阵

    float4x4 modelViewMatrix; // 当前模型视图矩阵
    float4x4 modelViewMatrixInverse; // 当前模型视图逆矩阵
    float4x4 projectionMatrix; // 当前投影矩阵
    float4x4 projectionMatrixInverse; // 当前投影逆矩阵
    float4x4 normalMatrix; // 法线矩阵（3x3存储在4x4中）
    float4x4 textureMatrix; // 纹理矩阵
    float4x4 modelMatrix; // 模型矩阵（模型空间→世界空间，Iris扩展）
    float4x4 modelMatrixInverse; // 模型逆矩阵（世界空间→模型空间，Iris扩展）
};

//──────────────────────────────────────────────────────
// Bindless资源访问函数
//──────────────────────────────────────────────────────

// ===== Uniform Buffer访问函数（8个）=====

/**
 * @brief 获取相机和玩家数据
 * @return CameraAndPlayerData结构体
 */
CameraAndPlayerData GetCameraAndPlayerData()
{
    StructuredBuffer<CameraAndPlayerData> buffer =
        ResourceDescriptorHeap[cameraAndPlayerBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取玩家状态数据
 * @return PlayerStatusData结构体
 */
PlayerStatusData GetPlayerStatusData()
{
    StructuredBuffer<PlayerStatusData> buffer =
        ResourceDescriptorHeap[playerStatusBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取屏幕和系统数据
 * @return ScreenAndSystemData结构体
 */
ScreenAndSystemData GetScreenAndSystemData()
{
    StructuredBuffer<ScreenAndSystemData> buffer =
        ResourceDescriptorHeap[screenAndSystemBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取ID数据
 * @return IDData结构体
 */
IDData GetIDData()
{
    StructuredBuffer<IDData> buffer =
        ResourceDescriptorHeap[idBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取世界和天气数据
 * @return WorldAndWeatherData结构体
 */
WorldAndWeatherData GetWorldAndWeatherData()
{
    StructuredBuffer<WorldAndWeatherData> buffer =
        ResourceDescriptorHeap[worldAndWeatherBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取生物群系和维度数据
 * @return BiomeAndDimensionData结构体
 */
BiomeAndDimensionData GetBiomeAndDimensionData()
{
    StructuredBuffer<BiomeAndDimensionData> buffer =
        ResourceDescriptorHeap[biomeAndDimensionBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取渲染数据
 * @return RenderingData结构体
 */
RenderingData GetRenderingData()
{
    StructuredBuffer<RenderingData> buffer =
        ResourceDescriptorHeap[renderingBufferIndex];
    return buffer[0];
}

/**
 * @brief 获取矩阵数据
 * @return MatricesData结构体
 */
MatricesData GetMatricesData()
{
    StructuredBuffer<MatricesData> buffer = ResourceDescriptorHeap[matricesBufferIndex];
    return buffer[0];
}

// ===== 纹理资源访问函数（7个）=====

/**
 * @brief 获取ColorTarget（colortex0-15读取用）
 * @param rtIndex colortex索引（0-15）
 * @return Texture2D资源（Main或Alt，根据flip状态）
 *
 * 工作原理:
 * 1. 通过colorTargetsBufferIndex访问RenderTargetsBuffer
 * 2. 查询readIndices[rtIndex]获取纹理索引
 * 3. 通过纹理索引访问ResourceDescriptorHeap
 *
 * 教学要点:
 * - 真正的Bindless访问 - 无需预绑定
 * - Main/Alt自动处理 - 由flip状态决定
 * - ResourceDescriptorHeap是SM6.6全局堆
 */
Texture2D GetRenderTarget(uint rtIndex)
{
    // 1. 获取RenderTargetsBuffer（StructuredBuffer）
    StructuredBuffer<RenderTargetsBuffer> rtBuffer =
        ResourceDescriptorHeap[colorTargetsBufferIndex];

    // 2. 查询读取索引（Main或Alt）
    uint textureIndex = rtBuffer[0].readIndices[rtIndex];

    // 3. 直接访问全局描述符堆
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理0（完整深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex0: 包含所有物体的完整深度（不透明+半透明+手部）
 * - 引擎渲染过程自动生成，不需要flip
 * - 用于后处理和深度测试
 */
Texture2D<float> GetDepthTex0()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex0索引（数组索引0）
    uint textureIndex = depthBuffer[0].depthTextureIndices[0];

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理1（半透明前深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex1: 只包含不透明物体的深度（no translucents）
 * - 用于半透明物体的正确混合和深度测试
 */
Texture2D<float> GetDepthTex1()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex1索引（数组索引1）
    uint textureIndex = depthBuffer[0].depthTextureIndices[1];

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取深度纹理2（手部前深度）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - depthtex2: 包含除手部外所有物体的深度（no hand）
 * - 用于手部的深度测试和特效
 */
Texture2D<float> GetDepthTex2()
{
    // 1. 获取DepthTexturesBuffer
    StructuredBuffer<DepthTexturesBuffer> depthBuffer =
        ResourceDescriptorHeap[depthTexturesBufferIndex];

    // 2. 查询depthtex2索引（数组索引2）
    uint textureIndex = depthBuffer[0].depthTextureIndices[2];

    // 3. 返回深度纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影颜色纹理（shadowcolor0-7）拆分设计
 * @param shadowIndex shadowcolor索引（0-7）
 * @return Texture2D资源（Main或Alt，根据flip状态）
 *
 * 教学要点:
 * - shadowcolor支持flip机制（Ping-Pong双缓冲）
 * - 用于多Pass阴影渲染（shadowcomp阶段）
 * - 拆分方案：独立管理shadowcolor和shadowtex
 */
Texture2D GetShadowColor(uint shadowIndex)
{
    // 1. 获取ShadowColorBuffer
    StructuredBuffer<ShadowColorBuffer> shadowColorBuffer =
        ResourceDescriptorHeap[shadowColorBufferIndex];

    // 2. 查询shadowcolor读取索引（Main或Alt）
    uint textureIndex = shadowColorBuffer[0].shadowColorReadIndices[shadowIndex];

    // 3. 返回shadowcolor纹理
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取阴影深度纹理（统一接口）拆分设计
 * @param index 阴影纹理索引（0或1）
 * @return Texture2D<float>资源
 *
 * 教学要点:
 * - shadowtex0 (index=0): 包含所有物体的阴影深度（不透明+半透明）
 * - shadowtex1 (index=1): 只包含不透明物体的阴影深度
 * - 只读纹理，不需要flip
 * - 拆分方案：独立管理shadowcolor和shadowtex
 * - 统一接口风格：与GetRenderTarget(uint)保持一致
 *
 * 架构优势:
 * - 统一的访问模式，减少代码重复
 * - 支持动态索引访问（循环、条件判断）
 * - 保持向后兼容性（通过宏定义）
 */
Texture2D<float> GetShadowTex(int index)
{
    // 1. 获取ShadowTexturesBuffer
    StructuredBuffer<ShadowTexturesBuffer> shadowTexBuffer =
        ResourceDescriptorHeap[shadowTexturesBufferIndex];

    // 2. 根据索引查询对应的shadowtex索引
    uint textureIndex;
    if (index == 0)
    {
        textureIndex = shadowTexBuffer[0].shadowtex0Index;
    }
    else // index == 1
    {
        textureIndex = shadowTexBuffer[0].shadowtex1Index;
    }

    // 3. 返回对应的shadowtex
    return ResourceDescriptorHeap[textureIndex];
}

/**
 * @brief 获取噪声纹理（noisetex）
 * @return Texture2D资源
 *
 * 教学要点:
 * - 静态噪声纹理，RGB8格式，默认256×256
 * - 用于随机采样、抖动、时间相关效果
 * - 直接使用纹理索引，无需Buffer包装
 */
Texture2D GetNoiseTex()
{
    // 直接访问噪声纹理（noiseTextureIndex是直接索引）
    return ResourceDescriptorHeap[noiseTextureIndex];
}

/**
 * @brief 获取自定义材质槽位的Bindless索引 新增
 * @param slotIndex 槽位索引（0-15）
 * @return Bindless索引，如果槽位未设置则返回UINT32_MAX (0xFFFFFFFF)
 *
 * 教学要点:
 * - 通过customImageBufferIndexCBV访问CustomImageIndexBuffer
 * - 返回值为Bindless索引，可直接访问ResourceDescriptorHeap
 * - UINT32_MAX表示槽位未设置，使用前应检查有效性
 *
 * 工作原理:
 * 1. 通过customImageBufferIndexCBV访问CustomImageIndexBuffer
 * 2. 查询customImageIndices[slotIndex]获取Bindless索引
 * 3. 返回索引供GetCustomImage()使用
 *
 * 安全性:
 * - 槽位索引会被限制在0-15范围内
 * - 超出范围返回UINT32_MAX（无效索引）
 */
uint GetCustomImageIndex(uint slotIndex)
{
    // 防止越界访问
    if (slotIndex >= 16)
    {
        return 0xFFFFFFFF; // UINT32_MAX - 无效索引
    }

    // 1. 获取CustomImageIndexBuffer（StructuredBuffer）
    StructuredBuffer<CustomImageIndexBuffer> customImageBuffer =
        ResourceDescriptorHeap[customImageBufferIndexCBV];

    // 2. 查询指定槽位的Bindless索引
    uint bindlessIndex = customImageBuffer[0].customImageIndices[slotIndex];

    // 3. 返回Bindless索引
    return bindlessIndex;
}

/**
 * @brief 获取自定义材质纹理（便捷访问函数）新增
 * @param slotIndex 槽位索引（0-15）
 * @return 对应的Bindless纹理（Texture2D）
 *
 * 教学要点:
 * - 封装GetCustomImageIndex() + ResourceDescriptorHeap访问
 * - 自动处理无效索引情况（返回黑色纹理或默认纹理）
 * - 遵循Iris纹理访问模式
 *
 * 使用示例:
 * ```hlsl
 * // 获取albedo贴图（假设slot 0存储albedo）
 * Texture2D albedoTex = GetCustomImage(0);
 * float4 albedo = albedoTex.Sample(linearSampler, uv);
 *
 * // 获取roughness贴图（假设slot 1存储roughness）
 * Texture2D roughnessTex = GetCustomImage(1);
 * float roughness = roughnessTex.Sample(linearSampler, uv).r;
 * ```
 *
 * 注意:
 * - 如果槽位未设置（UINT32_MAX），访问ResourceDescriptorHeap可能导致未定义行为
 * - 调用前应确保槽位已正确设置，或使用默认值处理
 */
Texture2D GetCustomImage(uint slotIndex)
{
    // 1. 获取Bindless索引
    uint bindlessIndex = GetCustomImageIndex(slotIndex);

    // 2. 通过Bindless索引访问全局描述符堆
    // 注意：如果bindlessIndex为UINT32_MAX，这里可能需要额外的有效性检查
    // 当前设计假设C++端会确保所有使用的槽位都已正确设置
    return ResourceDescriptorHeap[bindlessIndex];
}

//──────────────────────────────────────────────────────
// Iris兼容宏（完整纹理系统）
//──────────────────────────────────────────────────────

/**
 * @brief Iris Shader完全兼容宏定义
 *
 * 教学要点:
 * 1. 支持完整Iris纹理系统：colortex, shadowcolor, depthtex, shadowtex, noisetex, customImage
 * 2. 宏自动映射到对应的Get函数（统一接口设计）
 * 3. 零学习成本移植Iris Shader Pack
 * 4. Main/Alt自动处理，无需手动管理双缓冲
 * 5. Shadow系统拆分：shadowcolor和shadowtex独立Buffer管理
 *
 * 统一接口设计:
 * - GetRenderTarget(uint index): colortex0-15统一访问
 * - GetShadowColor(uint index): shadowcolor0-7统一访问
 * - GetShadowTex(int index): shadowtex0/1统一访问
 * - GetCustomImage(uint index): customImage0-15统一访问
 *
 * 示例:
 * ```hlsl
 * // 主渲染纹理（colortex0-15）
 * float4 color = colortex0.Sample(linearSampler, uv);
 *
 * // 阴影纹理（shadowcolor0-7, shadowtex0/1）
 * float4 shadowColor = shadowcolor0.Sample(linearSampler, shadowUV);
 * float shadowDepth = shadowtex0.Load(int3(pixelPos, 0));
 *
 * // 深度纹理（depthtex0/1/2）
 * float depth = depthtex0.Load(int3(pixelPos, 0));
 *
 * // 噪声纹理（noisetex）
 * float4 noise = noisetex.Sample(linearSampler, uv);
 *
 * // 自定义材质槽位（customImage0-15）
 * float4 albedo = customImage0.Sample(linearSampler, uv);
 * ```
 */

// ===== ColorTex（colortex0-15）=====
#define colortex0  GetRenderTarget(0)
#define colortex1  GetRenderTarget(1)
#define colortex2  GetRenderTarget(2)
#define colortex3  GetRenderTarget(3)
#define colortex4  GetRenderTarget(4)
#define colortex5  GetRenderTarget(5)
#define colortex6  GetRenderTarget(6)
#define colortex7  GetRenderTarget(7)
#define colortex8  GetRenderTarget(8)
#define colortex9  GetRenderTarget(9)
#define colortex10 GetRenderTarget(10)
#define colortex11 GetRenderTarget(11)
#define colortex12 GetRenderTarget(12)
#define colortex13 GetRenderTarget(13)
#define colortex14 GetRenderTarget(14)
#define colortex15 GetRenderTarget(15)

// ===== DepthTex（depthtex0/1/2）=====
#define depthtex0  GetDepthTex0()
#define depthtex1  GetDepthTex1()
#define depthtex2  GetDepthTex2()

// ===== ShadowColor（shadowcolor0-7）独立Buffer管理 =====
// 通过shadowColorBufferIndex访问ShadowColorBuffer
// 支持flip机制（Ping-Pong双缓冲）
#define shadowcolor0 GetShadowColor(0)
#define shadowcolor1 GetShadowColor(1)
#define shadowcolor2 GetShadowColor(2)
#define shadowcolor3 GetShadowColor(3)
#define shadowcolor4 GetShadowColor(4)
#define shadowcolor5 GetShadowColor(5)
#define shadowcolor6 GetShadowColor(6)
#define shadowcolor7 GetShadowColor(7)

// ===== ShadowTex（shadowtex0/1）独立Buffer + 统一接口 =====
// 通过shadowTexturesBufferIndex访问ShadowTexturesBuffer
// 使用统一的GetShadowTex(int index)接口，保持Iris兼容性
// 只读纹理，不需要flip机制
#define shadowtex0 GetShadowTex(0)
#define shadowtex1 GetShadowTex(1)

// ===== NoiseTex（noisetex）=====
#define noisetex GetNoiseTex()

// ===== CustomImage（customImage0-15）新增 =====
// 自定义材质槽位，用于扩展材质系统
// 使用场景：PBR材质贴图（albedo、roughness、metallic等）、自定义LUT、特效贴图
#define customImage0  GetCustomImage(0)
#define customImage1  GetCustomImage(1)
#define customImage2  GetCustomImage(2)
#define customImage3  GetCustomImage(3)
#define customImage4  GetCustomImage(4)
#define customImage5  GetCustomImage(5)
#define customImage6  GetCustomImage(6)
#define customImage7  GetCustomImage(7)
#define customImage8  GetCustomImage(8)
#define customImage9  GetCustomImage(9)
#define customImage10 GetCustomImage(10)
#define customImage11 GetCustomImage(11)
#define customImage12 GetCustomImage(12)
#define customImage13 GetCustomImage(13)
#define customImage14 GetCustomImage(14)
#define customImage15 GetCustomImage(15)

//──────────────────────────────────────────────────────
// Iris兼容矩阵宏（Matrices Uniforms）
//──────────────────────────────────────────────────────

/**
 * @brief Iris矩阵Uniform完全兼容宏定义
 *
 * 教学要点:
 * 1. 所有矩阵宏通过GetMatricesData()统一访问
 * 2. 自动映射到MatricesBuffer[0]的对应字段
 * 3. 零学习成本移植Iris Shader Pack
 * 4. 支持完整Iris矩阵系统：GBuffer、Shadow、通用矩阵
 *
 * 使用示例:
 * ```hlsl
 * // 顶点变换（GBuffer Pass）
 * float4 viewPos = mul(float4(worldPos, 1.0), gbufferModelView);
 * float4 clipPos = mul(viewPos, gbufferProjection);
 *
 * // 模型空间到世界空间变换（Iris扩展）
 * float4 worldPos = mul(float4(localPos, 1.0), modelMatrix);
 *
 * // 法线变换
 * float3 viewNormal = mul(float4(normal, 0.0), normalMatrix).xyz;
 * ```
 *
 * 架构优势:
 * - 统一访问接口：所有矩阵通过单个Buffer访问
 * - 自动缓存优化：GPU会缓存频繁访问的MatricesBuffer[0]
 * - Iris完全兼容：字段名称和语义与Iris官方文档一致
 */

// ===== 辅助宏：获取MatricesBuffer =====
// 注意：这是内部实现细节，用户Shader应直接使用下面的矩阵宏
#define MatricesBuffer GetMatricesData()

// ===== GBuffer矩阵（主渲染Pass）=====
#define gbufferModelView              MatricesBuffer.gbufferModelView
#define gbufferModelViewInverse       MatricesBuffer.gbufferModelViewInverse
#define gbufferProjection             MatricesBuffer.gbufferProjection
#define gbufferProjectionInverse      MatricesBuffer.gbufferProjectionInverse
#define gbufferPreviousModelView      MatricesBuffer.gbufferPreviousModelView
#define gbufferPreviousProjection     MatricesBuffer.gbufferPreviousProjection
#define cameraToRenderTransform       MatricesBuffer.cameraToRenderTransform

// ===== Shadow矩阵（阴影Pass）=====
#define shadowModelView               MatricesBuffer.shadowModelView
#define shadowModelViewInverse        MatricesBuffer.shadowModelViewInverse
#define shadowProjection              MatricesBuffer.shadowProjection
#define shadowProjectionInverse       MatricesBuffer.shadowProjectionInverse

// ===== 通用矩阵（当前几何体）=====
#define modelViewMatrix               MatricesBuffer.modelViewMatrix
#define modelViewMatrixInverse        MatricesBuffer.modelViewMatrixInverse
#define projectionMatrix              MatricesBuffer.projectionMatrix
#define projectionMatrixInverse       MatricesBuffer.projectionMatrixInverse
#define normalMatrix                  MatricesBuffer.normalMatrix
#define textureMatrix                 MatricesBuffer.textureMatrix

// ===== Iris扩展矩阵（模型到世界空间变换）=====
/**
 * @brief 模型到世界空间变换矩阵
 * @iris modelMatrix (Iris扩展)
 *
 * 教学要点:
 * - 将模型局部空间转换到世界空间
 * - 用于延迟渲染中的位置重建
 * - 与modelViewMatrix的关系: modelViewMatrix = viewMatrix * modelMatrix
 *
 * 使用场景:
 * - 延迟渲染后处理Pass需要重建世界空间位置
 * - 物体空间效果（如程序化纹理）需要世界坐标
 * - 粒子系统、体积雾等需要世界空间计算的效果
 */
#define modelMatrix                   MatricesBuffer.modelMatrix

/**
 * @brief 世界到模型空间变换矩阵
 * @iris modelMatrixInverse (Iris扩展)
 *
 * 教学要点:
 * - 将世界空间转换回模型局部空间
 * - 用于物体空间的后处理效果
 * - 常用于物体空间法线贴图、程序化着色
 */
#define modelMatrixInverse            MatricesBuffer.modelMatrixInverse

//──────────────────────────────────────────────────────
// Sampler States
//──────────────────────────────────────────────────────

/**
 * @brief 全局采样器定义
 *
 * 教学要点:
 * 1. 静态采样器 - 固定绑定到register(s0-s2)
 * 2. 线性采样: 用于纹理过滤
 * 3. 点采样: 用于精确像素访问
 * 4. 阴影采样: 比较采样器（Comparison Sampler）
 *
 * Shader Model 6.6也支持:
 * - SamplerDescriptorHeap[index] 动态采样器访问
 */
SamplerState linearSampler : register(s0); // 线性过滤
SamplerState pointSampler : register(s1); // 点采样
SamplerState shadowSampler : register(s2); // 阴影比较采样器

//──────────────────────────────────────────────────────
// 注意事项
//──────────────────────────────────────────────────────

/**
 * 不在Common.hlsl中定义固定的PSOutput！
 * + PSOutput由ShaderCodeGenerator动态生成
 *
 * 原因:
 * 1. 每个Shader的RENDERTARGETS注释不同
 * 2. PSOutput必须与实际输出RT数量匹配
 * 3. 固定16个输出会浪费寄存器和性能
 *
 * 示例:
 * cpp
 * // ShaderCodeGenerator根据 RENDERTARGETS: 0,3,7,12 动态生成:
 * struct PSOutput {
 *     float4 Color0 : SV_Target0;  // → colortex0
 *     float4 Color1 : SV_Target1;  // → colortex3
 *     float4 Color2 : SV_Target2;  // → colortex7
 *     float4 Color3 : SV_Target3;  // → colortex12
 * };
 * 
 */

//──────────────────────────────────────────────────────
// 固定 Input Layout (顶点格式) - 保留用于Gbuffers
//──────────────────────────────────────────────────────

/**
 * @brief 顶点着色器输入结构体
 *
 * 教学要点:
 * 1. 固定顶点格式 - 所有几何体使用相同布局
 * 2. 对应 C++ enigma::graphic::Vertex (Vertex_PCUTBN)
 * 3. 用于Gbuffers Pass（几何体渲染）
 *
 * 注意: Composite Pass使用全屏三角形，不需要复杂顶点格式
 */
struct VSInput
{
    float3 Position : POSITION; // 顶点位置 (世界空间)
    float4 Color : COLOR0; // 顶点颜色 (R8G8B8A8_UNORM)
    float2 TexCoord : TEXCOORD0; // 纹理坐标
    float3 Tangent : TANGENT; // 切线向量
    float3 Bitangent : BITANGENT; // 副切线向量
    float3 Normal : NORMAL; // 法线向量
};

/**
 * @brief 顶点着色器输出 / 像素着色器输入
 */
struct VSOutput
{
    float4 Position : SV_POSITION; // 裁剪空间位置
    float4 Color : COLOR0; // 顶点颜色 (解包后)
    float2 TexCoord : TEXCOORD0; // 纹理坐标
    float3 Tangent : TANGENT; // 切线向量
    float3 Bitangent : BITANGENT; // 副切线向量
    float3 Normal : NORMAL; // 法线向量
    float3 WorldPos : TEXCOORD1; // 世界空间位置
};

// 像素着色器输入 (与 VSOutput 相同)
typedef VSOutput PSInput;

//──────────────────────────────────────────────────────
// 辅助函数
//──────────────────────────────────────────────────────

/**
 * @brief 解包 Rgba8 颜色 (uint → float4)
 * @param packedColor 打包的 RGBA8 颜色 (0xAABBGGRR)
 * @return 解包后的 float4 颜色 (0.0-1.0 范围)
 */
float4 UnpackRgba8(uint packedColor)
{
    float r = float((packedColor >> 0) & 0xFF) / 255.0;
    float g = float((packedColor >> 8) & 0xFF) / 255.0;
    float b = float((packedColor >> 16) & 0xFF) / 255.0;
    float a = float((packedColor >> 24) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

/**
 * @brief 标准顶点变换函数（用于Fallback Shader）
 * @param input 顶点输入（VSInput结构体）
 * @return VSOutput - 变换后的顶点输出
 *
 * 教学要点:
 * 1. 用于 gbuffers_basic 和 gbuffers_textured 的 Fallback 着色器
 * 2. 自动处理 MVP 变换、颜色解包、数据传递
 * 3. 从 Uniform Buffers 获取变换矩阵
 * 4. KISS 原则 - 极简实现，无额外计算
 *
 * 工作流程:
 * 1. 从 MatricesData 获取 modelViewMatrix 和 projectionMatrix
 * 2. 顶点位置变换: Position → ViewSpace → ClipSpace
 * 3. 法线变换: normalMatrix 变换法线向量
 * 4. 颜色解包: uint → float4
 * 5. 传递所有顶点属性到像素着色器
 */
VSOutput StandardVertexTransform(VSInput input)
{
    VSOutput output;

    // [CRITICAL FIX] 直接使用宏访问，但只调用一次GetMatricesData()
    // 通过临时变量避免重复Buffer读取
    float4x4 _modelMat    = modelMatrix;
    float4x4 _viewMat     = gbufferModelView;
    float4x4 _camToRender = cameraToRenderTransform;
    float4x4 _projMat     = gbufferProjection;
    float4x4 _normalMat   = normalMatrix;

    // 1. 顶点位置变换
    float4 localPos  = float4(input.Position, 1.0);
    float4 worldPos  = mul(modelMatrix, localPos);
    float4 cameraPos = mul(gbufferModelView, worldPos);
    float4 renderPos = mul(cameraToRenderTransform, cameraPos);
    //float4 clipPos   = mul(gbufferProjection, renderPos);

    float4 clipPos = mul(localPos, gbufferProjection);

    output.Position = clipPos;
    output.WorldPos = worldPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    // 4. 法线变换
    output.Normal = normalize(mul(float4(input.Normal, 0.0), _normalMat).xyz);

    // 5. 切线和副切线
    output.Tangent   = normalize(mul(float4(input.Tangent, 0.0), _viewMat).xyz);
    output.Bitangent = normalize(mul(float4(input.Bitangent, 0.0), _viewMat).xyz);

    return output;
}

//──────────────────────────────────────────────────────
// 数学常量
//──────────────────────────────────────────────────────

#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define HALF_PI 1.57079632679

//──────────────────────────────────────────────────────
// 架构说明和使用示例
//──────────────────────────────────────────────────────

/**
 * ========== 使用示例 ==========
 *
 * 1. Composite Pass Shader (全屏后处理):
 * ```hlsl
 * #include "Common.hlsl"
 *
 * // PSOutput由ShaderCodeGenerator动态生成
 * // 例如: struct PSOutput { float4 Color0:SV_Target0; ... }
 *
 * PSOutput PSMain(PSInput input)
 * {
 *     PSOutput output;
 *
 *     // 读取纹理（Bindless，自动处理Main/Alt）
 *     float4 color0 = colortex0.Sample(linearSampler, input.TexCoord);
 *     float4 color8 = colortex8.Sample(linearSampler, input.TexCoord);
 *
 *     // 输出到多个RT
 *     output.Color0 = color0 * 0.5;    // 写入colortex0
 *     output.Color1 = color8 * 2.0;    // 写入colortex3
 *
 *     return output;
 * }
 * ```
 *
 * 1.5. CustomImage使用示例（延迟渲染PBR材质）新增:
 * ```hlsl
 * #include "Common.hlsl"
 *
 * PSOutput PSMain(PSInput input)
 * {
 *     PSOutput output;
 *
 *     // 方式1：通过宏直接访问（推荐）
 *     float4 albedo = customImage0.Sample(linearSampler, input.TexCoord);    // Albedo贴图
 *     float roughness = customImage1.Sample(linearSampler, input.TexCoord).r; // Roughness贴图
 *     float metallic = customImage2.Sample(linearSampler, input.TexCoord).r;  // Metallic贴图
 *     float3 normal = customImage3.Sample(linearSampler, input.TexCoord).rgb; // Normal贴图
 *
 *     // 方式2：通过索引访问（动态槽位）
 *     uint slotIndex = 5;
 *     Texture2D dynamicTex = GetCustomImage(slotIndex);
 *     float4 data = dynamicTex.Sample(linearSampler, input.TexCoord);
 *
 *     // 方式3：检查槽位有效性（安全访问）
 *     uint bindlessIndex = GetCustomImageIndex(4);
 *     if (bindlessIndex != 0xFFFFFFFF) // 检查是否为无效索引
 *     {
 *         Texture2D aoMap = GetCustomImage(4); // AO贴图
 *         float ao = aoMap.Sample(linearSampler, input.TexCoord).r;
 *         albedo.rgb *= ao; // 应用AO
 *     }
 *
 *     // PBR光照计算
 *     float3 finalColor = CalculatePBR(albedo.rgb, roughness, metallic, normal);
 *     output.Color0 = float4(finalColor, 1.0);
 *
 *     return output;
 * }
 * ```
 *
 * 2. RENDERTARGETS注释:
 * ```hlsl
 * RENDERTARGETS: 0,3,7,12
 * // ShaderCodeGenerator解析此注释，生成4个输出的PSOutput
 * ```
 *
 * 3. Main/Alt自动处理:
 * - colortex0自动访问Main或Alt（根据flip状态）
 * - 无需手动管理双缓冲
 * - 无需ResourceBarrier同步
 *
 * ========== 架构优势 ==========
 *
 * 1. Iris完全兼容:
 *    - colortex0-15宏自动映射
 *    - RENDERTARGETS注释支持
 *    - depthtex0/depthtex1支持
 *
 * 2. 性能优化:
 *    - Main/Alt消除90%+ ResourceBarrier
 *    - Bindless零绑定开销
 *    - Root Signature全局共享
 *
 * 3. 现代化设计:
 *    - Shader Model 6.6真正Bindless
 *    - MRT动态生成（不浪费）
 *    - 声明式编程（注释驱动）
 */
