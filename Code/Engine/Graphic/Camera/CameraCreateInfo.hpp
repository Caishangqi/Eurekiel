#pragma once
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/Vec3.hpp"

namespace enigma::graphic
{
    enum class CameraMode
    {
        Perspective = 0,
        Orthographic = 1
    };

    /**
     * @brief EnigmaCamera创建信息结构
     *
     * 教学要点：
     * 1. 提供完整的相机配置参数
     * 2. 默认值基于常见使用场景
     * 3. 支持透视和正交两种投影模式
     */
    struct CameraCreateInfo
    {
        // 投影模式 - 使用现有枚举
        CameraMode mode = CameraMode::Perspective;

        // 位置和朝向
        Vec3        position    = Vec3::ZERO;
        EulerAngles orientation = EulerAngles();

        // 透视投影参数
        float perspectiveAspect = 16.0f / 9.0f; // 默认16:9
        float perspectiveFOV    = 60.0f; // 默认60度
        float perspectiveNear   = 0.1f;
        float perspectiveFar    = 1000.0f;

        // 正交投影参数
        Vec2  orthographicBottomLeft = Vec2(-1.0f, -1.0f);
        Vec2  orthographicTopRight   = Vec2(1.0f, 1.0f);
        float orthographicNear       = 0.0f;
        float orthographicFar        = 1.0f;

        // 视口参数（标准化坐标 0-1）
        AABB2 viewport = AABB2(Vec2(0, 0), Vec2(1, 1));

        // 相机到渲染坐标系的转换矩阵（游戏坐标系到DirectX坐标系）
        Mat44 cameraToRenderTransform = Mat44::IDENTITY;

        // 默认构造函数 - 创建标准透视相机
        CameraCreateInfo() = default;

        // 便利构造函数 - 透视相机
        static CameraCreateInfo CreatePerspective(
            const Vec3&        pos       = Vec3(0, 0, 5),
            const EulerAngles& orient    = EulerAngles::ZERO,
            float              aspect    = 16.0f / 9.0f,
            float              fov       = 60.0f,
            float              nearPlane = 0.1f,
            float              farPlane  = 1000.0f);


        // 便利构造函数 - 正交相机
        static CameraCreateInfo CreateOrthographic(
            const Vec3&        pos        = Vec3::ZERO,
            const EulerAngles& orient     = EulerAngles::ZERO,
            const Vec2&        bottomLeft = Vec2(-1.0f, -1.0f),
            const Vec2&        topRight   = Vec2(1.0f, 1.0f),
            float              nearPlane  = 0.0f,
            float              farPlane   = 1.0f);

        // 便利构造函数 - 2D UI相机
        static CameraCreateInfo CreateUI2D(
            const Vec2& screenSize = Vec2(1920, 1080),
            float       nearPlane  = 0.0f,
            float       farPlane   = 1.0f);
    };
}
