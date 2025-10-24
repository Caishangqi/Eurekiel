#pragma once
#include "NoiseGenerator.hpp"

namespace enigma::voxel
{
    class FractalNoiseGenerator : public NoiseGenerator
    {
    public:
        FractalNoiseGenerator(
            unsigned int seed              = 0,
            float        scale             = 1.0f,
            unsigned int numOctaves        = 1,
            float        octavePersistence = 0.5f,
            float        octaveScale       = 2.0f,
            bool         renormalize       = true
        );

        float       Sample(float x, float y, float z) const override;
        float       Sample2D(float x, float z) const override;
        NoiseType   GetType() const override;
        std::string GetConfigString() const override;

        /// Getters
        unsigned int GetSeed() const { return m_seed; }
        float        GetScale() const { return m_scale; }
        unsigned int GetNumOctaves() const { return m_numOctaves; }
        float        GetOctavePersistence() const { return m_octavePersistence; }
        float        GetOctaveScale() const { return m_octaveScale; }
        bool         GetRenormalize() const { return m_renormalize; }

    private:
        unsigned int m_seed;
        float        m_scale;
        unsigned int m_numOctaves;
        float        m_octavePersistence;
        float        m_octaveScale;
        bool         m_renormalize;
    };
}
