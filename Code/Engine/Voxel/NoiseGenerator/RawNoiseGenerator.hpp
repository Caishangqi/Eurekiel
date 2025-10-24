#pragma once
#include "NoiseGenerator.hpp"

namespace enigma::voxel
{
    class RawNoiseGenerator : public NoiseGenerator
    {
    public:
        RawNoiseGenerator(
            unsigned int seed           = 0,
            bool         useNegOneToOne = true
        );

        float Sample(float x, float y, float z) const override;
        float Sample2D(float x, float z) const override;

        NoiseType   GetType() const override;
        std::string GetConfigString() const override;

        /// Getters
        unsigned int GetSeed() const { return m_seed; }
        bool         GetUseNegOneToOne() const { return m_useNegOneToOne; }

    private:
        unsigned int m_seed;
        bool         m_useNegOneToOne; // true=[-1,1], false=[0,1]
    };
}
