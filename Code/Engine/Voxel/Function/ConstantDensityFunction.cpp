#include "ConstantDensityFunction.hpp"

#include "Engine/Core/EngineCommon.hpp"
using namespace enigma::voxel;

ConstantDensityFunction::ConstantDensityFunction(float value) : m_value(value)
{
}

float ConstantDensityFunction::Evaluate(int x, int y, int z) const
{
    UNUSED(x);
    UNUSED(y);
    UNUSED(z);
    return m_value;
}

std::unique_ptr<ConstantDensityFunction> ConstantDensityFunction::FromJson(const Json& json)
{
    // JSON format 1: abbreviation - directly a number
    if (json.is_number())
    {
        return std::make_unique<ConstantDensityFunction>(json.get<float>());
    }

    // JSON format 2: complete object
    // { "type": "engine:constant", "argument": 0.5 }
    return std::make_unique<ConstantDensityFunction>(
        json["argument"].get<float>()
    );
}

std::string ConstantDensityFunction::GetTypeName() const
{
    return "engine:constant";
}
