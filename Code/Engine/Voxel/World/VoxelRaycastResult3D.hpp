#pragma once

#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Voxel/Block/BlockIterator.hpp"

namespace enigma::voxel
{
    //----------------------------------------------------------------------------------------------------------------
    // VoxelRaycastResult3D - voxel world ray detection results
    // Derived from RaycastResult3D, adds voxel-specific hit information
    //----------------------------------------------------------------------------------------------------------------
    struct VoxelRaycastResult3D : public RaycastResult3D
    {
        BlockIterator m_hitBlockIter; // Iterator of hit blocks (maintains Chunk pointer, efficient across blocks)
        Direction     m_hitFace = Direction::NORTH; //Which side of the block is hit (used to place the block)
        //----------------------------------------------------------------------------------------------------------------
        // Default constructor
        //----------------------------------------------------------------------------------------------------------------
        VoxelRaycastResult3D() = default;

        //----------------------------------------------------------------------------------------------------------------
        // Constructed from base class
        //----------------------------------------------------------------------------------------------------------------
        explicit VoxelRaycastResult3D(const RaycastResult3D& baseResult);

        //----------------------------------------------------------------------------------------------------------------
        // Get the neighbors of the hit block (used to place the block)
        // Return the BlockIterator that hits the opposite side
        //----------------------------------------------------------------------------------------------------------------
        BlockIterator GetPlacementIterator() const;
    };
} // namespace enigma::voxel
