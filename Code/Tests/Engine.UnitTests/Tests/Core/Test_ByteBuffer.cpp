#include <gtest/gtest.h>

#include "Engine/Core/Buffer/ByteBuffer.hpp"
#include "Engine/Core/Buffer/BufferSerializable.hpp"
#include "Engine/Core/Buffer/BufferExceptions.hpp"

#include <cmath>
#include <limits>

using namespace enigma::core;

//=============================================================================
// PrimitiveRoundTrip
//=============================================================================

TEST(PrimitiveRoundTrip, WriteBool_ReadBool)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteBool(true);
    buf.WriteBool(false);
    buf.Rewind();
    EXPECT_TRUE(buf.ReadBool());
    EXPECT_FALSE(buf.ReadBool());
}

TEST(PrimitiveRoundTrip, WriteInt_ReadInt_LittleEndian)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(0x12345678);
    const byte_t* data = buf.Data();
    EXPECT_EQ(data[0], 0x78);
    EXPECT_EQ(data[1], 0x56);
    EXPECT_EQ(data[2], 0x34);
    EXPECT_EQ(data[3], 0x12);
    buf.Rewind();
    EXPECT_EQ(buf.ReadInt(), 0x12345678);
}

TEST(PrimitiveRoundTrip, WriteInt_ReadInt_BigEndian)
{
    ByteBuffer buf(ByteOrder::Big);
    buf.WriteInt(0x12345678);
    const byte_t* data = buf.Data();
    EXPECT_EQ(data[0], 0x12);
    EXPECT_EQ(data[1], 0x34);
    EXPECT_EQ(data[2], 0x56);
    EXPECT_EQ(data[3], 0x78);
    buf.Rewind();
    EXPECT_EQ(buf.ReadInt(), 0x12345678);
}

TEST(PrimitiveRoundTrip, WriteFloat_ReadFloat)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteFloat(3.14f);
    buf.WriteFloat(std::numeric_limits<float>::infinity());
    buf.WriteFloat(-std::numeric_limits<float>::infinity());
    buf.WriteFloat(std::numeric_limits<float>::quiet_NaN());
    buf.Rewind();
    EXPECT_FLOAT_EQ(buf.ReadFloat(), 3.14f);
    EXPECT_TRUE(std::isinf(buf.ReadFloat()));
    EXPECT_TRUE(std::isinf(buf.ReadFloat()));
    EXPECT_TRUE(std::isnan(buf.ReadFloat()));
}

TEST(PrimitiveRoundTrip, WriteDouble_ReadDouble)
{
    ByteBuffer buf(ByteOrder::Big);
    buf.WriteDouble(2.718281828459045);
    buf.WriteDouble(std::numeric_limits<double>::infinity());
    buf.WriteDouble(std::numeric_limits<double>::quiet_NaN());
    buf.Rewind();
    EXPECT_DOUBLE_EQ(buf.ReadDouble(), 2.718281828459045);
    EXPECT_TRUE(std::isinf(buf.ReadDouble()));
    EXPECT_TRUE(std::isnan(buf.ReadDouble()));
}

TEST(PrimitiveRoundTrip, AllPrimitiveTypes)
{
    ByteBuffer buf(ByteOrder::Little);

    buf.WriteBool(true);
    buf.WriteByte(0xAB);
    buf.WriteSignedByte(-42);
    buf.WriteShort(-1234);
    buf.WriteUnsignedShort(60000);
    buf.WriteInt(-100000);
    buf.WriteUnsignedInt(3000000000u);
    buf.WriteLong(-9876543210LL);
    buf.WriteUnsignedLong(18000000000000000000ULL);
    buf.WriteFloat(1.5f);
    buf.WriteDouble(99.99);

    buf.Rewind();

    EXPECT_TRUE(buf.ReadBool());
    EXPECT_EQ(buf.ReadByte(), 0xAB);
    EXPECT_EQ(buf.ReadSignedByte(), -42);
    EXPECT_EQ(buf.ReadShort(), -1234);
    EXPECT_EQ(buf.ReadUnsignedShort(), 60000);
    EXPECT_EQ(buf.ReadInt(), -100000);
    EXPECT_EQ(buf.ReadUnsignedInt(), 3000000000u);
    EXPECT_EQ(buf.ReadLong(), -9876543210LL);
    EXPECT_EQ(buf.ReadUnsignedLong(), 18000000000000000000ULL);
    EXPECT_FLOAT_EQ(buf.ReadFloat(), 1.5f);
    EXPECT_DOUBLE_EQ(buf.ReadDouble(), 99.99);
    EXPECT_FALSE(buf.HasRemaining());
}

//=============================================================================
// SemiPrimitive
//=============================================================================

