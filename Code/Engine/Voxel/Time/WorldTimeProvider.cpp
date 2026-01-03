//-----------------------------------------------------------------------------------------------
// WorldTimeProvider.cpp
//
// [NEW] Implementation of WorldTimeProvider
// Minecraft-compatible time calculations matching TimeOfDayManager algorithms
//
// Reference: Iris CelestialUniforms.java, Minecraft ClientLevel.java
//-----------------------------------------------------------------------------------------------


#include "WorldTimeProvider.hpp"

#include <cmath>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Voxel/Light/LightEngineCommon.hpp"
using namespace enigma::core;

namespace enigma::voxel
{
    // Helper: fractional part calculation
    static double Frac(double value)
    {
        return value - floor(value);
    }

    WorldTimeProvider::WorldTimeProvider()
        : m_currentTick(TICK_DAY)
          , m_dayCount(0)
          , m_timeScale(1.0f)
          , m_accumulatedTime(0.0f)
          , m_totalTicks(0.0)
    {
        LogInfo(LogTimeProvider, "WorldTimeProvider:: Initialized at tick %d", m_currentTick);
    }

    int WorldTimeProvider::GetCurrentTick() const
    {
        return m_currentTick;
    }

    int WorldTimeProvider::GetDayCount() const
    {
        return m_dayCount;
    }

    // Reference: Minecraft DimensionType.java:113-116 timeOfDay()
    // Non-linear time progression with -0.25 offset and cosine smoothing
    // Formula: d = frac(tick/24000 - 0.25), e = 0.5 - cos(d*PI)/2, return (d*2 + e)/3
    float WorldTimeProvider::GetCelestialAngle() const
    {
        constexpr double PI = 3.14159265358979323846;
        double           d  = Frac(static_cast<double>(m_currentTick) / 24000.0 - 0.25);
        double           e  = 0.5 - std::cos(d * PI) / 2.0;
        return static_cast<float>((d * 2.0 + e) / 3.0);
    }

    float WorldTimeProvider::GetCompensatedCelestialAngle() const
    {
        float angle = GetCelestialAngle() + CELESTIAL_ANGLE_OFFSET;
        if (angle >= 1.0f)
        {
            angle -= 1.0f;
        }
        return angle;
    }

    //-------------------------------------------------------------------------------------------
    // [NEW] GetSunAngle - Reference: Iris CelestialUniforms.java:24-32
    // Converts skyAngle (celestialAngle) to sunAngle
    // sunAngle: 0.0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight
    //-------------------------------------------------------------------------------------------
    float WorldTimeProvider::GetSunAngle() const
    {
        float skyAngle = GetCelestialAngle();

        // Iris formula: if skyAngle < 0.75, return skyAngle + 0.25, else return skyAngle - 0.75
        if (skyAngle < 0.75f)
        {
            return skyAngle + 0.25f;
        }
        else
        {
            return skyAngle - 0.75f;
        }
    }

    //-------------------------------------------------------------------------------------------
    // [NEW] IsDay - Reference: Iris CelestialUniforms.java:60-65
    // Determines whether it is day or night based on the sun angle.
    // Day when sunAngle <= 0.5 (sun above horizon)
    //-------------------------------------------------------------------------------------------
    bool WorldTimeProvider::IsDay() const
    {
        return GetSunAngle() <= 0.5f;
    }

    //-------------------------------------------------------------------------------------------
    // [NEW] GetShadowAngle - Reference: Iris CelestialUniforms.java:34-42
    // Returns shadow angle in range [0, 0.5]
    // The shadow angle represents the progress of the shadow-casting celestial body
    // Day: shadowAngle = sunAngle (sun casts shadows)
    // Night: shadowAngle = sunAngle - 0.5 (moon casts shadows)
    //-------------------------------------------------------------------------------------------
    float WorldTimeProvider::GetShadowAngle() const
    {
        float shadowAngle = GetSunAngle();

        if (!IsDay())
        {
            shadowAngle -= 0.5f;
        }

        return shadowAngle;
    }

    // Reference: Sodium CloudRenderer.java - uses continuous ticks for cloudTime
    float WorldTimeProvider::GetCloudTime() const
    {
        return static_cast<float>(m_totalTicks * CLOUD_TIME_SCALE);
    }

    // Calculate sky light multiplier based on celestial angle
    // Reference: Minecraft LightEngine sky light calculation
    float WorldTimeProvider::GetSkyLightMultiplier() const
    {
        float celestialAngle = GetCelestialAngle();

        // Calculate daylight factor using cosine
        // h = cos(celestialAngle * 2PI) * 2 + 0.5, clamped to [0,1]
        constexpr float TWO_PI = 6.2831853f;
        float           h      = std::cos(celestialAngle * TWO_PI) * 2.0f + 0.5f;

        // Clamp to [0, 1]
        if (h < 0.0f) h = 0.0f;
        if (h > 1.0f) h = 1.0f;

        return h;
    }

    float WorldTimeProvider::GetTimeScale() const
    {
        return m_timeScale;
    }

