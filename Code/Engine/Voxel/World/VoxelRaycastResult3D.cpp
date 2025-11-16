#include "VoxelRaycastResult3D.hpp"
using namespace enigma::voxel;

VoxelRaycastResult3D::VoxelRaycastResult3D(const RaycastResult3D& baseResult)
    : RaycastResult3D(baseResult)
{
}

BlockIterator VoxelRaycastResult3D::GetPlacementIterator() const
{
    if (!m_didImpact)
    {
        return BlockIterator(); // return invalid iterator
    }
    return m_hitBlockIter.GetNeighbor(m_hitFace);
}
