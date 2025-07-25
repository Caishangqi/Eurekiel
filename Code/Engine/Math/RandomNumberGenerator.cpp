#include "Engine/Math/RandomNumberGenerator.hpp"
#include <stdlib.h>

int RandomNumberGenerator::RollRandomIntLessThan(int maxNotInclusive)
{
    return rand() % maxNotInclusive;
}

int RandomNumberGenerator::RollRandomIntInRange(int minInclusive, int maxExclusive)
{
    int range = (maxExclusive - minInclusive) + 1; // 8 - 6 + 1 = 3
    return minInclusive + RollRandomIntLessThan(range);
}

float RandomNumberGenerator::RollRandomFloatZeroToOne()
{
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float RandomNumberGenerator::RollRandomFloatInRange(float minInclusive, float maxExclusive)
{
    float range       = (maxExclusive - minInclusive); // 计算范围
    float randomValue = static_cast<float>(rand()) / RAND_MAX;
    return minInclusive + randomValue * range;
}

float RandomNumberGenerator::__RollRandomFloatInRange(float minInclusive, float maxExclusive)
{
    float range       = (maxExclusive - minInclusive); // 计算范围
    float randomValue = static_cast<float>(rand()) / RAND_MAX;
    return minInclusive + randomValue * range;
}
