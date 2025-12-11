#pragma once
#include "Engine/Registry/Block/Block.hpp"
#include "Engine/Voxel/Property/PropertyTypes.hpp"
#include "Engine/Voxel/Block/HalfType.hpp"
#include "Engine/Voxel/Block/StairsShape.hpp"

namespace enigma::voxel
{
    class World;
    struct BlockPos;
    class BlockIterator;

    /**
     * @brief Stairs block implementation with auto-connecting shape calculation
     *
     * StairsBlock represents stair blocks that can exist in multiple states:
     * - Facing: 4 horizontal directions (NORTH, SOUTH, EAST, WEST)
     * - Half: TOP or BOTTOM (vertical placement)
     * - Shape: 5 variants (STRAIGHT, INNER_LEFT, INNER_RIGHT, OUTER_LEFT, OUTER_RIGHT)
     *
     * Total States: 4 (facing) × 2 (half) × 5 (shape) = 40 block states
     *
     * Key Features:
     * - Facing determined by player look direction (GetHorizontalFacing)
     * - Half determined by raycast hit point (IsTopHalf)
     * - Shape auto-calculated based on adjacent stairs with matching half
     * - Shape updates when neighboring stairs change (OnNeighborChanged)
     *
     * Shape Calculation Logic (Reference: Minecraft StairBlock.java lines 126-153):
     * 1. Check front neighbor (in facing direction):
     *    - If stairs with same half and perpendicular axis → OUTER corner
     *    - CounterClockWise relative direction → OUTER_LEFT
     *    - Clockwise relative direction → OUTER_RIGHT
     * 2. Check back neighbor (opposite facing direction):
     *    - If stairs with same half and perpendicular axis → INNER corner
     *    - CounterClockWise relative direction → INNER_LEFT
     *    - Clockwise relative direction → INNER_RIGHT
     * 3. Default: STRAIGHT (no adjacent stairs or parallel alignment)
     *
     * Example Usage:
     * @code
     * auto oakStairs = std::make_shared<StairsBlock>("oak_stairs", "simpleminer");
     * registry->Register(oakStairs);
     * @endcode
     */
    class StairsBlock : public registry::block::Block
    {
    private:
        std::shared_ptr<DirectionProperty>         m_facingProperty; ///< Facing direction (4 horizontal values)
        std::shared_ptr<EnumProperty<HalfType>>    m_halfProperty; ///< Vertical half (top/bottom)
        std::shared_ptr<EnumProperty<StairsShape>> m_shapeProperty; ///< Stairs shape (5 variants)

    public:
        /**
         * @brief Construct a new StairsBlock
         * @param registryName Unique registry name (e.g., "oak_stairs")
         * @param ns Namespace (default: "", uses global namespace)
         *
         * Constructor automatically:
         * - Creates DirectionProperty with 4 horizontal directions
         * - Creates EnumProperty<HalfType> with 2 values (bottom, top)
         * - Creates EnumProperty<StairsShape> with 5 values (straight, inner_left, inner_right, outer_left, outer_right)
         * - Sets block-level flags: opaque=false, full_block=false
         * - Registers properties and generates 40 block states
         */
        explicit StairsBlock(const std::string& registryName, const std::string& ns = "");

        /**
         * @brief Destructor
         */
        ~StairsBlock() override = default;

        /**
         * @brief Determine BlockState for placement based on player facing and hit point
         * @param ctx Placement context containing player look direction and hit point
         * @return BlockState with facing, half, and calculated shape
         *
         * Placement Logic:
         * 1. Facing = ctx.GetHorizontalFacing() (player look direction)
         * 2. Half = ctx.IsTopHalf() ? TOP : BOTTOM (raycast hit point Z >= 0.5)
         * 3. Shape = GetStairsShape() (calculated from adjacent stairs)
         *
         * Example:
         * - Player looks NORTH, hits bottom half → facing=NORTH, half=BOTTOM
         * - Adjacent stairs determines shape (STRAIGHT/INNER/OUTER)
         */
        enigma::voxel::BlockState* GetStateForPlacement(const PlacementContext& ctx) const override;

