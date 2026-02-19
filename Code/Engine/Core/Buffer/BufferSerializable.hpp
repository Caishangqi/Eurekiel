#pragma once
//-----------------------------------------------------------------------------------------------
// BufferSerializable.hpp
//
// Centralized traits specializations enabling ByteBuffer::Write<T> / Read<T>
// for engine math and vertex types. All specializations live here (not in math
// headers) to avoid reverse dependency from math module to serialization module.
//
// Supported types:
//   Vec2, Vec3, Vec4, IntVec2, IntVec3, IntVec4,
//   Rgba8, AABB2, AABB3, OBB2, OBB3, Plane2, Plane3,
//   Vertex_PCU, Vertex_PCUTBN
//
// Note: Rgb8 is not present in the engine; omitted intentionally.
//-----------------------------------------------------------------------------------------------

#include "Engine/Core/Buffer/ByteBuffer.hpp"

// Math types
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/IntVec4.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/OBB2.hpp"
#include "Engine/Math/OBB3.hpp"
#include "Engine/Math/Plane2.hpp"
#include "Engine/Math/Plane3.hpp"

// Core types
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

namespace enigma::core
{
    //===========================================================================================
    // Vec2
    //===========================================================================================
    template<>
    struct BufferSerializable<Vec2>
    {
        static void Serialize(ByteBuffer& buf, const Vec2& v)
        {
            buf.WriteFloat(v.x);
            buf.WriteFloat(v.y);
        }
        static Vec2 Deserialize(ByteBuffer& buf)
        {
            float x = buf.ReadFloat();
            float y = buf.ReadFloat();
            return Vec2(x, y);
        }
    };

    //===========================================================================================
    // Vec3
    //===========================================================================================
    template<>
    struct BufferSerializable<Vec3>
    {
        static void Serialize(ByteBuffer& buf, const Vec3& v)
        {
            buf.WriteFloat(v.x);
            buf.WriteFloat(v.y);
            buf.WriteFloat(v.z);
        }
        static Vec3 Deserialize(ByteBuffer& buf)
        {
            float x = buf.ReadFloat();
            float y = buf.ReadFloat();
            float z = buf.ReadFloat();
            return Vec3(x, y, z);
        }
    };

    //===========================================================================================
    // Vec4
    //===========================================================================================
    template<>
    struct BufferSerializable<Vec4>
    {
        static void Serialize(ByteBuffer& buf, const Vec4& v)
        {
            buf.WriteFloat(v.x);
            buf.WriteFloat(v.y);
            buf.WriteFloat(v.z);
            buf.WriteFloat(v.w);
        }
        static Vec4 Deserialize(ByteBuffer& buf)
        {
            float x = buf.ReadFloat();
            float y = buf.ReadFloat();
            float z = buf.ReadFloat();
            float w = buf.ReadFloat();
            return Vec4(x, y, z, w);
        }
    };

    //===========================================================================================
    // IntVec2
    //===========================================================================================
    template<>
    struct BufferSerializable<IntVec2>
    {
        static void Serialize(ByteBuffer& buf, const IntVec2& v)
        {
            buf.WriteInt(v.x);
            buf.WriteInt(v.y);
        }
        static IntVec2 Deserialize(ByteBuffer& buf)
        {
            int x = buf.ReadInt();
            int y = buf.ReadInt();
            return IntVec2(x, y);
        }
    };

    //===========================================================================================
    // IntVec3
    //===========================================================================================
    template<>
    struct BufferSerializable<IntVec3>
    {
        static void Serialize(ByteBuffer& buf, const IntVec3& v)
        {
            buf.WriteInt(v.x);
            buf.WriteInt(v.y);
            buf.WriteInt(v.z);
        }
        static IntVec3 Deserialize(ByteBuffer& buf)
        {
            int x = buf.ReadInt();
            int y = buf.ReadInt();
            int z = buf.ReadInt();
            return IntVec3(x, y, z);
        }
    };