TEST(SemiPrimitive, Vec2_RoundTrip)
{
    ByteBuffer buf(ByteOrder::Little);
    Vec2 original(3.14f, -2.71f);
    buf.Write<Vec2>(original);
    buf.Rewind();
    Vec2 result = buf.Read<Vec2>();
    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.y, original.y);
}

TEST(SemiPrimitive, AABB2_RoundTrip)
{
    ByteBuffer buf(ByteOrder::Big);
    AABB2 original(Vec2(1.0f, 2.0f), Vec2(10.0f, 20.0f));
    buf.Write<AABB2>(original);
    EXPECT_EQ(buf.WrittenBytes(), 4 * sizeof(float));
    buf.Rewind();
    AABB2 result = buf.Read<AABB2>();
    EXPECT_FLOAT_EQ(result.m_mins.x, 1.0f);
    EXPECT_FLOAT_EQ(result.m_mins.y, 2.0f);
    EXPECT_FLOAT_EQ(result.m_maxs.x, 10.0f);
    EXPECT_FLOAT_EQ(result.m_maxs.y, 20.0f);
}

TEST(SemiPrimitive, Rgba8_RoundTrip)
{
    ByteBuffer buf(ByteOrder::Little);
    Rgba8 original(128, 64, 32, 255);
    buf.Write<Rgba8>(original);
    EXPECT_EQ(buf.WrittenBytes(), 4u);
    EXPECT_EQ(buf.Data()[0], 128);
    EXPECT_EQ(buf.Data()[1], 64);
    EXPECT_EQ(buf.Data()[2], 32);
    EXPECT_EQ(buf.Data()[3], 255);
    buf.Rewind();
    Rgba8 result = buf.Read<Rgba8>();
    EXPECT_EQ(result.r, 128);
    EXPECT_EQ(result.g, 64);
    EXPECT_EQ(result.b, 32);
    EXPECT_EQ(result.a, 255);
}

//=============================================================================
// String
//=============================================================================

TEST(StringOps, LengthPrefixedString)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteString("Hello, ByteBuffer!");
    buf.Rewind();
    std::string result = buf.ReadString();
    EXPECT_EQ(result, "Hello, ByteBuffer!");
    EXPECT_FALSE(buf.HasRemaining());
}

TEST(StringOps, ShortString)
{
    ByteBuffer buf(ByteOrder::Big);
    buf.WriteShortString("Short");
    buf.Rewind();
    std::string result = buf.ReadShortString();
    EXPECT_EQ(result, "Short");
    EXPECT_FALSE(buf.HasRemaining());
}

TEST(StringOps, NullTerminatedString)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteNullTerminatedString("NullTerm");
    buf.Rewind();
    std::string result = buf.ReadNullTerminatedString();
    EXPECT_EQ(result, "NullTerm");
    EXPECT_FALSE(buf.HasRemaining());
}

TEST(StringOps, EmptyString)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteString("");
    buf.WriteShortString("");
    buf.WriteNullTerminatedString("");
    buf.Rewind();
    EXPECT_EQ(buf.ReadString(), "");
    EXPECT_EQ(buf.ReadShortString(), "");
    EXPECT_EQ(buf.ReadNullTerminatedString(), "");
    EXPECT_FALSE(buf.HasRemaining());
}

//=============================================================================
// Cursor
//=============================================================================

TEST(CursorOps, Skip)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(1);
    buf.WriteInt(2);
    buf.WriteInt(3);
    buf.Rewind();
    buf.Skip(sizeof(int32_t));
    EXPECT_EQ(buf.ReadInt(), 2);
}

TEST(CursorOps, Seek)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(10);
    buf.WriteInt(20);
    buf.WriteInt(30);
    buf.Seek(sizeof(int32_t) * 2);
    EXPECT_EQ(buf.ReadInt(), 30);
    buf.Seek(0);
    EXPECT_EQ(buf.ReadInt(), 10);
}

TEST(CursorOps, Rewind)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteFloat(1.0f);
    buf.WriteFloat(2.0f);
    buf.Rewind();
    EXPECT_FLOAT_EQ(buf.ReadFloat(), 1.0f);
    buf.Rewind();
    EXPECT_FLOAT_EQ(buf.ReadFloat(), 1.0f);
    EXPECT_EQ(buf.GetReadCursor(), sizeof(float));
}

TEST(CursorOps, Compact)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(100);
    buf.WriteInt(200);
    buf.Rewind();
    buf.ReadInt();
    EXPECT_EQ(buf.GetReadCursor(), sizeof(int32_t));
    buf.Compact();
    EXPECT_EQ(buf.GetReadCursor(), 0u);
    EXPECT_EQ(buf.WrittenBytes(), sizeof(int32_t));
    EXPECT_EQ(buf.ReadInt(), 200);
}

//=============================================================================
// Exception
//=============================================================================