    void WorldTimeProvider::SetTimeScale(float scale)
    {
        m_timeScale = scale;
        LogInfo(LogTimeProvider, "WorldTimeProvider:: TimeScale set to %.2f", scale);
    }

    void WorldTimeProvider::SetCurrentTick(int tick)
    {
        // Wrap tick value to valid range [0, 23999]
        m_currentTick = tick % TICKS_PER_DAY;
        if (m_currentTick < 0)
        {
            m_currentTick += TICKS_PER_DAY;
        }
        // Reset accumulated time to prevent immediate tick advancement
        m_accumulatedTime = 0.0f;

        LogInfo(LogTimeProvider, "WorldTimeProvider:: Tick set to %d", m_currentTick);
    }

    void WorldTimeProvider::Update(float deltaTime)
    {
        m_accumulatedTime += deltaTime * m_timeScale;

        // Accumulate total ticks for cloud animation (continuous, never resets)
        m_totalTicks += (deltaTime * m_timeScale) / SECONDS_PER_TICK;

        int deltaTicks = static_cast<int>(m_accumulatedTime / SECONDS_PER_TICK);

        if (deltaTicks > 0)
        {
            m_accumulatedTime -= deltaTicks * SECONDS_PER_TICK;
            m_currentTick     += deltaTicks;

            // Handle day rollover
            while (m_currentTick >= TICKS_PER_DAY)
            {
                m_currentTick -= TICKS_PER_DAY;
                m_dayCount++;
                LogInfo(LogTimeProvider, "WorldTimeProvider:: Day %d started", m_dayCount);
            }
        }
    }

    //=============================================================================
    // [NEW] Sun/Moon Direction Vector Calculation - VIEW SPACE
    //=============================================================================
    // Reference: Iris CelestialUniforms.java:119-133 getCelestialPosition()
    //
    // Key Implementation: Returns VIEW SPACE DIRECTION VECTOR (w=0), not position.
    // Uses TransformVectorQuantity3D to ignore camera translation, ensuring
    // celestial bodies maintain correct orientation regardless of player movement.
    //
    // Algorithm:
    // 1. Start with initial direction (0, 0, y) where y=100 for sun, y=-100 for moon
    // 2. Apply Y-axis rotation (celestialAngle * 360 degrees) for day/night cycle
    // 3. Transform as direction vector (w=0) through gbufferModelView
    // 4. Result: View space direction pointing toward celestial body
    //=============================================================================

    Vec3 WorldTimeProvider::CalculateCelestialPosition(float y, const Mat44& gbufferModelView) const
    {
        // ==========================================================================
        // Coordinate System Adaptation: Iris (Y-up) -> Engine (Z-up)
        // ==========================================================================
        // Iris/Minecraft: Y-up, Z-forward (OpenGL style)
        // Our Engine: Z-up, X-forward
        //
        // Iris sun path: rotates around X-axis (east-west), Y is up
        // Our sun path: rotates around Y-axis (left-right), Z is up
        //
        // Mapping: Iris Y -> Engine Z, Iris X -> Engine Y, Iris Z -> Engine X
        // ==========================================================================

        // [Step 1] Initial direction - start pointing UP (+Z in our engine)
        // In Iris this would be (0, y, 0) pointing up along Y
        // In our engine, up is Z, so we use (0, 0, y)
        Vec3 direction(0.0f, 0.0f, y);

        // [Step 2] Get sky angle for rotation
        float celestialAngle = GetCelestialAngle();
        float angleDegrees   = celestialAngle * 360.0f;

        // [Step 3] Build transformation matrix
        // In our Z-up coordinate system:
        // - Sun rises in East (-Y), peaks at Zenith (+Z), sets in West (+Y)
        // - This requires rotation around Y-axis (left-right axis)
        Mat44 celestial = gbufferModelView;

        // Apply Y rotation for celestial angle (sun path rotation)
        // This rotates the sun from +Z (up) around the Y-axis
        Mat44 rotY = Mat44::MakeYRotationDegrees(angleDegrees);
        celestial.Append(rotY);

        // [Step 4] Transform as DIRECTION VECTOR (w=0), not position!
        // This ignores the translation component of gbufferModelView
        // Result is a VIEW SPACE direction pointing toward the celestial body
        direction = celestial.TransformVectorQuantity3D(direction);

        return direction;
    }

    Vec3 WorldTimeProvider::CalculateSunPosition(const Mat44& gbufferModelView) const
    {
        // Sun direction: initial magnitude +100 (pointing upward in local space)
        // Transformed to view space direction vector (w=0)
        return CalculateCelestialPosition(100.0f, gbufferModelView);
    }

    Vec3 WorldTimeProvider::CalculateMoonPosition(const Mat44& gbufferModelView) const
    {
        // Moon direction: initial magnitude -100 (opposite to sun in local space)
        // Transformed to view space direction vector (w=0)
        return CalculateCelestialPosition(-100.0f, gbufferModelView);
    }

