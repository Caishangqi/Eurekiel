#pragma once
#include "Engine/Math/IntVec3.hpp"
#include <vector>
#include <string>
#include <unordered_map>

//-----------------------------------------------------------------------------------------------
// TreeStampBlock: Represents a single block in a tree template
//
struct TreeStampBlock
{
    IntVec3 offset; // Offset from tree origin (trunk base)
    int     blockId; // Block type ID (e.g., log, leaves)

    TreeStampBlock() = default;

    TreeStampBlock(const IntVec3& offset, int blockId)
        : offset(offset), blockId(blockId)
    {
    }
};

//-----------------------------------------------------------------------------------------------
// TreeStamp: Base class for tree templates
//
// This is an abstract base class in the Engine layer. Game-specific tree types
// (Oak, Birch, Spruce, etc.) should derive from this class in the Game layer.
//
// Coordinate Convention:
// - Minecraft: X=East, Y=Up, Z=North
// - SimpleMiner: X=Forward, Y=Left, Z=Up
// - Conversion: MC.Y -> SM.Z, MC.Z -> SM.Y, MC.X -> SM.X
//
// Block ID Management:
// - Derived classes should query Block IDs from BlockRegistry using namespace+name
// - Never hardcode Block IDs - they may change between game sessions
//
class TreeStamp
{
public:
    // Block part enumeration for cache management
    enum class BlockPart
    {
        LOG,
        LEAVES
    };

public:
    TreeStamp() = default;
    TreeStamp(const std::vector<TreeStampBlock>& blocks);
    virtual ~TreeStamp() = default;

    // Accessors
    const std::vector<TreeStampBlock>& GetBlocks() const { return m_blocks; }
    int                                GetMaxRadius() const { return m_maxRadius; }
    int                                GetHeight() const { return m_height; }

    // Virtual interface for derived classes
    virtual std::string GetTypeName() const = 0; // e.g., "Oak", "Birch"
    virtual std::string GetSizeName() const { return "Medium"; } // e.g., "Small", "Medium", "Large"

    // Block name interface (to be implemented by derived classes)
    // Returns the block name for the log/trunk block (e.g., "oak_log")
    virtual std::string GetLogBlockName() const = 0;

    // Returns the block name for the leaves block (e.g., "oak_leaves")
    virtual std::string GetLeavesBlockName() const = 0;

protected:
    void SetBlocks(const std::vector<TreeStampBlock>& blocks);
    void CalculateDimensions();

    // Block ID cache management
    void InitializeBlockIds(const std::string& logName, const std::string& leavesName);
    int  GetLogBlockId() const;
    int  GetLeavesBlockId() const;

protected:
    std::vector<TreeStampBlock> m_blocks;
    int                         m_maxRadius = 0; // Maximum horizontal distance from origin
    int                         m_height    = 0; // Maximum vertical height

    // Block ID cache (initialized on first access)
    std::unordered_map<BlockPart, int> m_blockIds;
};
