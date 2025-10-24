#pragma once
#include "DensityFunction.hpp"

namespace enigma::voxel
{
    class AddDensityFunction : public DensityFunction
    {
    public:
        AddDensityFunction(std::unique_ptr<DensityFunction> arg1, std::unique_ptr<DensityFunction> arg2);

        float       Evaluate(int x, int y, int z) const override;
        std::string GetTypeName() const override;

    private:
        std::unique_ptr<DensityFunction> m_argument1;
        std::unique_ptr<DensityFunction> m_argument2;
    };

    class MultiplyDensityFunction : public DensityFunction
    {
    public:
        MultiplyDensityFunction(std::unique_ptr<DensityFunction> arg1, std::unique_ptr<DensityFunction> arg2);

        float       Evaluate(int x, int y, int z) const override;
        std::string GetTypeName() const override;

    private:
        std::unique_ptr<DensityFunction> m_argument1;
        std::unique_ptr<DensityFunction> m_argument2;
    };
}
