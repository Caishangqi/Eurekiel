#pragma once
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include <vector>

namespace enigma::voxel
{
    /**
     * @brief Represents a collision shape composed of one or more AABB3 boxes
     *
     * VoxelShape is used for precise collision detection with non-full blocks
     * like slabs and stairs. It supports:
     * - Single AABB shapes (slabs, full blocks)
     * - Compound shapes with multiple AABBs (stairs with corner pieces)
     * - Raycast testing against all component boxes
     *
     * Coordinate System: +X forward, +Y left, +Z up
     * All coordinates are in block-local space [0,1]³
     *
     * Reference: Minecraft's VoxelShape system (phys/shapes/VoxelShape.java)
     */
    class VoxelShape
    {
    public:
        /**
         * @brief Construct an empty VoxelShape
         */
        VoxelShape() = default;

        /**
         * @brief Construct a VoxelShape with a single AABB
         * @param box The AABB in block-local coordinates [0,1]³
         */
        explicit VoxelShape(const AABB3& box);

        /**
         * @brief Construct a VoxelShape with multiple AABBs
         * @param boxes Vector of AABBs in block-local coordinates
         */
        explicit VoxelShape(std::vector<AABB3> boxes);

        /**
         * @brief Destructor
         */
        ~VoxelShape() = default;

        // ==================== Factory Methods ====================

        /**
         * @brief Create a full block shape (1x1x1 cube)
         * @return VoxelShape covering the entire block space
         */
        static VoxelShape Block();

        /**
         * @brief Create an empty shape (no collision)
         * @return Empty VoxelShape with no boxes
         */
        static VoxelShape Empty();

        /**
         * @brief Create a box shape from min/max coordinates
         * @param minX, minY, minZ Minimum corner (block-local, 0-1 range)
         * @param maxX, maxY, maxZ Maximum corner (block-local, 0-1 range)
         * @return VoxelShape with single AABB
         *
         * Note: Coordinates are in Minecraft-style 0-16 range internally,
         * but this method accepts normalized 0-1 range for convenience.
         */
        static VoxelShape Box(float minX, float minY, float minZ,
                              float maxX, float maxY, float maxZ);

        /**
         * @brief Combine two shapes using OR operation
         * @param a First shape
         * @param b Second shape
         * @return Combined shape containing all boxes from both
         *
         * Used for building compound shapes like stairs.
         */
        static VoxelShape Or(const VoxelShape& a, const VoxelShape& b);

        // ==================== Accessors ====================

        /**
         * @brief Check if the shape is empty (no collision boxes)
         * @return True if shape has no boxes
         */
        bool IsEmpty() const { return m_boxes.empty(); }

        /**
         * @brief Get the number of boxes in this shape
         * @return Number of AABB components
         */
        size_t GetBoxCount() const { return m_boxes.size(); }

        /**
         * @brief Get all boxes in the shape
         * @return Const reference to box vector
         */
        const std::vector<AABB3>& GetBoxes() const { return m_boxes; }

        /**
         * @brief Check if a point is inside any box of this shape
         * @param point Point in block-local coordinates
         * @return True if point is inside any component box
         */
        bool IsPointInside(const Vec3& point) const;

        // ==================== Raycast ====================

        /**
         * @brief Perform raycast against all boxes in this shape
         * @param rayStart Ray origin in block-local coordinates
         * @param rayDir Ray direction (normalized)
         * @param maxDist Maximum ray distance
         * @return RaycastResult3D with closest hit (if any)
         *
         * Tests ray against all component boxes and returns the closest hit.
         * Uses RaycastVsAABB3D for individual box tests.
         */
        RaycastResult3D Raycast(const Vec3& rayStart, const Vec3& rayDir, float maxDist) const;

        /**
         * @brief Perform raycast with block world position offset
         * @param rayStart Ray origin in world coordinates
         * @param rayDir Ray direction (normalized)
         * @param maxDist Maximum ray distance
         * @param blockWorldPos World position of the block (integer coords)
         * @return RaycastResult3D with closest hit in world coordinates
         *
         * Transforms ray to block-local space, performs raycast,
         * then transforms result back to world space.
         */
        RaycastResult3D RaycastWorld(const Vec3& rayStart, const Vec3& rayDir,
                                     float       maxDist, const Vec3&  blockWorldPos) const;

    private:
        std::vector<AABB3> m_boxes; ///< Component AABBs in block-local space [0,1]³
    };

    // ==================== Pre-defined Shapes ====================

    /**
     * @brief Common shape constants for reuse
     *
     * These are lazy-initialized static shapes for common block types.
     * Coordinates follow engine convention: +X forward, +Y left, +Z up
     */
    namespace Shapes
    {
        // Full block
        inline const VoxelShape& FullBlock()
        {
            static VoxelShape shape = VoxelShape::Block();
            return shape;
        }

        // Slab shapes (Z-axis is vertical in our coordinate system)
        inline const VoxelShape& SlabBottom()
        {
            // Bottom half: Z from 0 to 0.5
            static VoxelShape shape = VoxelShape::Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f);
            return shape;
        }

        inline const VoxelShape& SlabTop()
        {
            // Top half: Z from 0.5 to 1.0
            static VoxelShape shape = VoxelShape::Box(0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f);
            return shape;
        }

        // Empty shape (air, transparent blocks)
        inline const VoxelShape& Empty()
        {
            static VoxelShape shape = VoxelShape::Empty();
            return shape;
        }
    }
}
