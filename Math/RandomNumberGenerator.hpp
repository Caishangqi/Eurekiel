#pragma once
class RandomNumberGenerator
{
public:
    int RollRandomIntLessThan(int maxNotInclusive);
    int RollRandomIntInRange(int minInclusive, int maxExclusive);
    float RollRandomFloatZeroToOne();
    float RollRandomFloatInRange(float minInclusive, float maxExclusive);

private:
    
};