        /**
         * @brief Calculate stairs shape based on adjacent stairs with matching half
         * @param facing Current stairs facing direction
         * @param half Current stairs half (top/bottom)
         * @param world World instance for neighbor queries
         * @param pos Position of the stairs block
         * @return Calculated StairsShape based on neighbor configuration
         *
         * Algorithm (Reference: Minecraft StairBlock.java getStairsShape):
         * 1. Get front neighbor (pos + facing direction)
         *    - Check if stairs with same half and perpendicular axis
         *    - If neighbor facing is CounterClockWise(facing) → OUTER_LEFT
         *    - If neighbor facing is Clockwise(facing) → OUTER_RIGHT
         * 2. Get back neighbor (pos - facing direction)
         *    - Check if stairs with same half and perpendicular axis
         *    - If neighbor facing is CounterClockWise(facing) → INNER_LEFT
         *    - If neighbor facing is Clockwise(facing) → INNER_RIGHT
         * 3. No matching neighbors → STRAIGHT
         *
         * Uses BlockIterator::GetNeighbor() for cross-chunk access.
         * Checks CanTakeShape() to ensure valid corner formation.
         */
        StairsShape GetStairsShape(Direction facing, HalfType half, World* world, const BlockPos& pos) const;

        /**
         * @brief Update shape when neighboring blocks change
         * @param world World instance
         * @param pos Position of this stairs block
         * @param state Current BlockState of this stairs
         * @param neighborBlock Block type of the changed neighbor
         *
         * Called by World when a neighboring block changes (place/break).
         * Only updates if:
         * 1. Neighbor is also a StairsBlock (neighborBlock == this)
         * 2. Recalculated shape differs from current shape
         *
         * Prevents infinite recursion by checking shape equality before SetBlock().
         * Uses World::SetBlock() to replace state, triggering mesh rebuild.
         */
        void OnNeighborChanged(World* world, const BlockPos& pos, enigma::voxel::BlockState* state, registry::block::Block* neighborBlock) override;

        /**
         * @brief Get model path for specific stairs state
         * @param state BlockState to get model for
         * @return Model path string (e.g., "simpleminer:block/oak_stairs")
         *
         * Model Path Format:
         * - All shapes use same base model: "{namespace}:block/{registryName}"
         * - Shape variants handled by blockstate JSON (multipart with when clauses)
         * - Example: "simpleminer:block/oak_stairs"
         *
         * Note: Unlike SlabBlock, stairs use multipart models where shape property
         * determines which model elements to include (stairs/inner_stairs/outer_stairs).
         */
        std::string GetModelPath(enigma::voxel::BlockState* state) const override;

    protected:
        /**
         * @brief Initialize BlockState during state generation
         * @param state BlockState being initialized
         * @param properties Property map for this state
         *
         * Called by Block::GenerateBlockStates() for each property combination.
         * Currently no custom initialization needed beyond base class.
         */
        void InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties) override;

    private:
        /**
         * @brief Check if a BlockState is a stairs block
         * @param state BlockState to check
         * @return True if state belongs to a StairsBlock
         *
         * Helper for GetStairsShape() to detect adjacent stairs.
         */
        static bool IsStairs(enigma::voxel::BlockState* state);

        /**
         * @brief Check if stairs can form a corner with neighbor in given direction
         * @param state Current stairs BlockState
         * @param world World instance for neighbor queries
         * @param pos Position of current stairs
         * @param neighborDir Direction to check for interfering stairs
         * @return True if no interfering stairs prevents corner formation
         *
         * Helper for GetStairsShape() to validate corner connections.
         * Returns false if there's a stairs in neighborDir with same facing and half.
         * Reference: Minecraft StairBlock.java canTakeShape() line 155-158
         */
        bool CanTakeShape(enigma::voxel::BlockState* state, World* world, const BlockPos& pos, Direction neighborDir) const;

        /**
         * @brief Get direction rotated counter-clockwise (left turn)
         * @param dir Direction to rotate
         * @return Rotated direction (NORTH→WEST, WEST→SOUTH, SOUTH→EAST, EAST→NORTH)
         */
        static Direction GetCounterClockWise(Direction dir);

        /**
         * @brief Get direction rotated clockwise (right turn)
         * @param dir Direction to rotate
         * @return Rotated direction (NORTH→EAST, EAST→SOUTH, SOUTH→WEST, WEST→NORTH)
         */
        static Direction GetClockWise(Direction dir);

        /**
         * @brief Get opposite direction
         * @param dir Direction to reverse
         * @return Opposite direction (NORTH↔SOUTH, EAST↔WEST, UP↔DOWN)
         */
        static Direction GetOpposite(Direction dir);
    };
}
