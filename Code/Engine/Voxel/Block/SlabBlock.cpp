#include "SlabBlock.hpp"
#include "VoxelShape.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Voxel/Block/BlockState.hpp"
#include "Engine/Voxel/Property/PropertyMap.hpp"
#include "Engine/Voxel/World/World.hpp"

namespace enigma::voxel
{
    SlabBlock::SlabBlock(const std::string& registryName, const std::string& ns)
        : Block(registryName, ns)
    {
        // [NEW] Create EnumProperty for slab type with 3 values (bottom, top, double)
        m_typeProperty = std::make_shared<EnumProperty<SlabType>>(
            "type",
            std::vector<SlabType>{SlabType::BOTTOM, SlabType::TOP, SlabType::DOUBLE},
            SlabType::BOTTOM, // Default to BOTTOM
            SlabTypeToString, // Enum to string converter
            StringToSlabType // String to enum converter
        );

        // [NEW] Register the property
        AddProperty(m_typeProperty);

        // [NEW] Set block-level properties
        // Only DOUBLE slabs are opaque, so block-level opaque = false
        SetCanOcclude(false);
        SetFullBlock(false); // Slabs are not full blocks (except DOUBLE, but YAML can override)

        // [NEW] Generate all possible block states (3 states: BOTTOM, TOP, DOUBLE)
        GenerateBlockStates();
    }

    enigma::voxel::BlockState* SlabBlock::GetStateForPlacement(const PlacementContext& ctx) const
    {
        core::LogInfo("SlabBlock", "[DEBUG] GetStateForPlacement() called for %s:%s",
                      m_namespace.c_str(), m_registryName.c_str());
        core::LogInfo("SlabBlock", "  hitPoint.z = %.3f, IsTopHalf() = %s",
                      ctx.hitPoint.z, ctx.IsTopHalf() ? "true" : "false");

        // [FIX] Check if merging to double slab at CLICKED position
        // Minecraft logic: Check clickedPos (the existing slab) NOT targetPos
        BlockState* existingState = ctx.world->GetBlockState(ctx.clickedPos);
        if (existingState && CanBeReplaced(existingState, ctx))
        {
            core::LogInfo("SlabBlock", "  [MERGE] Creating DOUBLE slab");
            // Create double slab state
            PropertyMap props;
            props.Set(std::static_pointer_cast<Property<SlabType>>(m_typeProperty), SlabType::DOUBLE);

            BlockState* result = GetState(props);
            core::LogInfo("SlabBlock", "  [MERGE] GetState() returned: %s, properties='%s'",
                          result ? "valid" : "NULL",
                          result ? result->GetProperties().ToString().c_str() : "N/A");
            return result;
        }

        // [FIX] Normal placement - determine slab type based on clicked face and hit point
        // Minecraft logic (SlabBlock.java line 73-74):
        // - Click UP face → always BOTTOM (place slab at bottom of target block)
        // - Click DOWN face → always TOP (place slab at top of target block)
        // - Click horizontal face → use hitPoint.z to determine
        SlabType type;
        if (ctx.clickedFace == Direction::UP)
        {
            type = SlabType::BOTTOM; // Clicked top of block below, place bottom slab
        }
        else if (ctx.clickedFace == Direction::DOWN)
        {
            type = SlabType::TOP; // Clicked bottom of block above, place top slab
        }
        else
        {
            // Horizontal face: use hit point Z to determine
            type = ctx.IsTopHalf() ? SlabType::TOP : SlabType::BOTTOM;
        }

        core::LogInfo("SlabBlock", "  [NORMAL] Placing %s slab", SlabTypeToString(type));

        // [NEW] Create property map with selected type
        PropertyMap props;
        props.Set(std::static_pointer_cast<Property<SlabType>>(m_typeProperty), type);

        core::LogInfo("SlabBlock", "  PropertyMap before GetState(): '%s'", props.ToString().c_str());

        // [NEW] Return the BlockState matching these properties
        BlockState* result = GetState(props);

        core::LogInfo("SlabBlock", "  GetState() returned: %s", result ? "valid" : "NULL");
        if (result)
        {
            core::LogInfo("SlabBlock", "  Result properties: '%s'", result->GetProperties().ToString().c_str());
        }

        return result;
    }

    bool SlabBlock::CanBeReplaced(enigma::voxel::BlockState* state, const PlacementContext& ctx) const
    {
        // [FIX] First check if the clicked block IS a slab of the same type
        // Without this check, clicking on grass/stone would incorrectly trigger merge!
        if (state->GetBlock() != this)
        {
            return false; // Clicked block is not the same slab type
        }

        // [CHECK] Must be placing the same slab type (holding same item)
        if (ctx.heldItemBlock != this)
        {
            return false; // Different block type in hand, cannot merge
        }

        // [CHECK] Get current slab type from state
        SlabType currentType = state->Get(std::static_pointer_cast<Property<SlabType>>(m_typeProperty));

        // [CHECK] Cannot replace DOUBLE slabs (already full)
        if (currentType == SlabType::DOUBLE)
        {
            return false; // Already a full block
        }

        // [NEW] Check if can merge to DOUBLE slab
        // Merge conditions:
        // 1. BOTTOM slab + click top half (isTopHit=true) → DOUBLE
        // 2. TOP slab + click bottom half (isTopHit=false) → DOUBLE
        bool isTopHit = ctx.IsTopHalf();

        // [LOGIC] Return true if clicking opposite half of existing slab
        return (currentType == SlabType::BOTTOM && isTopHit) ||
            (currentType == SlabType::TOP && !isTopHit);
    }

    bool SlabBlock::IsOpaque(enigma::voxel::BlockState* state) const
    {
        // [NEW] Per-state opacity check
        // Only DOUBLE slabs are opaque and block light
        SlabType type = state->Get(std::static_pointer_cast<Property<SlabType>>(m_typeProperty));
        return IsSlabOpaque(type); // Use helper function from SlabType.hpp
    }

    VoxelShape SlabBlock::GetCollisionShape(enigma::voxel::BlockState* state) const
    {
        // [NEW] Get collision shape based on slab type
        SlabType type = state->Get(std::static_pointer_cast<Property<SlabType>>(m_typeProperty));

        switch (type)
        {
        case SlabType::BOTTOM:
            // Bottom slab: Z from 0 to 0.5
            return Shapes::SlabBottom();
        case SlabType::TOP:
            // Top slab: Z from 0.5 to 1.0
            return Shapes::SlabTop();
        case SlabType::DOUBLE:
            // Double slab: full block
            return Shapes::FullBlock();
        default:
            return Shapes::Empty();
        }
    }

    std::string SlabBlock::GetModelPath(enigma::voxel::BlockState* state) const
    {
        // [NEW] Get slab type from state
        SlabType type = state->Get(std::static_pointer_cast<Property<SlabType>>(m_typeProperty));

        // [NEW] Build model path based on type
        // Format: "{namespace}:block/{registryName}_{type}"
        // Example: "simpleminer:block/oak_slab_bottom"
        std::string typeSuffix = SlabTypeToString(type);
        std::string modelPath  = m_namespace + ":block/" + m_registryName + "_" + typeSuffix;

        return modelPath;
    }

    void SlabBlock::InitializeState(enigma::voxel::BlockState* state, const PropertyMap& properties)
    {
        // [NOTE] No custom initialization needed beyond base class
        // Base class already sets up the PropertyMap correctly
        Block::InitializeState(state, properties);
    }
}