    //===========================================================================================
    // IntVec4
    //===========================================================================================
    template<>
    struct BufferSerializable<IntVec4>
    {
        static void Serialize(ByteBuffer& buf, const IntVec4& v)
        {
            buf.WriteInt(v.x);
            buf.WriteInt(v.y);
            buf.WriteInt(v.z);
            buf.WriteInt(v.w);
        }
        static IntVec4 Deserialize(ByteBuffer& buf)
        {
            int x = buf.ReadInt();
            int y = buf.ReadInt();
            int z = buf.ReadInt();
            int w = buf.ReadInt();
            return IntVec4(x, y, z, w);
        }
    };

    //===========================================================================================
    // Rgba8 (per-byte, no endian conversion)
    //===========================================================================================
    template<>
    struct BufferSerializable<Rgba8>
    {
        static void Serialize(ByteBuffer& buf, const Rgba8& c)
        {
            buf.WriteByte(c.r);
            buf.WriteByte(c.g);
            buf.WriteByte(c.b);
            buf.WriteByte(c.a);
        }
        static Rgba8 Deserialize(ByteBuffer& buf)
        {
            unsigned char r = buf.ReadByte();
            unsigned char g = buf.ReadByte();
            unsigned char b = buf.ReadByte();
            unsigned char a = buf.ReadByte();
            return Rgba8(r, g, b, a);
        }
    };

    //===========================================================================================
    // AABB2 (decompose into two Vec2)
    //===========================================================================================
    template<>
    struct BufferSerializable<AABB2>
    {
        static void Serialize(ByteBuffer& buf, const AABB2& aabb)
        {
            BufferSerializable<Vec2>::Serialize(buf, aabb.m_mins);
            BufferSerializable<Vec2>::Serialize(buf, aabb.m_maxs);
        }
        static AABB2 Deserialize(ByteBuffer& buf)
        {
            Vec2 mins = BufferSerializable<Vec2>::Deserialize(buf);
            Vec2 maxs = BufferSerializable<Vec2>::Deserialize(buf);
            return AABB2(mins, maxs);
        }
    };

    //===========================================================================================
    // AABB3 (decompose into two Vec3)
    //===========================================================================================
    template<>
    struct BufferSerializable<AABB3>
    {
        static void Serialize(ByteBuffer& buf, const AABB3& aabb)
        {
            BufferSerializable<Vec3>::Serialize(buf, aabb.m_mins);
            BufferSerializable<Vec3>::Serialize(buf, aabb.m_maxs);
        }
        static AABB3 Deserialize(ByteBuffer& buf)
        {
            Vec3 mins = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3 maxs = BufferSerializable<Vec3>::Deserialize(buf);
            return AABB3(mins, maxs);
        }
    };

    //===========================================================================================
    // OBB2 (center + iBasisNormal + halfDimensions)
    //===========================================================================================
    template<>
    struct BufferSerializable<OBB2>
    {
        static void Serialize(ByteBuffer& buf, const OBB2& obb)
        {
            BufferSerializable<Vec2>::Serialize(buf, obb.m_center);
            BufferSerializable<Vec2>::Serialize(buf, obb.m_iBasisNormal);
            BufferSerializable<Vec2>::Serialize(buf, obb.m_halfDimensions);
        }
        static OBB2 Deserialize(ByteBuffer& buf)
        {
            Vec2 center    = BufferSerializable<Vec2>::Deserialize(buf);
            Vec2 iBasis    = BufferSerializable<Vec2>::Deserialize(buf);
            Vec2 halfDims  = BufferSerializable<Vec2>::Deserialize(buf);
            return OBB2(center, iBasis, halfDims);
        }
    };

    //===========================================================================================
    // OBB3 (center + halfDimensions + 3 basis normals)
    //===========================================================================================
    template<>
    struct BufferSerializable<OBB3>
    {
        static void Serialize(ByteBuffer& buf, const OBB3& obb)
        {
            BufferSerializable<Vec3>::Serialize(buf, obb.m_center);
            BufferSerializable<Vec3>::Serialize(buf, obb.m_halfDimensions);
            BufferSerializable<Vec3>::Serialize(buf, obb.m_iBasisNormal);
            BufferSerializable<Vec3>::Serialize(buf, obb.m_jBasisNormal);
            BufferSerializable<Vec3>::Serialize(buf, obb.m_kBasisNormal);
        }
        static OBB3 Deserialize(ByteBuffer& buf)
        {
            Vec3 center    = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3 halfDims  = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3 iBasis    = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3 jBasis    = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3 kBasis    = BufferSerializable<Vec3>::Deserialize(buf);
            return OBB3(center, halfDims, iBasis, jBasis, kBasis);
        }
    };

