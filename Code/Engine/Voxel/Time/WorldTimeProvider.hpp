#pragma once
#include "ITimeProvider.hpp"
//-----------------------------------------------------------------------------------------------
// WorldTimeProvider.hpp
//
// [NEW] Default time provider implementing ITimeProvider interface
// Minecraft-compatible time system with tick progression and celestial calculations
//
// Reference: Minecraft DimensionType.java, TimeCommand.java, ClientLevel.java
// Reference: Iris CelestialUniforms.java
//
//-----------------------------------------------------------------------------------------------


namespace enigma::voxel
{
    class WorldTimeProvider : public ITimeProvider
    {
    public:
        // Minecraft Official Time Constants (Reference: TimeCommand.java:17-24)
        static constexpr int TICK_DAY      = 1000; // Morning (sun just risen)
        static constexpr int TICK_NOON     = 6000; // Noon (sun at zenith)
        static constexpr int TICK_NIGHT    = 13000; // Night begins
        static constexpr int TICK_MIDNIGHT = 18000; // Midnight (moon at zenith)
        static constexpr int TICKS_PER_DAY = 24000; // Full day-night cycle

        // Time conversion constants
        static constexpr float SECONDS_PER_TICK       = 0.05f; // 20 ticks per second
        static constexpr float CLOUD_TIME_SCALE       = 0.03f; // Cloud animation speed
        static constexpr float CELESTIAL_ANGLE_OFFSET = 0.25f; // Sun/Moon phase offset
        static constexpr float SUN_PATH_ROTATION      = 0.0f; // Sun path tilt (default: 0)

        WorldTimeProvider();
        ~WorldTimeProvider() override = default;

        // ITimeProvider interface implementation
        int   GetCurrentTick() const override;
        int   GetDayCount() const override;
        float GetCelestialAngle() const override;
        float GetCompensatedCelestialAngle() const override;
        float GetCloudTime() const override;
        float GetSkyLightMultiplier() const override;
        float GetTimeScale() const override;
        void  SetTimeScale(float scale) override;
        void  SetCurrentTick(int tick) override;
        void  Update(float deltaTime) override;

        //-------------------------------------------------------------------------------------------
        // [NEW] Sun angle - Reference: Iris CelestialUniforms.java:24-32
        //-------------------------------------------------------------------------------------------
        float GetSunAngle() const override;

        //-------------------------------------------------------------------------------------------
        // [NEW] Celestial body direction vectors - Reference: Iris CelestialUniforms.java:119-133
        //-------------------------------------------------------------------------------------------
        Vec3 CalculateSunPosition(const Mat44& gbufferModelView) const override;
        Vec3 CalculateMoonPosition(const Mat44& gbufferModelView) const override;

        //-------------------------------------------------------------------------------------------
        // [NEW] Cloud color - Reference: Minecraft ClientLevel.java:673-703
        //-------------------------------------------------------------------------------------------
        Vec3 CalculateCloudColor(float rainLevel, float thunderLevel) const override;

    private:
        //-------------------------------------------------------------------------------------------
        // Internal helper: Calculate celestial direction vector in view space
        // @param y Initial direction magnitude (100 for sun, -100 for moon)
        // @param gbufferModelView The World->Camera transform matrix
        // @return View space direction vector (w=0), length ~100 units
        //-------------------------------------------------------------------------------------------
        Vec3 CalculateCelestialPosition(float y, const Mat44& gbufferModelView) const;

        int    m_currentTick     = TICK_DAY; // Current day tick (0-23999)
        int    m_dayCount        = 0; // Total days elapsed
        float  m_timeScale       = 1.0f; // Time progression multiplier
        float  m_accumulatedTime = 0.0f; // Sub-tick time accumulator
        double m_totalTicks      = 0.0; // Continuous tick counter for cloud animation
    };
} // namespace enigma::voxel
