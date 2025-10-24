#include "BinaryOperationDensityFunction.hpp"
using namespace enigma::voxel;

AddDensityFunction::AddDensityFunction(std::unique_ptr<DensityFunction> arg1, std::unique_ptr<DensityFunction> arg2) : m_argument1(std::move(arg1)), m_argument2(std::move(arg2))
{
}

float AddDensityFunction::Evaluate(int x, int y, int z) const
{
    float val1 = m_argument1->Evaluate(x, y, z);
    float val2 = m_argument2->Evaluate(x, y, z);
    return val1 + val2;
}

std::string AddDensityFunction::GetTypeName() const
{
    return "engine:add";
}

MultiplyDensityFunction::MultiplyDensityFunction(std::unique_ptr<DensityFunction> arg1, std::unique_ptr<DensityFunction> arg2) : m_argument1(std::move(arg1)), m_argument2(std::move(arg2))
{
}

float MultiplyDensityFunction::Evaluate(int x, int y, int z) const
{
    float val1 = m_argument1->Evaluate(x, y, z);
    float val2 = m_argument2->Evaluate(x, y, z);
    return val1 * val2;
}

std::string MultiplyDensityFunction::GetTypeName() const
{
    return "engine:mul";
}
