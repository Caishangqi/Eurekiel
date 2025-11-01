#include "EulerAngles.hpp"
#include "Mat44.hpp"
#include "MathUtils.hpp"
#include "Vec3.hpp"
#include "Engine/Core/EngineCommon.hpp"

EulerAngles EulerAngles::ZERO = EulerAngles(0, 0, 0);

EulerAngles::EulerAngles(float yawDegrees, float pitchDegrees, float rollDegrees) : m_yawDegrees(yawDegrees), m_pitchDegrees(pitchDegrees), m_rollDegrees(rollDegrees)
{
}

EulerAngles::EulerAngles(const Vec3& vec3) : m_yawDegrees(vec3.x), m_pitchDegrees(vec3.y), m_rollDegrees(vec3.z)
{
}

void EulerAngles::GetAsVectors_IFwd_JLeft_KUp(Vec3& out_forwardIBasis, Vec3& out_leftJBasis, Vec3& out_upKBasis) const
{
    float sy = SinDegrees(m_yawDegrees);
    float cr = CosDegrees(m_rollDegrees);
    float cy = CosDegrees(m_yawDegrees);
    float cp = CosDegrees(m_pitchDegrees);
    float sp = SinDegrees(m_pitchDegrees);
    float sr = SinDegrees(m_rollDegrees);

    out_forwardIBasis.x = cy * cp;
    out_forwardIBasis.y = sy * cp;
    out_forwardIBasis.z = -sp;

    out_leftJBasis.x = cy * sp * sr - sy * cr;
    out_leftJBasis.y = sy * sp * sr + cy * cr;
    out_leftJBasis.z = cp * sr;

    out_upKBasis.x = cy * sp * cr + sy * sr;
    out_upKBasis.y = sy * sp * cr - cy * sr;
    out_upKBasis.z = cp * cr;
}

Mat44 EulerAngles::GetAsMatrix_IFwd_JLeft_KUp() const
{
    float sy = SinDegrees(m_yawDegrees);
    float cr = CosDegrees(m_rollDegrees);
    float cy = CosDegrees(m_yawDegrees);
    float cp = CosDegrees(m_pitchDegrees);
    float sp = SinDegrees(m_pitchDegrees);
    float sr = SinDegrees(m_rollDegrees);

    Mat44 result;

    result.m_values[Mat44::Ix] = cy * cp;
    result.m_values[Mat44::Iy] = sy * cp;
    result.m_values[Mat44::Iz] = -sp;

    result.m_values[Mat44::Jx] = cy * sp * sr - sy * cr;
    result.m_values[Mat44::Jy] = sy * sp * sr + cy * cr;
    result.m_values[Mat44::Jz] = cp * sr;

    result.m_values[Mat44::Kx] = cy * sp * cr + sy * sr;
    result.m_values[Mat44::Ky] = sy * sp * cr - cy * sr;
    result.m_values[Mat44::Kz] = cp * cr;

    return result;
}

EulerAngles EulerAngles::operator*(float value) const
{
    EulerAngles result;
    result.m_pitchDegrees = m_pitchDegrees * value;
    result.m_yawDegrees   = m_yawDegrees * value;
    result.m_rollDegrees  = m_rollDegrees * value;
    return result;
}

EulerAngles EulerAngles::operator+(const EulerAngles& value) const
{
    EulerAngles result;
    result.m_pitchDegrees = m_pitchDegrees + value.m_pitchDegrees;
    result.m_rollDegrees  = m_rollDegrees + value.m_rollDegrees;
    result.m_yawDegrees   = m_yawDegrees + value.m_yawDegrees;
    return result;
}

/*EulerAngles& EulerAngles::operator=(const Vec3& value) const
{
    EulerAngles result;
    result.m_yawDegrees   = value.x;
    result.m_pitchDegrees = value.y;
    result.m_rollDegrees  = value.z;
    return result;
}*/
