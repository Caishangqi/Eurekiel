#include "YClampedGradientDensityFunction.hpp"

#include "Engine/Math/MathUtils.hpp"
using namespace enigma::voxel;

YClampedGradientDensityFunction::YClampedGradientDensityFunction(int fromY, int toY, float fromValue, float toValue) : m_fromY(fromY), m_toY(toY), m_fromValue(fromValue), m_toValue(toValue)
{
}

float YClampedGradientDensityFunction::Evaluate(int x, int y, int z) const
{
    // Step 1: Clamp Y to within the range
    int clampedY = (int)GetClamped((float)y, (float)m_fromY, (float)m_toY);

    // Step 2: Linear interpolation
    if (m_toY == m_fromY)
    {
        return m_fromValue;
    }

    float t = (float)(clampedY - m_fromY) / (float)(m_toY - m_fromY);
    return Interpolate(m_fromValue, m_toValue, t);
}

std::string YClampedGradientDensityFunction::GetTypeName() const
{
    return "engine:y_clamped_gradient";
}
