#pragma once

#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"

namespace enigma::graphic
{
    /**
     * @brief Camera/Player Uniforms - 相机和玩家模型数据
     *
     * Iris官方文档参考:
     * https://shaders.properties/current/reference/uniforms/camera/
     *
     * 教学要点:
     * 1. 此结构体对应Iris "Camera and Player Model" Uniform分类
     * 2. 存储在GPU StructuredBuffer，通过cameraAndPlayerBufferIndex访问
     * 3. 使用alignas确保与HLSL对齐要求一致
     * 4. 所有字段名称、类型、语义与Iris官方文档完全一致
     *
     * HLSL访问示例:
     * ```hlsl
     * StructuredBuffer<CameraAndPlayerUniforms> cameraPlayerBuffer =
     *     ResourceDescriptorHeap[cameraAndPlayerBufferIndex];
     * float3 camPos = cameraPlayerBuffer[0].cameraPosition;
     * float altitude = cameraPlayerBuffer[0].eyeAltitude;
     * ```
     *
     * @note alignas(16)用于Vec3确保16字节对齐（HLSL float3在StructuredBuffer中16字节对齐）
     * @note alignas(8)用于ivec2/vec2确保8字节对齐
     * @note alignas(4)用于float确保4字节对齐
     */
#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因alignas而填充 - 预期行为
    struct CameraAndPlayerUniforms
    {
        /**
         * @brief 相机在世界空间的位置
         * @type vec3
         * @iris cameraPosition
         *
         * 教学要点:
         * - 类似游戏内坐标，但因精度原因会周期性重置
         * - Iris: 每30000方块或传送超过1000方块时重置
         * - OptiFine: 每1000024方块重置
         */
        alignas(16) Vec3 cameraPosition;

        /**
         * @brief 玩家高度（Y坐标）
         * @type float
         * @iris eyeAltitude
         *
         * 教学要点: 等价于 cameraPosition.y，与F3屏幕显示值一致
         */
        alignas(4) float eyeAltitude;

        /**
         * @brief 相机位置的小数部分
         * @type vec3
         * @iris cameraPositionFract
         * @tag Iris Exclusive
         * @range [0,1)
         *
         * 教学要点: 与cameraPositionInt配合使用，避免精度误差
         */
        alignas(16) Vec3 cameraPositionFract;

        /**
         * @brief 相机位置的整数部分
         * @type ivec3
         * @iris cameraPositionInt
         * @tag Iris Exclusive
         *
         * 教学要点: 不会像cameraPosition那样偏移
         */
        alignas(16) IntVec3 cameraPositionInt;

        /**
         * @brief 上一帧的相机位置
         * @type vec3
         * @iris previousCameraPosition
         *
         * 教学要点: 用于运动模糊、TAA等需要历史帧数据的效果
         */
        alignas(16) Vec3 previousCameraPosition;

        /**
         * @brief 上一帧的相机位置小数部分
         * @type vec3
         * @iris previousCameraPositionFract
         * @tag Iris Exclusive
         * @range [0,1)
         */
        alignas(16) Vec3 previousCameraPositionFract;

        /**
         * @brief 上一帧的相机位置整数部分
         * @type ivec3
         * @iris previousCameraPositionInt
         * @tag Iris Exclusive
         */
        alignas(16) IntVec3 previousCameraPositionInt;

        /**
         * @brief 玩家头部模型在世界空间的位置
         * @type vec3
         * @iris eyePosition
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 第一人称: 等价于cameraPosition
         * - 第三人称: 与cameraPosition不同（相机和头部分离）
         */
        alignas(16) Vec3 eyePosition;

        /**
         * @brief 玩家头部到相机的世界空间偏移
         * @type vec3
         * @iris relativeEyePosition
         * @tag Iris Exclusive
         *
         * 教学要点: 计算公式为 cameraPosition - eyePosition
         */
        alignas(16) Vec3 relativeEyePosition;

        /**
         * @brief 玩家身体朝向的世界对齐方向
         * @type vec3
         * @iris playerBodyVector
         * @tag Iris Exclusive
         *
         * 教学要点: ⚠️ 当前Iris实现有bug，读取值与playerLookVector相同
         */
        alignas(16) Vec3 playerBodyVector;

        /**
         * @brief 玩家头部朝向的世界对齐方向
         * @type vec3
         * @iris playerLookVector
         * @tag Iris Exclusive
         *
         * 教学要点:
         * - 不受游泳等动画影响
         * - 第一人称和后视第三人称: 等价于相机朝向
         * - 前视第三人称: 与相机朝向相反
         */
        alignas(16) Vec3 playerLookVector;

        /**
         * @brief 视图空间中的上方向向量
         * @type vec3
         * @iris upPosition
         * @range 长度为100
         *
         * 教学要点:
         * - 可从 gbufferModelView[1].xyz 获取
         * - 长度固定为100 (归一化值 * 100)
         */
        alignas(16) Vec3 upPosition;

        /**
         * @brief 玩家位置的光照值
         * @type ivec2
         * @iris eyeBrightness
         * @range [0, 240]
         *
         * 教学要点:
         * - x: 方块光（火把、熔岩等）
         * - y: 天空光（太阳、月亮）
         */
        alignas(8) IntVec2 eyeBrightness;

        /**
         * @brief 平滑后的玩家位置光照值
         * @type ivec2
         * @iris eyeBrightnessSmooth
         * @range [0, 240]
         *
         * 教学要点:
         * - eyeBrightness随时间平滑，避免光照突变
         * - 平滑速度可通过eyeBrightnessHalflife控制
         */
        alignas(8) IntVec2 eyeBrightnessSmooth;

        /**
         * @brief 屏幕中心深度值（平滑）
         * @type float
         * @iris centerDepthSmooth
         *
         * 教学要点:
         * - 从 depthtex0 屏幕中心采样，随时间平滑
         * - 平滑速度可通过centerDepthHalflife控制
         * - 用于自动对焦等效果
         */
        alignas(4) float centerDepthSmooth;

        /**
         * @brief 是否为第一人称视角
         * @type bool
         * @iris firstPersonCamera
         *
         * 教学要点:
         * - true: 第一人称
         * - false: 任何第三人称视角
         */
        alignas(4) bool firstPersonCamera;

        /**
         * @brief 默认构造函数 - 初始化为合理默认值
         */
        CameraAndPlayerUniforms()
            : cameraPosition(Vec3::ZERO)
              , eyeAltitude(0.0f)
              , cameraPositionFract(Vec3::ZERO)
              , cameraPositionInt(IntVec3::ZERO)
              , previousCameraPosition(Vec3::ZERO)
              , previousCameraPositionFract(Vec3::ZERO)
              , previousCameraPositionInt(IntVec3::ZERO)
              , eyePosition(Vec3::ZERO)
              , relativeEyePosition(Vec3::ZERO)
              , playerBodyVector(Vec3::ZERO)
              , playerLookVector(Vec3::ZERO)
              , upPosition(Vec3(0.0f, 100.0f, 0.0f)) // 默认Y轴正方向，长度100
              , eyeBrightness(IntVec2::ZERO)
              , eyeBrightnessSmooth(IntVec2::ZERO)
              , centerDepthSmooth(0.0f)
              , firstPersonCamera(true)
        {
        }
    };
#pragma warning(pop)

    // 编译期验证: 检查结构体大小合理性（不超过512字节）
    static_assert(sizeof(CameraAndPlayerUniforms) <= 512,
                  "CameraAndPlayerUniforms too large, consider optimization");
} // namespace enigma::graphic
