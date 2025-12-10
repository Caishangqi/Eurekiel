#include "StairsBlock.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Block/BlockIterator.hpp"
#include "Engine/Voxel/Block/BlockPos.hpp"
#include "Engine/Voxel/Property/PropertyMap.hpp"
#include "Engine/Voxel/World/World.hpp"

namespace enigma::voxel
{
    StairsBlock::StairsBlock(const std::string& registryName, const std::string& ns)
        : Block(registryName, ns)
    {
        // [NEW] Create DirectionProperty for horizontal facing (4 directions)
        m_facingProperty = DirectionProperty::CreateHorizontal("facing");

        // [NEW] Create EnumProperty for half type (top/bottom)
        m_halfProperty = std::make_shared<EnumProperty<HalfType>>(
            "half",
            std::vector<HalfType>{HalfType::BOTTOM, HalfType::TOP},
            HalfType::BOTTOM, // Default to BOTTOM
            HalfTypeToString,
            StringToHalfType
        );

        // [NEW] Create EnumProperty for stairs shape (5 variants)
        m_shapeProperty = std::make_shared<EnumProperty<StairsShape>>(
            "shape",
            std::vector<StairsShape>{
                StairsShape::STRAIGHT,
                StairsShape::INNER_LEFT,
                StairsShape::INNER_RIGHT,
                StairsShape::OUTER_LEFT,
                StairsShape::OUTER_RIGHT
            },
            StairsShape::STRAIGHT, // Default to STRAIGHT
            StairsShapeToString,
            StringToStairsShape
        );

        // [NEW] Register all properties
        AddProperty(m_facingProperty);
        AddProperty(m_halfProperty);
        AddProperty(m_shapeProperty);

        // [NEW] Set block-level properties
        // Stairs are not fully opaque (have empty spaces in corners)
        SetOpaque(false);
        SetFullBlock(false);

        // [NEW] Generate all possible block states
        // 4 (facing) × 2 (half) × 5 (shape) = 40 states
        GenerateBlockStates();
    }

    enigma::voxel::BlockState* StairsBlock::GetStateForPlacement(const PlacementContext& ctx) const
    {
        // [NEW] Determine facing from player look direction
        Direction facing = ctx.GetHorizontalFacing();

        // [NEW] Determine half from raycast hit point
        HalfType half = ctx.IsTopHalf() ? HalfType::TOP : HalfType::BOTTOM;

        // [NEW] Calculate shape based on adjacent stairs
        StairsShape shape = GetStairsShape(facing, half, ctx.world, ctx.targetPos);

        // [NEW] Create property map with all properties
        PropertyMap props;
        props.Set(std::static_pointer_cast<Property<Direction>>(m_facingProperty), facing);
        props.Set(std::static_pointer_cast<Property<HalfType>>(m_halfProperty), half);
        props.Set(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty), shape);

