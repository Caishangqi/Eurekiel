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
         * Coordinate Conversion (applied in BlockModelCompiler):
         *   SimpleMiner(x, y, z) = Minecraft(x, z, y)
         *   - SimpleMiner X = Minecraft X (no change)
         *   - SimpleMiner Y = Minecraft Z (swapped)
         *   - SimpleMiner Z = Minecraft Y (swapped)
         *
         * ============================================================================
         * STAIRS FACING SEMANTICS
         * ============================================================================
         *
         * "Facing" direction = where the CLIMBABLE FACE (low step side) points
         * - If stairs faces -X, the climbable face is at -X side
         * - Player stands at +X side and climbs toward -X
         * - HIGH STEP is at +X (player's side), LOW STEP is at -X
         *
         * When placing stairs:
         * - Player looks in direction D
         * - Stairs should have HIGH STEP toward player (direction D)
         * - So "facing" should be OPPOSITE of player look direction
         *
         * ============================================================================
         * DIRECTION MAPPING TABLE
         * ============================================================================
         *
         * | Player Look | High Step At | Facing Dir | Rotation | Result in SimpleMiner |
         * |-------------|--------------|------------|----------|----------------------|
         * | +X (Fwd)    | +X           | EAST       | y:0      | High step at +X      |
         * | -X (Back)   | -X           | WEST       | y:180    | High step at -X      |
         * | +Y (Left)   | +Y           | NORTH      | y:270    | High step at +Y      |
         * | -Y (Right)  | -Y           | SOUTH      | y:90     | High step at -Y      |
         *
         * [IMPORTANT] Y-axis mapping appears "swapped" (NORTH for +Y, SOUTH for -Y):
         * This is because Minecraft's Y rotation is applied BEFORE coordinate conversion.
         * - Minecraft y:90  rotates EAST(+X) -> SOUTH(+Z) -> SimpleMiner -Y
         * - Minecraft y:270 rotates EAST(+X) -> NORTH(-Z) -> SimpleMiner +Y
         * The rotation direction in Minecraft space combined with Y<->Z swap produces
         * this counter-intuitive but correct mapping.
         *
         * ============================================================================
         */
        Direction GetHorizontalFacing() const
        {
            float absX = std::abs(playerLookDir.x);
            float absY = std::abs(playerLookDir.y);

            if (absX > absY)
            {
                // X-axis dominant (Forward/Backward in SimpleMiner)
                // Direct mapping: SimpleMiner +X = Minecraft EAST, -X = WEST
                return (playerLookDir.x > 0.0f) ? Direction::EAST : Direction::WEST;
            }
            else
            {
                // Y-axis dominant (Left/Right in SimpleMiner)
                // Swapped mapping due to rotation + coordinate conversion interaction:
                // SimpleMiner +Y needs NORTH (y:270), -Y needs SOUTH (y:90)
                return (playerLookDir.y > 0.0f) ? Direction::NORTH : Direction::SOUTH;
            }
        }
    };
}
