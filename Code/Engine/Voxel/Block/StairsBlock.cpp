#include "StairsBlock.hpp"

#include "PlacementContext.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Block/BlockIterator.hpp"
#include "Engine/Voxel/Block/BlockPos.hpp"
#include "Engine/Voxel/Property/PropertyMap.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

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

        // [DEBUG] Log input parameters
        LogInfo("Stairs", "[GetStairsShape] pos=%s facing=%s half=%s",
                pos.ToString().c_str(), DirectionToString(facing).c_str(), HalfTypeToString(half));

        // [STEP 1] Check front neighbor (in facing direction)
        BlockPos                   frontPos   = pos.GetRelative(facing);
        enigma::voxel::BlockState* frontState = world->GetBlockState(frontPos);

        LogInfo("Stairs", "  [STEP1] frontPos=%s (in facing dir %s)",
                frontPos.ToString().c_str(), DirectionToString(facing).c_str());

        if (frontState && IsStairs(frontState))
        {
            // [CHECK] Must have same half
            HalfType frontHalf = frontState->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
            LogInfo("Stairs", "    frontState is stairs, frontHalf=%s", HalfTypeToString(frontHalf));

            if (frontHalf == half)
            {
                // [CHECK] Must have perpendicular axis
                Direction frontFacing    = frontState->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
                Direction oppositeFacing = GetOpposite(facing);

                LogInfo("Stairs", "    frontFacing=%s oppositeFacing=%s",
                        DirectionToString(frontFacing).c_str(), DirectionToString(oppositeFacing).c_str());

                // Check if perpendicular (different axis)
                bool frontIsNS  = (frontFacing == Direction::NORTH || frontFacing == Direction::SOUTH);
                bool facingIsNS = (facing == Direction::NORTH || facing == Direction::SOUTH);
                LogInfo("Stairs", "    frontIsNS=%d facingIsNS=%d perpendicular=%d",
                        frontIsNS, facingIsNS, frontIsNS != facingIsNS);

                if (frontIsNS != facingIsNS)
                {
                    // [VALIDATE] Check if can form outer corner
                    // Minecraft: canTakeShape(blockState, blockGetter, blockPos, direction2.getOpposite())
                    // - blockState = current stair state
                    // - blockPos = current stair position
                    // - direction2.getOpposite() = opposite of front neighbor's facing
                    Direction checkDir = GetOpposite(frontFacing);

                    // We need to create current stair's state for CanTakeShape check
                    PropertyMap currentProps;
                    currentProps.Set(std::static_pointer_cast<Property<Direction>>(m_facingProperty), facing);
                    currentProps.Set(std::static_pointer_cast<Property<HalfType>>(m_halfProperty), half);
                    currentProps.Set(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty), StairsShape::STRAIGHT);
                    enigma::voxel::BlockState* currentState = GetState(currentProps);

                    bool canTake = CanTakeShape(currentState, world, pos, checkDir);
                    LogInfo("Stairs", "    CanTakeShape(checkDir=%s)=%d",
                            DirectionToString(checkDir).c_str(), canTake);

                    if (canTake)
                    {
                        // [DETERMINE] Left or right outer corner
                        Direction ccw = GetCounterClockWise(facing);
                        LogInfo("Stairs", "    CCW(facing=%s)=%s frontFacing==CCW? %d",
                                DirectionToString(facing).c_str(), DirectionToString(ccw).c_str(), frontFacing == ccw);

                        if (frontFacing == ccw)
                        {
                            LogInfo("Stairs", "    => OUTER_LEFT");
                            return StairsShape::OUTER_LEFT;
                        }
                        LogInfo("Stairs", "    => OUTER_RIGHT");
                        return StairsShape::OUTER_RIGHT;
                    }
                }
            }
        }
        else
        {
            LogInfo("Stairs", "    frontState=%s", frontState ? "not stairs" : "null");
        }

        // [STEP 2] Check back neighbor (opposite facing direction)
        Direction                  backDirection = GetOpposite(facing);
        BlockPos                   backPos       = pos.GetRelative(backDirection);
        enigma::voxel::BlockState* backState     = world->GetBlockState(backPos);

        LogInfo("Stairs", "  [STEP2] backPos=%s (in dir %s)",
                backPos.ToString().c_str(), DirectionToString(backDirection).c_str());

        if (backState && IsStairs(backState))
        {
            // [CHECK] Must have same half
            HalfType backHalf = backState->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
            LogInfo("Stairs", "    backState is stairs, backHalf=%s", HalfTypeToString(backHalf));

            if (backHalf == half)
            {
                // [CHECK] Must have perpendicular axis
                Direction backFacing = backState->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));

                LogInfo("Stairs", "    backFacing=%s", DirectionToString(backFacing).c_str());

                // Check if perpendicular (different axis)
                bool backIsNS   = (backFacing == Direction::NORTH || backFacing == Direction::SOUTH);
                bool facingIsNS = (facing == Direction::NORTH || facing == Direction::SOUTH);
                LogInfo("Stairs", "    backIsNS=%d facingIsNS=%d perpendicular=%d",
                        backIsNS, facingIsNS, backIsNS != facingIsNS);

                if (backIsNS != facingIsNS)
                {
                    // [VALIDATE] Check if can form inner corner
                    // Minecraft: canTakeShape(blockState, blockGetter, blockPos, direction3)
                    // - blockState = current stair state
                    // - blockPos = current stair position
                    // - direction3 = back neighbor's facing direction
                    Direction checkDir = backFacing;

                    // We need to create current stair's state for CanTakeShape check
                    PropertyMap currentProps;
                    currentProps.Set(std::static_pointer_cast<Property<Direction>>(m_facingProperty), facing);
                    currentProps.Set(std::static_pointer_cast<Property<HalfType>>(m_halfProperty), half);
                    currentProps.Set(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty), StairsShape::STRAIGHT);
                    enigma::voxel::BlockState* currentState = GetState(currentProps);

                    bool canTake = CanTakeShape(currentState, world, pos, checkDir);
                    LogInfo("Stairs", "    CanTakeShape(checkDir=%s)=%d",
                            DirectionToString(checkDir).c_str(), canTake);

                    if (canTake)
                    {
                        // [DETERMINE] Left or right inner corner
                        Direction ccw = GetCounterClockWise(facing);
                        LogInfo("Stairs", "    CCW(facing=%s)=%s backFacing==CCW? %d",
                                DirectionToString(facing).c_str(), DirectionToString(ccw).c_str(), backFacing == ccw);

                        if (backFacing == ccw)
                        {
                            LogInfo("Stairs", "    => INNER_LEFT");
                            return StairsShape::INNER_LEFT;
                        }
                        LogInfo("Stairs", "    => INNER_RIGHT");
                        return StairsShape::INNER_RIGHT;
                    }
                }
            }
        }
        else
        {
            LogInfo("Stairs", "    backState=%s", backState ? "not stairs" : "null");
        }

        // [DEFAULT] No matching neighbors or parallel alignment
        LogInfo("Stairs", "  => STRAIGHT (default)");
        return StairsShape::STRAIGHT;
    }

    void StairsBlock::OnNeighborChanged(World* world, const BlockPos& pos, enigma::voxel::BlockState* state, registry::block::Block* neighborBlock)
    {
        LogInfo("Stairs", "[OnNeighborChanged] pos=%s neighborBlock=%s",
                pos.ToString().c_str(),
                neighborBlock ? neighborBlock->GetRegistryName().c_str() : "null");

        // [CHECK] Only update if neighbor is also a stairs block
        // This prevents unnecessary updates from other block types
        auto* neighborStairs = dynamic_cast<StairsBlock*>(neighborBlock);
        if (!neighborStairs)
        {
            LogInfo("Stairs", "  => Skipped (neighbor is not stairs)");
            return; // Different block type, no shape update needed
        }

        // [GET] Current properties
        Direction   facing   = state->Get(std::static_pointer_cast<Property<Direction>>(m_facingProperty));
        HalfType    half     = state->Get(std::static_pointer_cast<Property<HalfType>>(m_halfProperty));
        StairsShape oldShape = state->Get(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty));

        LogInfo("Stairs", "  Current: facing=%s half=%s shape=%s",
                DirectionToString(facing).c_str(), HalfTypeToString(half), StairsShapeToString(oldShape));

        // [RECALCULATE] Shape based on new neighbor configuration
        StairsShape newShape = GetStairsShape(facing, half, world, pos);

        // [CHECK] Only update if shape actually changed
        // This prevents infinite recursion when placing adjacent stairs
        if (newShape != oldShape)
        {
            LogInfo("Stairs", "  => Shape changed: %s -> %s, updating state",
                    StairsShapeToString(oldShape), StairsShapeToString(newShape));

            // [UPDATE] Create new state with updated shape
            PropertyMap props;
            props.Set(std::static_pointer_cast<Property<Direction>>(m_facingProperty), facing);
            props.Set(std::static_pointer_cast<Property<HalfType>>(m_halfProperty), half);
            props.Set(std::static_pointer_cast<Property<StairsShape>>(m_shapeProperty), newShape);

            enigma::voxel::BlockState* newState = GetState(props);

            // [APPLY] Set the new state (triggers mesh rebuild)
            world->SetBlockState(pos, newState);
        }
        else
        {
            LogInfo("Stairs", "  => Shape unchanged: %s", StairsShapeToString(oldShape));
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
        // [ROTATION] Counter-clockwise rotation (from above, looking down at XY plane)
        //
        // [COORDINATE SYSTEM ANALYSIS]
        // SimpleMiner: +X=Forward, +Y=Left, +Z=Up
        // Looking down from +Z axis (bird's eye view):
        //   - NORTH = +Y (up on screen)
        //   - SOUTH = -Y (down on screen)
        //   - EAST  = +X (right on screen)
        //   - WEST  = -X (left on screen)
        //
        // Counter-clockwise from +Z looking down:
        //   NORTH → WEST → SOUTH → EAST → NORTH
        //
        // This matches Minecraft's CCW because both look down the "up" axis.
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
        // [ROTATION] Clockwise rotation (from above, looking down at XY plane)
        // Opposite of GetCounterClockWise:
        //   NORTH → EAST → SOUTH → WEST → NORTH
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