    //===========================================================================================
    // Plane2 (normal + distance)
    //===========================================================================================
    template<>
    struct BufferSerializable<Plane2>
    {
        static void Serialize(ByteBuffer& buf, const Plane2& p)
        {
            BufferSerializable<Vec2>::Serialize(buf, p.m_normal);
            buf.WriteFloat(p.m_distance);
        }
        static Plane2 Deserialize(ByteBuffer& buf)
        {
            Vec2  normal   = BufferSerializable<Vec2>::Deserialize(buf);
            float distance = buf.ReadFloat();
            return Plane2(normal, distance);
        }
    };

    //===========================================================================================
    // Plane3 (normal + distToPlaneAlongNormalFromOrigin)
    //===========================================================================================
    template<>
    struct BufferSerializable<Plane3>
    {
        static void Serialize(ByteBuffer& buf, const Plane3& p)
        {
            BufferSerializable<Vec3>::Serialize(buf, p.m_normal);
            buf.WriteFloat(p.m_distToPlaneAlongNormalFromOrigin);
        }
        static Plane3 Deserialize(ByteBuffer& buf)
        {
            Vec3  normal   = BufferSerializable<Vec3>::Deserialize(buf);
            float distance = buf.ReadFloat();
            return Plane3(normal, distance);
        }
    };

    //===========================================================================================
    // Vertex_PCU (position + color + uvTextCoords)
    //===========================================================================================
    template<>
    struct BufferSerializable<Vertex_PCU>
    {
        static void Serialize(ByteBuffer& buf, const Vertex_PCU& v)
        {
            BufferSerializable<Vec3>::Serialize(buf, v.m_position);
            BufferSerializable<Rgba8>::Serialize(buf, v.m_color);
            BufferSerializable<Vec2>::Serialize(buf, v.m_uvTextCoords);
        }
        static Vertex_PCU Deserialize(ByteBuffer& buf)
        {
            Vec3  pos   = BufferSerializable<Vec3>::Deserialize(buf);
            Rgba8 color = BufferSerializable<Rgba8>::Deserialize(buf);
            Vec2  uv    = BufferSerializable<Vec2>::Deserialize(buf);
            return Vertex_PCU(pos, color, uv);
        }
    };

    //===========================================================================================
    // Vertex_PCUTBN (position + color + uvTexCoords + tangent + bitangent + normal)
    //===========================================================================================
    template<>
    struct BufferSerializable<Vertex_PCUTBN>
    {
        static void Serialize(ByteBuffer& buf, const Vertex_PCUTBN& v)
        {
            BufferSerializable<Vec3>::Serialize(buf, v.m_position);
            BufferSerializable<Rgba8>::Serialize(buf, v.m_color);
            BufferSerializable<Vec2>::Serialize(buf, v.m_uvTexCoords);
            BufferSerializable<Vec3>::Serialize(buf, v.m_tangent);
            BufferSerializable<Vec3>::Serialize(buf, v.m_bitangent);
            BufferSerializable<Vec3>::Serialize(buf, v.m_normal);
        }
        static Vertex_PCUTBN Deserialize(ByteBuffer& buf)
        {
            Vec3  pos       = BufferSerializable<Vec3>::Deserialize(buf);
            Rgba8 color     = BufferSerializable<Rgba8>::Deserialize(buf);
            Vec2  uv        = BufferSerializable<Vec2>::Deserialize(buf);
            Vec3  tangent   = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3  bitangent = BufferSerializable<Vec3>::Deserialize(buf);
            Vec3  normal    = BufferSerializable<Vec3>::Deserialize(buf);
            return Vertex_PCUTBN(pos, color, uv, normal, tangent, bitangent);
        }
    };
} // namespace enigma::core
