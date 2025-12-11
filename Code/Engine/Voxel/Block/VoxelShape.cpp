#include "VoxelShape.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include <limits>

namespace enigma::voxel
{
    //-----------------------------------------------------------------------------------------------
    VoxelShape::VoxelShape(const AABB3& box)
        : m_boxes{box}
    {
    }

    //-----------------------------------------------------------------------------------------------
    VoxelShape::VoxelShape(std::vector<AABB3> boxes)
        : m_boxes(std::move(boxes))
    {
    }

    //-----------------------------------------------------------------------------------------------
    VoxelShape VoxelShape::Block()
    {
        return VoxelShape(AABB3(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f));
    }

    //-----------------------------------------------------------------------------------------------
    VoxelShape VoxelShape::Empty()
    {
        return VoxelShape();
    }

    //-----------------------------------------------------------------------------------------------
    VoxelShape VoxelShape::Box(float minX, float minY, float minZ,
                               float maxX, float maxY, float maxZ)
    {
        return VoxelShape(AABB3(minX, minY, minZ, maxX, maxY, maxZ));
    }

    //-----------------------------------------------------------------------------------------------
    VoxelShape VoxelShape::Or(const VoxelShape& a, const VoxelShape& b)
    {
        std::vector<AABB3> combined;
        combined.reserve(a.m_boxes.size() + b.m_boxes.size());

        for (const auto& box : a.m_boxes)
        {
            combined.push_back(box);
        }
        for (const auto& box : b.m_boxes)
        {
            combined.push_back(box);
        }

        return VoxelShape(std::move(combined));
    }

    //-----------------------------------------------------------------------------------------------
    bool VoxelShape::IsPointInside(const Vec3& point) const
    {
        for (const auto& box : m_boxes)
        {
            if (box.IsPointInside(point))
            {
                return true;
            }
        }
        return false;
    }

    //-----------------------------------------------------------------------------------------------
    RaycastResult3D VoxelShape::Raycast(const Vec3& rayStart, const Vec3& rayDir, float maxDist) const
    {
        RaycastResult3D closestHit;
        closestHit.m_didImpact    = false;
        closestHit.m_rayStartPos  = rayStart;
        closestHit.m_rayFwdNormal = rayDir;
        closestHit.m_rayMaxLength = maxDist;
        closestHit.m_impactDist   = maxDist;

        // Test against all component boxes
        for (const auto& box : m_boxes)
        {
            RaycastResult3D result = RaycastVsAABB3D(rayStart, rayDir, maxDist, box);

            if (result.m_didImpact && result.m_impactDist < closestHit.m_impactDist)
            {
                closestHit = result;
            }
        }

        return closestHit;
    }

    //-----------------------------------------------------------------------------------------------
    RaycastResult3D VoxelShape::RaycastWorld(const Vec3& rayStart, const Vec3& rayDir,
                                             float       maxDist, const Vec3&  blockWorldPos) const
    {
        // Transform ray to block-local space
        Vec3 localRayStart = rayStart - blockWorldPos;

        // Perform raycast in local space
        RaycastResult3D localResult = Raycast(localRayStart, rayDir, maxDist);

        // Transform result back to world space
        if (localResult.m_didImpact)
        {
            localResult.m_impactPos   = localResult.m_impactPos + blockWorldPos;
            localResult.m_rayStartPos = rayStart; // Restore world-space ray start
        }

        return localResult;
    }
}
