#pragma once
#include "Engine/Core/Json.hpp"

namespace enigma::voxel
{
    class DensityFunction
    {
    public:
        virtual ~DensityFunction() = default;

        // Core Evaluate Function, giving 3D coords, return density value
        virtual float Evaluate(int x, int y, int z) const = 0;

        // Data Driven
        static std::unique_ptr<DensityFunction> FromJson(const core::Json& json);

        // Get Function MIN / MAX possible value
        virtual float GetMinValue() const { return std::numeric_limits<float>::lowest(); }
        virtual float GetMaxValue() const { return std::numeric_limits<float>::max(); }

        // Debugging Name
        virtual std::string GetTypeName() const = 0;
    };
}