        // [NEW] Return the BlockState matching these properties
        return GetState(props);
    }

    StairsShape StairsBlock::GetStairsShape(Direction facing, HalfType half, World* world, const BlockPos& pos) const
    {
        // [ALGORITHM] Reference: Minecraft StairBlock.java getStairsShape() lines 126-153

        // [STEP 1] Check front neighbor (in facing direction)
        BlockPos                   frontPos   = pos.GetRelative(facing);
        enigma::voxel::BlockState* frontState = world->GetBlockState(frontPos);

        if (frontState && IsStairs(frontState))
        {
            // [CHECK] Must have same half
            HalfType frontHalf = frontState->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
            if (frontHalf == half)
            {
                // [CHECK] Must have perpendicular axis
                Direction frontFacing    = frontState->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
                Direction oppositeFacing = GetOpposite(facing);

                // Check if perpendicular (different axis)
                if ((frontFacing == Direction::NORTH || frontFacing == Direction::SOUTH) !=
                    (facing == Direction::NORTH || facing == Direction::SOUTH))
                {
                    // [VALIDATE] Check if can form outer corner
                    if (CanTakeShape(frontState, world, frontPos, oppositeFacing))
                    {
                        // [DETERMINE] Left or right outer corner
                        if (frontFacing == GetCounterClockWise(facing))
                        {
                            return StairsShape::OUTER_LEFT;
                        }
                        return StairsShape::OUTER_RIGHT;
                    }
                }
            }
        }

        // [STEP 2] Check back neighbor (opposite facing direction)
        Direction                  backDirection = GetOpposite(facing);
        BlockPos                   backPos       = pos.GetRelative(backDirection);
        enigma::voxel::BlockState* backState     = world->GetBlockState(backPos);

        if (backState && IsStairs(backState))
        {
            // [CHECK] Must have same half
            HalfType backHalf = backState->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
            if (backHalf == half)
            {
                // [CHECK] Must have perpendicular axis
                Direction backFacing = backState->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));

                // Check if perpendicular (different axis)
                if ((backFacing == Direction::NORTH || backFacing == Direction::SOUTH) !=
                    (facing == Direction::NORTH || facing == Direction::SOUTH))
                {
                    // [VALIDATE] Check if can form inner corner
                    if (CanTakeShape(backState, world, backPos, backFacing))
                    {
                        // [DETERMINE] Left or right inner corner
                        if (backFacing == GetCounterClockWise(facing))
                        {
                            return StairsShape::INNER_LEFT;
                        }
                        return StairsShape::INNER_RIGHT;
                    }
                }
            }
        }

        // [DEFAULT] No matching neighbors or parallel alignment
        return StairsShape::STRAIGHT;
    }

    void StairsBlock::OnNeighborChanged(World* world, const BlockPos& pos, enigma::voxel::BlockState* state, registry::block::Block* neighborBlock)
    {
        // [CHECK] Only update if neighbor is also a stairs block
        // This prevents unnecessary updates from other block types
        if (neighborBlock != this)
        {
            return; // Different block type, no shape update needed
        }

        // [GET] Current properties
        Direction   facing   = state->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
        HalfType    half     = state->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
        StairsShape oldShape = state->Get(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty));

        // [RECALCULATE] Shape based on new neighbor configuration
        StairsShape newShape = GetStairsShape(facing, half, world, pos);

        // [CHECK] Only update if shape actually changed
        // This prevents infinite recursion when placing adjacent stairs
        if (newShape != oldShape)
        {
            // [UPDATE] Create new state with updated shape
            PropertyMap props;
            props.Set(std::static_pointer_cast<Property<Direction>>(m_facingProperty), facing);
            props.Set(std::static_pointer_cast<Property<HalfType>>(m_halfProperty), half);
            props.Set(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty), newShape);

            enigma::voxel::BlockState* newState = GetState(props);

            // [APPLY] Set the new state (triggers mesh rebuild)
            world->SetBlockState(pos, newState);
        }
    }

    std::string StairsBlock::GetModelPath(enigma::voxel::BlockState* state) const
    {
        // [NOTE] Stairs use base model path
        // Shape variants are handled by blockstate JSON multipart
        // Format: "{namespace}:block/{registryName}"
        // Example: "simpleminer:block/oak_stairs"
        UNUSED(state);
        std::string modelPath = m_namespace + ":block/" + m_registryName;
        return modelPath;
    }

    void StairsBlock::InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties)
    {
        // [NOTE] No custom initialization needed beyond base class
        // Base class already sets up the PropertyMap correctly
        Block::InitializeState(state, properties);
    }

    bool StairsBlock::IsStairs(enigma::voxel::BlockState* state)
    {
        // [CHECK] If block type is StairsBlock
        if (!state)
        {
            return false;
        }

        registry::block::Block* block = state->GetBlock();
        return dynamic_cast<StairsBlock*>(block) != nullptr;
    }

    bool StairsBlock::CanTakeShape(enigma::voxel::BlockState* state, World* world, const BlockPos& pos, Direction neighborDir) const
    {
        // [ALGORITHM] Reference: Minecraft StairBlock.java canTakeShape() lines 155-158
        // Returns true if no interfering stairs prevents corner formation

        BlockPos                   neighborPos   = pos.GetRelative(neighborDir);
        enigma::voxel::BlockState* neighborState = world->GetBlockState(neighborPos);

        // [CHECK] If neighbor is not stairs, can form corner
        if (!neighborState || !IsStairs(neighborState))
        {
            return true;
        }

        // [CHECK] If neighbor stairs has different facing, can form corner
        Direction neighborFacing = neighborState->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
        Direction currentFacing  = state->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
        if (neighborFacing != currentFacing)
        {
            return true;
        }

        // [CHECK] If neighbor stairs has different half, can form corner
        HalfType neighborHalf = neighborState->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
        HalfType currentHalf  = state->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
        if (neighborHalf != currentHalf)
        {
            return true;
        }

        // [BLOCKED] Neighbor stairs has same facing and half, blocks corner formation
        return false;
    }

    Direction StairsBlock::GetCounterClockWise(Direction dir)
    {
        // [ROTATION] Counter-clockwise (left turn)
        // NORTH → WEST → SOUTH → EAST → NORTH
        switch (dir)
        {
        case Direction::NORTH: return Direction::WEST;
        case Direction::WEST: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::EAST;
        case Direction::EAST: return Direction::NORTH;
        default: return dir; // UP/DOWN unchanged
        }
    }

    Direction StairsBlock::GetClockWise(Direction dir)
    {
        // [ROTATION] Clockwise (right turn)
        // NORTH → EAST → SOUTH → WEST → NORTH
        switch (dir)
        {
        case Direction::NORTH: return Direction::EAST;
        case Direction::EAST: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::WEST;
        case Direction::WEST: return Direction::NORTH;
        default: return dir; // UP/DOWN unchanged
        }
    }

    Direction StairsBlock::GetOpposite(Direction dir)
    {
        // [OPPOSITE] Reverse direction
        // NORTH ↔ SOUTH, EAST ↔ WEST, UP ↔ DOWN
        switch (dir)
        {
        case Direction::NORTH: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::NORTH;
        case Direction::EAST: return Direction::WEST;
        case Direction::WEST: return Direction::EAST;
        case Direction::UP: return Direction::DOWN;
        case Direction::DOWN: return Direction::UP;
        default: return dir;
        }
    }
}
