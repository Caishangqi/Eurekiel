#pragma once
#include "DensityFunction.hpp"
using namespace enigma::core;

namespace enigma::voxel
{
    class ConstantDensityFunction : public DensityFunction
    {
    public:
        ConstantDensityFunction(float value);
        float Evaluate(int x, int y, int z) const override;

        static std::unique_ptr<ConstantDensityFunction> FromJson(const Json& json);

        std::string GetTypeName() const override;

    private:
        float m_value = 0.f;
    };
}