    //=============================================================================
    // [NEW] Shadow Light Position - Reference: Iris CelestialUniforms.java:93-95
    //=============================================================================
    // Returns the position of the current shadow-casting light source.
    // During day: sun casts shadows -> return sunPosition
    // During night: moon casts shadows -> return moonPosition
    //
    // Iris implementation:
    //   public Vector4f getShadowLightPosition() {
    //       return isDay() ? getSunPosition() : getMoonPosition();
    //   }
    //=============================================================================
    Vec3 WorldTimeProvider::CalculateShadowLightPosition(const Mat44& gbufferModelView) const
    {
        return IsDay() ? CalculateSunPosition(gbufferModelView) : CalculateMoonPosition(gbufferModelView);
    }

    //=============================================================================
    // [NEW] Up Direction Vector - Reference: Iris CelestialUniforms.java:44-58
    //=============================================================================
    // Returns VIEW SPACE direction pointing toward world "up" (zenith).
    // This is used for atmospheric scattering calculations and determining
    // the angle between view direction and zenith.
    //
    // Iris implementation:
    //   Vector4f upVector = new Vector4f(0.0F, 100.0F, 0.0F, 0.0F);
    //   Matrix4f preCelestial = new Matrix4f(gbufferModelView);
    //   preCelestial.rotate(Axis.YP.rotationDegrees(-90.0F));
    //   upVector = preCelestial.transform(upVector);
    //
    // Coordinate adaptation (Iris Y-up -> Engine Z-up):
    //   Iris: upVector = (0, 100, 0) pointing up along Y
    //   Engine: upVector = (0, 0, 100) pointing up along Z
    //=============================================================================
    Vec3 WorldTimeProvider::CalculateUpPosition(const Mat44& gbufferModelView) const
    {
        // [Step 1] Initial up direction - pointing UP (+Z in our Z-up engine)
        // In Iris (Y-up), this would be (0, 100, 0)
        Vec3 upVector(0.0f, 0.0f, 100.0f);

        // [Step 2] Apply gbufferModelView to transform to view space
        // Note: Unlike celestial positions, we don't apply celestial rotation here
        // because "up" is always world up, not dependent on time of day
        Mat44 preCelestial = gbufferModelView;

        // [Step 3] Apply -90 degree Y rotation (matching Iris's YP.rotationDegrees(-90))
        // This aligns the coordinate system for proper sky orientation
        Mat44 rotY = Mat44::MakeYRotationDegrees(-90.0f);
        preCelestial.Append(rotY);

        // [Step 4] Transform as DIRECTION VECTOR (w=0)
        upVector = preCelestial.TransformVectorQuantity3D(upVector);

        return upVector;
    }

    //=============================================================================
    // [NEW] Cloud Color Calculation
    //=============================================================================
    // Reference: Minecraft ClientLevel.java:673-703 getCloudColor()
    //
    // Algorithm:
    // 1. Calculate daylight factor h = cos(timeOfDay * 2PI) * 2 + 0.5, clamped [0,1]
    // 2. Base color is white (1, 1, 1)
    // 3. Apply rain darkening if rainLevel > 0
    // 4. Apply daylight factor: r,g *= h*0.9+0.1, b *= h*0.85+0.15
    // 5. Apply thunder darkening if thunderLevel > 0
    //=============================================================================

    Vec3 WorldTimeProvider::CalculateCloudColor(float rainLevel, float thunderLevel) const
    {
        // [Step 1] Calculate daylight factor
        float           celestialAngle = GetCelestialAngle();
        constexpr float TWO_PI         = 6.2831855f;
        float           h              = std::cos(celestialAngle * TWO_PI) * 2.0f + 0.5f;

        // Clamp to [0, 1]
        if (h < 0.0f) h = 0.0f;
        if (h > 1.0f) h = 1.0f;

        // [Step 2] Base cloud color is white
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;

        // [Step 3] Apply rain darkening
        if (rainLevel > 0.0f)
        {
            float grayscale = (r * 0.3f + g * 0.59f + b * 0.11f) * 0.6f;
            float factor    = 1.0f - rainLevel * 0.95f;
            r               = r * factor + grayscale * (1.0f - factor);
            g               = g * factor + grayscale * (1.0f - factor);
            b               = b * factor + grayscale * (1.0f - factor);
        }

        // [Step 4] Apply daylight factor
        r *= h * 0.9f + 0.1f;
        g *= h * 0.9f + 0.1f;
        b *= h * 0.85f + 0.15f; // Blue channel slightly brighter at night

        // [Step 5] Apply thunder darkening
        if (thunderLevel > 0.0f)
        {
            float grayscale = (r * 0.3f + g * 0.59f + b * 0.11f) * 0.2f;
            float factor    = 1.0f - thunderLevel * 0.95f;
            r               = r * factor + grayscale * (1.0f - factor);
            g               = g * factor + grayscale * (1.0f - factor);
            b               = b * factor + grayscale * (1.0f - factor);
        }

        return Vec3(r, g, b);
    }
} // namespace enigma::voxel
