#pragma once

struct Mat44;
struct Vec3;

struct EulerAngles
{
    static EulerAngles ZERO;

    EulerAngles() = default;
    EulerAngles(float yawDegrees, float pitchDegrees, float rollDegrees);
    explicit EulerAngles(const Vec3& vec3);
    void     GetAsVectors_IFwd_JLeft_KUp(Vec3& out_forwardIBasis, Vec3& out_leftJBasis, Vec3& out_upKBasis) const;
    Mat44    GetAsMatrix_IFwd_JLeft_KUp() const;

    EulerAngles operator*(float value) const;
    EulerAngles operator+(const EulerAngles& value) const;
    //EulerAngles& operator=(const Vec3& value) const;

    float m_yawDegrees   = 0.f;
    float m_pitchDegrees = 0.f;
    float m_rollDegrees  = 0.f;
};