TEST(ExceptionTests, ReadBeyondEnd)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteByte(0x01);
    buf.Rewind();
    buf.ReadByte();
    EXPECT_THROW(buf.ReadByte(), BufferUnderflowException);
}

TEST(ExceptionTests, SkipBeyondEnd)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(42);
    buf.Rewind();
    EXPECT_THROW(buf.Skip(100), BufferUnderflowException);
}

TEST(ExceptionTests, SeekBeyondEnd)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(42);
    EXPECT_THROW(buf.Seek(100), BufferUnderflowException);
}

//=============================================================================
// ByteOrderSwitch
//=============================================================================

TEST(ByteOrderSwitch, SwitchMidStream)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(0x12345678);
    buf.SetByteOrder(ByteOrder::Big);
    buf.WriteInt(0x12345678);

    EXPECT_EQ(buf.Data()[0], 0x78);
    EXPECT_EQ(buf.Data()[1], 0x56);
    EXPECT_EQ(buf.Data()[2], 0x34);
    EXPECT_EQ(buf.Data()[3], 0x12);

    EXPECT_EQ(buf.Data()[4], 0x12);
    EXPECT_EQ(buf.Data()[5], 0x34);
    EXPECT_EQ(buf.Data()[6], 0x56);
    EXPECT_EQ(buf.Data()[7], 0x78);

    buf.Seek(0);
    buf.SetByteOrder(ByteOrder::Little);
    EXPECT_EQ(buf.ReadInt(), 0x12345678);
    buf.SetByteOrder(ByteOrder::Big);
    EXPECT_EQ(buf.ReadInt(), 0x12345678);
}

//=============================================================================
// Factory
//=============================================================================

TEST(FactoryTests, Wrap_MoveVector)
{
    ByteArray data = {0x01, 0x02, 0x03, 0x04};
    ByteBuffer buf = ByteBuffer::Wrap(std::move(data), ByteOrder::Little);
    EXPECT_EQ(buf.WrittenBytes(), 4u);
    EXPECT_EQ(buf.ReadByte(), 0x01);
    EXPECT_EQ(buf.ReadByte(), 0x02);
}

TEST(FactoryTests, Wrap_CopyPointer)
{
    byte_t raw[] = {0xAA, 0xBB, 0xCC};
    ByteBuffer buf = ByteBuffer::Wrap(raw, 3, ByteOrder::Big);
    EXPECT_EQ(buf.WrittenBytes(), 3u);
    EXPECT_EQ(buf.ReadByte(), 0xAA);
    EXPECT_EQ(buf.ReadByte(), 0xBB);
    EXPECT_EQ(buf.ReadByte(), 0xCC);
}

TEST(FactoryTests, Clone)
{
    ByteBuffer original(ByteOrder::Little);
    original.WriteInt(42);
    original.WriteFloat(3.14f);
    original.Rewind();
    original.ReadInt();

    ByteBuffer clone = original.Clone();
    EXPECT_EQ(clone.WrittenBytes(), original.WrittenBytes());
    EXPECT_EQ(clone.GetReadCursor(), original.GetReadCursor());
    EXPECT_FLOAT_EQ(clone.ReadFloat(), 3.14f);

    clone.WriteInt(999);
    EXPECT_NE(clone.WrittenBytes(), original.WrittenBytes());
}

TEST(FactoryTests, Release)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(123);
    buf.WriteInt(456);
    size_t expectedSize = buf.WrittenBytes();

    ByteArray released = buf.Release();
    EXPECT_EQ(released.size(), expectedSize);
    EXPECT_EQ(buf.WrittenBytes(), 0u);
    EXPECT_EQ(buf.GetReadCursor(), 0u);
}

//=============================================================================
// Peek
//=============================================================================

TEST(PeekTests, PeekWithData)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(42);
    buf.Rewind();

    auto peeked = buf.Peek<int32_t>();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(peeked.value(), 42);
    EXPECT_EQ(buf.GetReadCursor(), 0u);
    EXPECT_EQ(buf.ReadInt(), 42);
}

TEST(PeekTests, PeekWithoutData)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteByte(0x01);
    buf.Rewind();

    auto peeked = buf.Peek<int32_t>();
    EXPECT_FALSE(peeked.has_value());
}

//=============================================================================
// OverwriteAt
//=============================================================================

TEST(OverwriteAtTests, OverwriteInt)
{
    ByteBuffer buf(ByteOrder::Little);
    buf.WriteInt(100);
    buf.WriteInt(200);
    buf.WriteInt(300);

    buf.OverwriteAt<int32_t>(sizeof(int32_t), 999);

    buf.Rewind();
    EXPECT_EQ(buf.ReadInt(), 100);
    EXPECT_EQ(buf.ReadInt(), 999);
    EXPECT_EQ(buf.ReadInt(), 300);
}
