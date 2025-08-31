#pragma once
#include "../Property/PropertyTypes.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace enigma::voxel::block
{
    using namespace enigma::voxel::property;
    
    /**
     * @brief 3D integer position for blocks in the world
     * 
     * Similar to Minecraft's BlockPos, represents a position in block coordinates.
     * Provides utility methods for getting neighboring positions.
     */
    struct BlockPos
    {
        int32_t x;
        int32_t y;
        int32_t z;
        
        // Constructors
        BlockPos() : x(0), y(0), z(0) {}
        BlockPos(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
        
        // Comparison operators
        bool operator==(const BlockPos& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
        
        bool operator!=(const BlockPos& other) const
        {
            return !(*this == other);
        }
        
        bool operator<(const BlockPos& other) const
        {
            if (x != other.x) return x < other.x;
            if (y != other.y) return y < other.y;
            return z < other.z;
        }
        
        // Arithmetic operators
        BlockPos operator+(const BlockPos& other) const
        {
            return BlockPos(x + other.x, y + other.y, z + other.z);
        }
        
        BlockPos operator-(const BlockPos& other) const
        {
            return BlockPos(x - other.x, y - other.y, z - other.z);
        }
        
        BlockPos& operator+=(const BlockPos& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }
        
        BlockPos& operator-=(const BlockPos& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }
        
        // Direction-based movement
        BlockPos GetRelative(Direction direction) const
        {
            switch (direction)
            {
                case Direction::NORTH: return BlockPos(x, y, z - 1);
                case Direction::SOUTH: return BlockPos(x, y, z + 1);
                case Direction::EAST:  return BlockPos(x + 1, y, z);
                case Direction::WEST:  return BlockPos(x - 1, y, z);
                case Direction::UP:    return BlockPos(x, y + 1, z);
                case Direction::DOWN:  return BlockPos(x, y - 1, z);
                default: return *this;
            }
        }
        
        // Convenience methods for common directions
        BlockPos North() const { return GetRelative(Direction::NORTH); }
        BlockPos South() const { return GetRelative(Direction::SOUTH); }
        BlockPos East() const { return GetRelative(Direction::EAST); }
        BlockPos West() const { return GetRelative(Direction::WEST); }
        BlockPos Up() const { return GetRelative(Direction::UP); }
        BlockPos Down() const { return GetRelative(Direction::DOWN); }
        
        // Get all 6 neighbors
        std::vector<BlockPos> GetNeighbors() const
        {
            return {
                North(), South(), East(), West(), Up(), Down()
            };
        }
        
        // Distance calculations
        double DistanceTo(const BlockPos& other) const
        {
            int32_t dx = x - other.x;
            int32_t dy = y - other.y;
            int32_t dz = z - other.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        
        int32_t ManhattanDistanceTo(const BlockPos& other) const
        {
            return std::abs(x - other.x) + std::abs(y - other.y) + std::abs(z - other.z);
        }
        
        // Chunk coordinates (assuming 16x16 chunks)
        int32_t GetChunkX() const { return x >> 4; }
        int32_t GetChunkZ() const { return z >> 4; }
        
        // Position within chunk (0-15)
        int32_t GetBlockXInChunk() const { return x & 15; }
        int32_t GetBlockZInChunk() const { return z & 15; }
        
        // Utility methods
        std::string ToString() const
        {
            return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
        }
        
        // Hash for use in unordered containers
        size_t GetHash() const
        {
            size_t hash = 0;
            hash ^= std::hash<int32_t>{}(x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int32_t>{}(y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int32_t>{}(z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
        
        // Static utility methods
        static BlockPos FromChunkCoords(int32_t chunkX, int32_t chunkZ, int32_t blockX, int32_t blockY, int32_t blockZ)
        {
            return BlockPos((chunkX << 4) + blockX, blockY, (chunkZ << 4) + blockZ);
        }
        
        static BlockPos Origin() { return BlockPos(0, 0, 0); }
    };
}

// Hash specialization for BlockPos
namespace std
{
    template<>
    struct hash<enigma::voxel::block::BlockPos>
    {
        size_t operator()(const enigma::voxel::block::BlockPos& pos) const
        {
            return pos.GetHash();
        }
    };
}
