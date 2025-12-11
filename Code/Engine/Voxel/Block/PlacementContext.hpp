#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"
#include "Engine/Voxel/Block/BlockPos.hpp"

// Forward declarations
namespace enigma::voxel
{
    class World;
}

namespace enigma::registry::block
{
    class Block;
}

namespace enigma::voxel
{
    /**
     * @brief Placement context encapsulating raycast results and player state
     *
     * This structure provides all necessary information for blocks to determine
     * their placement state based on player interaction. Used by Block::GetStateForPlacement().
     */
    struct PlacementContext
    {
        World*                  world; // World instance where block is being placed
        BlockPos                targetPos; // Target position for block placement
        BlockPos                clickedPos; // Position of the clicked block
        Direction               clickedFace; // Face of the clicked block (from VoxelRaycastResult3D)
        Vec3                    hitPoint; // Hit point in block-local space (0-1 normalized)
        Vec3                    playerLookDir; // Player camera forward vector
        registry::block::Block* heldItemBlock; // Held item block type (nullptr if not a block)

        /**
         * @brief Check if the click hit the top half of the block
         * @return True if hitPoint.z >= 0.5f, false otherwise
         */
        bool IsTopHalf() const
        {
            return hitPoint.z >= 0.5f;
        }

        /**
         * @brief Convert player look direction to horizontal facing (N/E/S/W)
         * @return Horizontal direction based on player look vector
         *
         * ============================================================================
         * MINECRAFT REFERENCE (StairBlock.java:114)
         * ============================================================================
         *
         * Minecraft's StairBlock.getStateForPlacement():
         *   .setValue(FACING, blockPlaceContext.getHorizontalDirection())
         *
         * Where getHorizontalDirection() (UseOnContext.java:70-71):
         *   return this.player == null ? Direction.NORTH : this.player.getDirection();
         *
         * This means: stairs FACING = player look direction (SAME, not opposite!)
         *
         * ============================================================================
         * COORDINATE SYSTEM REFERENCE
         * ============================================================================
         *
         * SimpleMiner Coordinate System:
         *   +X = Forward, -X = Backward
         *   +Y = Left,    -Y = Right
         *   +Z = Up,      -Z = Down
         *
         * Minecraft Coordinate System:
         *   +X = East,  -X = West
         *   +Y = Up,    -Y = Down
         *   +Z = South, -Z = North
         *
         * Coordinate Conversion (applied in BlockModelCompiler.cpp:206-207):
         *   SimpleMiner(x, y, z) = Minecraft(x, z, y)
         *   - SimpleMiner X = Minecraft X (no change)
         *   - SimpleMiner Y = Minecraft Z (swapped)
         *   - SimpleMiner Z = Minecraft Y (swapped)
         *
         * Direction Mapping:
         *   SimpleMiner +X = Minecraft +X = EAST
         *   SimpleMiner -X = Minecraft -X = WEST
         *   SimpleMiner +Y = Minecraft -Z = NORTH
         *   SimpleMiner -Y = Minecraft +Z = SOUTH
         *
         * ============================================================================
         * STAIRS FACING SEMANTICS
         * ============================================================================
         *
         * "Facing" direction = direction player walks UP the stairs
         *   - facing=EAST means player walks toward +X to climb
         *   - The LOW STEP (entry point) is at the -X side
         *   - The HIGH STEP is at the +X side
         *
         * When placing stairs:
         *   - Player looks in direction D
         *   - Stairs "facing" is set to D (same as player look)
         *   - Player will walk UP the stairs in direction D
         *
         * ============================================================================
         * DIRECTION MAPPING TABLE
         * ============================================================================
         *
         * | Player Look (SM) | facing Value | Blockstate Rotation | High Step At |
         * |------------------|--------------|---------------------|--------------|
         * | +X (Forward)     | EAST         | y:0                 | +X           |
         * | -X (Backward)    | WEST         | y:180               | -X           |
         * | +Y (Left)        | NORTH        | y:270               | +Y           |
         * | -Y (Right)       | SOUTH        | y:90                | -Y           |
         *
         * ============================================================================
         */
        Direction GetHorizontalFacing() const
        {
            // Stairs facing = player look direction (same as Minecraft behavior)
            // Reference: Minecraft StairBlock.java:114, UseOnContext.java:70-71

            float absX = std::abs(playerLookDir.x);
            float absY = std::abs(playerLookDir.y);

            if (absX > absY)
            {
                // X-axis dominant (Forward/Backward in SimpleMiner)
                // Player looks +X → facing EAST (climb toward +X)
                // Player looks -X → facing WEST (climb toward -X)
                return (playerLookDir.x > 0.0f) ? Direction::EAST : Direction::WEST;
            }
            else
            {
                // Y-axis dominant (Left/Right in SimpleMiner)
                // SimpleMiner +Y = Minecraft -Z = NORTH
                // SimpleMiner -Y = Minecraft +Z = SOUTH
                // Player looks +Y → facing NORTH (climb toward +Y)
                // Player looks -Y → facing SOUTH (climb toward -Y)
                return (playerLookDir.y > 0.0f) ? Direction::NORTH : Direction::SOUTH;
            }
        }
    };
}
