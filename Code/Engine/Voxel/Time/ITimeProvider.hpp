#pragma once
#include "Engine/Core/LogCategory/LogCategory.hpp"
//-----------------------------------------------------------------------------------------------
// ITimeProvider.hpp
//
// [NEW] Pure virtual interface for time access in VoxelLight module
// Provides abstraction for different time implementations (WorldTimeProvider, FixedTimeProvider)
//
// Usage:
//   ITimeProvider* provider = GetTimeProvider();
//   float angle = provider->GetCelestialAngle();
//   provider->Update(deltaTime);
//
// Reference: Iris CelestialUniforms.java, Minecraft ClientLevel.java
//-----------------------------------------------------------------------------------------------

struct Vec3;
struct Mat44;

DECLARE_LOG_CATEGORY_EXTERN(LogTimeProvider);

namespace enigma::voxel
{
    class ITimeProvider
    {
    public:
        virtual ~ITimeProvider() = default;

        // Tick and day queries
        virtual int GetCurrentTick() const = 0;
        virtual int GetDayCount() const = 0;

        // Celestial calculations
        virtual float GetCelestialAngle() const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Sun angle calculation - Reference: Iris CelestialUniforms.java:24-32
        // Returns sun angle in range [0, 1] where:
        //   0.0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight
        //-------------------------------------------------------------------------------------------
        virtual float GetSunAngle() const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Day/Night check - Reference: Iris CelestialUniforms.java:60-65
        // Returns true if sun is above horizon (sunAngle <= 0.5)
        //-------------------------------------------------------------------------------------------
        virtual bool IsDay() const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Shadow angle - Reference: Iris CelestialUniforms.java:34-42
        // Returns shadow angle in range [0, 0.5]
        // Day: shadowAngle = sunAngle, Night: shadowAngle = sunAngle - 0.5
        //-------------------------------------------------------------------------------------------
        virtual float GetShadowAngle() const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Celestial body direction vectors - Reference: Iris CelestialUniforms.java:119-133
        // Returns VIEW SPACE DIRECTION VECTOR (w=0), not position!
        // The vector points toward the celestial body, ignoring camera translation.
        // @param gbufferModelView The World->Camera transform matrix (from BeginCamera)
        //-------------------------------------------------------------------------------------------
        virtual Vec3 CalculateSunPosition(const Mat44& gbufferModelView) const = 0;
        virtual Vec3 CalculateMoonPosition(const Mat44& gbufferModelView) const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Shadow light position - Reference: Iris CelestialUniforms.java:93-95
        // Returns the position of the current shadow-casting light source (sun or moon)
        // Day: returns sunPosition, Night: returns moonPosition
        // @param gbufferModelView The World->Camera transform matrix
        //-------------------------------------------------------------------------------------------
        virtual Vec3 CalculateShadowLightPosition(const Mat44& gbufferModelView) const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Up direction vector - Reference: Iris CelestialUniforms.java:44-58
        // Returns VIEW SPACE direction pointing toward world "up" (zenith)
        // Used for atmospheric scattering and sky rendering calculations
        // @param gbufferModelView The World->Camera transform matrix
        //-------------------------------------------------------------------------------------------
        virtual Vec3 CalculateUpPosition(const Mat44& gbufferModelView) const = 0;

        //-------------------------------------------------------------------------------------------
        // [NEW] Cloud color calculation - Reference: Minecraft ClientLevel.java:673-703
        // Returns RGB color for clouds based on time of day
        // @param rainLevel Rain intensity [0, 1] (0 = clear, 1 = heavy rain)
        // @param thunderLevel Thunder intensity [0, 1] (0 = none, 1 = heavy thunder)
        //-------------------------------------------------------------------------------------------
        virtual Vec3 CalculateCloudColor(float rainLevel, float thunderLevel) const = 0;

        // Environment queries
        virtual float GetCloudTime() const = 0;
        virtual float GetSkyLightMultiplier() const = 0;

        // Time scale control
        virtual float GetTimeScale() const = 0;
        virtual void  SetTimeScale(float scale) = 0;

        // Time manipulation
        virtual void SetCurrentTick(int tick) = 0;

        // Frame update
        virtual void Update(float deltaTime) = 0;
    };
} // namespace enigma::voxel
