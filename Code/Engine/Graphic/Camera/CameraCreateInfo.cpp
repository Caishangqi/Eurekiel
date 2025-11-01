#include "CameraCreateInfo.hpp"
using namespace enigma::graphic;

CameraCreateInfo CameraCreateInfo::CreatePerspective(const Vec3& pos, const EulerAngles& orient, float aspect, float fov, float nearPlane, float farPlane)
{
    CameraCreateInfo info;
    info.mode              = CameraMode::Perspective;
    info.position          = pos;
    info.orientation       = orient;
    info.perspectiveAspect = aspect;
    info.perspectiveFOV    = fov;
    info.perspectiveNear   = nearPlane;
    info.perspectiveFar    = farPlane;
    return info;
}

CameraCreateInfo CameraCreateInfo::CreateOrthographic(const Vec3& pos, const EulerAngles& orient, const Vec2& bottomLeft, const Vec2& topRight, float nearPlane, float farPlane)

{
    CameraCreateInfo info;
    info.mode                   = CameraMode::Orthographic;
    info.position               = pos;
    info.orientation            = orient;
    info.orthographicBottomLeft = bottomLeft;
    info.orthographicTopRight   = topRight;
    info.orthographicNear       = nearPlane;
    info.orthographicFar        = farPlane;
    return info;
}

CameraCreateInfo CameraCreateInfo::CreateUI2D(const Vec2& screenSize, float nearPlane, float farPlane)

{
    CameraCreateInfo info;
    info.mode                   = CameraMode::Orthographic;
    info.position               = Vec3::ZERO;
    info.orientation            = EulerAngles::ZERO;
    info.orthographicBottomLeft = Vec2::ZERO;
    info.orthographicTopRight   = screenSize;
    info.orthographicNear       = nearPlane;
    info.orthographicFar        = farPlane;
    return info;
}
