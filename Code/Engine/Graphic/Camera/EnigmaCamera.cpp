#include "EnigmaCamera.hpp"
#include "Engine/Math/MathUtils.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数
    // ========================================================================

    EnigmaCamera::EnigmaCamera(const CameraCreateInfo& createInfo)
        : m_createInfo(createInfo)
    {
        // 构造函数只保存配置，不执行任何GPU操作
        // 符合纯粹数据载体的设计原则
    }

    EnigmaCamera::EnigmaCamera()
        : m_createInfo(CameraCreateInfo::CreatePerspective())
    {
        // 默认构造函数创建标准透视相机
    }

    // ========================================================================
    // 矩阵计算 - 核心数学功能
    // ========================================================================

    Mat44 EnigmaCamera::GetWorldToCameraTransform() const
    {
        Mat44 cameraToWorld = GetCameraToWorldTransform().GetInverse();
        return cameraToWorld;
    }

    Mat44 EnigmaCamera::GetCameraToWorldTransform() const
    {
        Mat44 cameraToWorld = Mat44::MakeTranslation3D(m_createInfo.position);
        cameraToWorld.Append(Mat44::MakeZRotationDegrees(m_createInfo.orientation.m_yawDegrees));
        cameraToWorld.Append(Mat44::MakeXRotationDegrees(m_createInfo.orientation.m_rollDegrees));
        cameraToWorld.Append(Mat44::MakeYRotationDegrees(m_createInfo.orientation.m_pitchDegrees));
        return cameraToWorld;
    }

    Mat44 EnigmaCamera::GetProjectionMatrix() const
    {
        // 实时计算投影矩阵
        if (m_createInfo.mode == CameraMode::Perspective)
        {
            return Mat44::MakePerspectiveProjection(
                m_createInfo.perspectiveFOV,
                m_createInfo.perspectiveAspect,
                m_createInfo.perspectiveNear,
                m_createInfo.perspectiveFar
            );
        }
        else
        {
            Vec2 size   = m_createInfo.orthographicTopRight - m_createInfo.orthographicBottomLeft;
            Vec2 center = (m_createInfo.orthographicTopRight + m_createInfo.orthographicBottomLeft) * 0.5f;

            Mat44 projection = Mat44::MakeOrthoProjection(
                -size.x * 0.5f,
                size.x * 0.5f,
                -size.y * 0.5f,
                size.y * 0.5f,
                m_createInfo.orthographicNear,
                m_createInfo.orthographicFar
            );

            if (center.x != 0.0f || center.y != 0.0f)
            {
                Mat44 translation = Mat44::MakeTranslation3D(Vec3(-center.x, -center.y, 0.0f));
                projection.Append(translation);
            }

            return projection;
        }
    }

    Mat44 EnigmaCamera::GetViewProjectionMatrix() const
    {
        // ViewProjection = Projection * View
        Mat44 viewProjection = GetProjectionMatrix();
        viewProjection.Append(GetWorldToCameraTransform());
        return viewProjection;
    }

    // ========================================================================
    // 配置更新
    // ========================================================================

    void EnigmaCamera::UpdateCreateInfo(const CameraCreateInfo& createInfo)
    {
        m_createInfo = createInfo;
    }

    void EnigmaCamera::SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation)
    {
        m_createInfo.position    = position;
        m_createInfo.orientation = orientation;
    }

    void EnigmaCamera::SetPosition(const Vec3& position)
    {
        m_createInfo.position = position;
    }

    void EnigmaCamera::SetOrientation(const EulerAngles& orientation)
    {
        m_createInfo.orientation = orientation;
    }

    // ========================================================================
    // 实用功能
    // ========================================================================

    Vec3 EnigmaCamera::GetForwardVector() const
    {
        // 基于欧拉角计算前问向量
        // 坐标系约定：x forward +y left +z up（右手坐标系，forward is +Y）
        // 使用与Camera.cpp相同的旋转顺序构造矩阵

        Mat44 rotation = Mat44::MakeZRotationDegrees(m_createInfo.orientation.m_yawDegrees);
        rotation.Append(Mat44::MakeXRotationDegrees(m_createInfo.orientation.m_rollDegrees));
        rotation.Append(Mat44::MakeYRotationDegrees(m_createInfo.orientation.m_pitchDegrees));
        return rotation.GetJBasis3D(); // +Y方向（forward）
    }

    Vec3 EnigmaCamera::GetRightVector() const
    {
        // 基于欧拉角计算右向量
        // 坐标系约定：x forward +y left +z up（右手坐标系，right is +X）
        // 注意：由于y轴指向左侧，所以右向量实际上是+X方向
        // 使用与Camera.cpp相同的旋转顺序构造矩阵

        Mat44 rotation = Mat44::MakeZRotationDegrees(m_createInfo.orientation.m_yawDegrees);
        rotation.Append(Mat44::MakeXRotationDegrees(m_createInfo.orientation.m_rollDegrees));
        rotation.Append(Mat44::MakeYRotationDegrees(m_createInfo.orientation.m_pitchDegrees));
        return rotation.GetIBasis3D(); // +X方向（right）
    }

    Vec3 EnigmaCamera::GetUpVector() const
    {
        // 基于欧拉角计算上向量
        // 坐标系约定：x forward +y left +z up（右手坐标系，up is +Z）
        // 使用与Camera.cpp相同的旋转顺序构造矩阵

        Mat44 rotation = Mat44::MakeZRotationDegrees(m_createInfo.orientation.m_yawDegrees);
        rotation.Append(Mat44::MakeXRotationDegrees(m_createInfo.orientation.m_rollDegrees));
        rotation.Append(Mat44::MakeYRotationDegrees(m_createInfo.orientation.m_pitchDegrees));
        return rotation.GetKBasis3D(); // +Z方向（up）
    }

    Vec2 EnigmaCamera::WorldToScreen(const Vec3& worldPos, const Vec2& clientSize) const
    {
        // 1. 世界坐标 -> 相机坐标
        Vec3 cameraPos = GetWorldToCameraTransform().TransformPosition3D(worldPos);

        // 2. 相机坐标 -> 裁剪坐标
        Vec4 clipPos = GetProjectionMatrix().TransformHomogeneous3D(Vec4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f));

        // 3. 裁剪坐标 -> NDC坐标
        if (clipPos.w <= 0.0f)
        {
            return Vec2(-1.0f, -1.0f); // 在相机后面
        }

        Vec2 ndcPos = Vec2(clipPos.x / clipPos.w, clipPos.y / clipPos.w);

        // 4. NDC坐标 -> 视口坐标
        AABB2 viewport    = GetViewport();
        Vec2  viewportPos = viewport.GetPointAtUV(ndcPos);

        // 5. 视口坐标 -> 屏幕坐标
        return Vec2(viewportPos.x * clientSize.x, viewportPos.y * clientSize.y);
    }
} // namespace enigma::graphic
