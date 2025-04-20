#pragma once
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"


enum Mode
{
    eMode_Orthographic,
    eMode_Perspective,
    eMode_Count
};

class Camera
{
public:
    /// DirectX

    // SetOrthographicView and SetPerspectiveView set the camera mode appropriately and store the parameter values.
    void SetOrthographicView(Vec2 const& bottomLeft, Vec2 const& topRight, float near = 0.0f, float far = 1.0f);
    void SetPerspectiveView(float aspect, float fov, float near, float far);

    void SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation);
    void SetPosition(const Vec3& position);

    Vec3 GetPosition() const;

    void        SetOrientation(const EulerAngles& orientation);
    EulerAngles GetOrientation() const;

    /// returns the equivalent of the camera model to world transform using the position and orientation of the camera.
    /// @return camera model to world transform
    Mat44 GetCameraToWorldTransform() const;
    /// returns the inverse of the GetCameraToWorldTransform()
    /// @return inverse of the GetCameraToWorldTransform()
    Mat44 GetWorldToCameraTransform() const;

    void SetCameraToRenderTransform(const Mat44& m);
    /// returns our game conventions to DirectX conventions transform, which must be supplied by game code when configuring
    /// the camera.
    /// @return game conventions to DirectX conventions transform
    Mat44 GetCameraToRenderTransform() const;

    /// returns the projection matrix.
    /// @return projection matrix
    Mat44 GetRenderToClipTransform() const;

    Vec2 GetOrthographicBottomLeft() const;
    Vec2 GetOrthographicTopRight() const;
    void Translate2D(Vec2 const& translation);

    /// GetOrthographicMatrix and GetPerspectiveMatrix build and return orthographic and perspective matrices,
    /// respectively, using the stored values set previously.
    /// @return orthographic and perspective matrices
    Mat44 GetOrthographicMatrix() const;
    Mat44 GetPerspectiveMatrix() const;
    Mat44 GetProjectionMatrix() const;
    /// 
    [[deprecated("SetOrthoView() deprecated | Use SetOrthographicView() instead.")]]
    void SetOrthoView(const Vec2& bottomLeft, const Vec2& topRight);
    [[deprecated("GetOrthoBottomLeft() deprecated | Use GetOrthographicBottomLeft() instead.")]]
    Vec2 GetOrthoBottomLeft() const;
    [[deprecated("GetOrthoTopRight() deprecated | Use GetOrthographicTopRight() instead.")]]
    Vec2 GetOrthoTopRight() const;

    void Update(float deltaTime);
    //void Translate2D(const Vec2& translation2D);
    void DoShakeEffect(const Vec2& translation2D, float duration, bool decreaseShakeOverTime = true);

    /// Viewport
    void  SetNormalizedViewport(AABB2 const& viewportSize); // Set the normalized viewport from zero to one
    AABB2 GetNormalizedViewport() const; // Get the normalized viewport from zero to one
    /// Get the unnormalized view port based on client size, for example, if we have the client dimension 1600 x 800
    /// and the normalized view port mins = (0, 0.5f) maxs = (1, 1), the function should return dimension of
    /// mins = (0, 400) maxs = (1600, 800)
    /// @param clientSize the desired client size for the application.
    /// @return AABB2 The unnormalized screen viewport size.
    AABB2 GetViewPortUnnormalized(Vec2 const& clientSize);
    float GetViewPortAspectRatio(Vec2 const& clientSize) const; // Get the normalized viewport aspect ratio
    Vec2  GetViewportSize(Vec2 const& clientSize) const; // Get the dimension of screen viewport size after client size was input.
    float GetViewPortUnnormalizedAspectRatio(Vec2 const& clientSize) const; // Get the unnormalized screen port aspect ratio.

private:
    void ApplyShakeEffect(float deltaTime);
    Vec2 GenerateRandomShakeOffset(const Vec2& translation2D) const;

    Vec2 m_bottomLeft;
    Vec2 m_topRight;

    // Post-processing
    Vec2 m_postBottomLeft;
    Vec2 m_postTopRight;
    bool m_isPostProcessing = false;

    // Shake effect
    float m_shakeTotalTime     = 0.f;
    float m_shakeRemainingTime = 0.f;
    bool  m_isShaking          = false;
    Vec2  m_shakeTranslation;
    bool  m_decreaseShakeOverTime = false;

public:
    /// DirectX
    Mode m_mode = eMode_Orthographic;

    Vec3        m_position;
    EulerAngles m_orientation;

    Vec2  m_orthographicBottomLeft;
    Vec2  m_orthographicTopRight;
    float m_orthographicNear = 0.f;
    float m_orthographicFar  = 1.f;

    float m_perspectiveAspect = 0.f;
    float m_perspectiveFOV    = 0.f;
    float m_perspectiveNear   = 0.f;
    float m_perspectiveFar    = 0.f;

    Mat44 m_cameraToRenderTransform;

    /// View port

    // This view port is start form bottom left (0, 0) to top right with normalized
    AABB2 m_viewPort = AABB2(Vec2(0, 0), Vec2(1, 1));
};
