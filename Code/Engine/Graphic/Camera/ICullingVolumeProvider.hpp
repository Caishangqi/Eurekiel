#pragma once

class Frustum;

namespace enigma::graphic
{
    class ICullingVolumeProvider
    {
    public:
        virtual ~ICullingVolumeProvider() = default;

        virtual bool GetFrustum(Frustum& outFrustum) const = 0;
    };
}
