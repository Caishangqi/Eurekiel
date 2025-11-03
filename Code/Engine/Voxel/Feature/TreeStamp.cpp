#include "Engine/Voxel/Feature/TreeStamp.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <algorithm>

using namespace enigma::registry::block;

//-----------------------------------------------------------------------------------------------
// Constructor
//
TreeStamp::TreeStamp(const std::vector<TreeStampBlock>& blocks)
    : m_blocks(blocks)
{
    CalculateDimensions();
}

//-----------------------------------------------------------------------------------------------
// Set blocks and recalculate dimensions
//
void TreeStamp::SetBlocks(const std::vector<TreeStampBlock>& blocks)
{
    m_blocks = blocks;
    CalculateDimensions();
}

//-----------------------------------------------------------------------------------------------
// Calculate max radius and height from block offsets
//
void TreeStamp::CalculateDimensions()
{
    if (m_blocks.empty())
    {
        m_maxRadius = 0;
        m_height    = 0;
        return;
    }

    m_maxRadius = 0;
    m_height    = 0;

    for (const TreeStampBlock& block : m_blocks)
    {
        // Calculate horizontal distance (X-Y plane in SimpleMiner coords)
        int horizontalDist = static_cast<int>(sqrtf(static_cast<float>(
            block.offset.x * block.offset.x +
            block.offset.y * block.offset.y)));
        m_maxRadius = std::max(m_maxRadius, horizontalDist);

        // Height is Z coordinate in SimpleMiner
        m_height = std::max(m_height, block.offset.z);
    }
}

//-----------------------------------------------------------------------------------------------
// Initialize Block IDs from BlockRegistry
//
void TreeStamp::InitializeBlockIds(const std::string& logName, const std::string& leavesName)
{
    // Query Block IDs from BlockRegistry and cache them
    int logId    = BlockRegistry::GetBlockId("simpleminer", logName);
    int leavesId = BlockRegistry::GetBlockId("simpleminer", leavesName);

    m_blockIds[BlockPart::LOG]    = logId;
    m_blockIds[BlockPart::LEAVES] = leavesId;
}

//-----------------------------------------------------------------------------------------------
// Get Log Block ID (from cache)
//
int TreeStamp::GetLogBlockId() const
{
    auto it = m_blockIds.find(BlockPart::LOG);
    if (it != m_blockIds.end())
    {
        return it->second;
    }
    return -1; // Not initialized
}

//-----------------------------------------------------------------------------------------------
// Get Leaves Block ID (from cache)
//
int TreeStamp::GetLeavesBlockId() const
{
    auto it = m_blockIds.find(BlockPart::LEAVES);
    if (it != m_blockIds.end())
    {
        return it->second;
    }
    return -1; // Not initialized
}
