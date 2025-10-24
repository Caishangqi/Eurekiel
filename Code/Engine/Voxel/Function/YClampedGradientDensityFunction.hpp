#pragma once
#include "DensityFunction.hpp"

namespace enigma::voxel
{
    class YClampedGradientDensityFunction : public DensityFunction
    {
    public:
        YClampedGradientDensityFunction(
            int   fromY, int       toY,
            float fromValue, float toValue
        );

        float       Evaluate(int x, int y, int z) const override;
        std::string GetTypeName() const override;

    private:
        int   m_fromY; // Starting Y coordinate
        int   m_toY; //End Y coordinate
        float m_fromValue; // starting value
        float m_toValue; // end value
    };
}
