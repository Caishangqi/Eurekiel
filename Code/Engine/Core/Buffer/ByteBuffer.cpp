#include "Engine/Core/Buffer/ByteBuffer.hpp"

#include <algorithm>

namespace enigma::core
{
    //===========================================================================================
    // Construction & Factories
    //===========================================================================================

    ByteBuffer::ByteBuffer(ByteOrder order, size_t initialCapacity)
        : m_byteOrder(order)
    {
        m_data.reserve(initialCapacity);
    }

    ByteBuffer ByteBuffer::Wrap(ByteArray data, ByteOrder order)
    {
        ByteBuffer buf(order, 0);
        buf.m_data = std::move(data);
        return buf;
    }

    ByteBuffer ByteBuffer::Wrap(const byte_t* data, size_t size, ByteOrder order)
    {
        ByteBuffer buf(order, size);
        buf.m_data.assign(data, data + size);
        return buf;
    }

    ByteBuffer ByteBuffer::Clone() const
    {
        ByteBuffer buf(m_byteOrder, m_data.size());
        buf.m_data = m_data;
        buf.m_readCursor = m_readCursor;
        return buf;
    }

    //===========================================================================================
    // Primitive Writes
    //===========================================================================================

    void ByteBuffer::WriteBool(bool value)
    {
        m_data.push_back(value ? byte_t(1) : byte_t(0));
    }

    void ByteBuffer::WriteByte(byte_t value)
    {
        m_data.push_back(value);
    }

    void ByteBuffer::WriteSignedByte(int8_t value)
    {
        m_data.push_back(static_cast<byte_t>(value));
    }

    void ByteBuffer::WriteShort(int16_t value)       { writeIntegral(value); }
    void ByteBuffer::WriteUnsignedShort(uint16_t value) { writeIntegral(value); }
    void ByteBuffer::WriteInt(int32_t value)          { writeIntegral(value); }
    void ByteBuffer::WriteUnsignedInt(uint32_t value) { writeIntegral(value); }
    void ByteBuffer::WriteLong(int64_t value)         { writeIntegral(value); }
    void ByteBuffer::WriteUnsignedLong(uint64_t value){ writeIntegral(value); }
    void ByteBuffer::WriteFloat(float value)          { writeFloating(value); }
    void ByteBuffer::WriteDouble(double value)        { writeFloating(value); }

    //===========================================================================================
    // Primitive Reads
    //===========================================================================================

    bool ByteBuffer::ReadBool()
    {
        ensureReadable(1);
        return m_data[m_readCursor++] != 0;
    }

    byte_t ByteBuffer::ReadByte()
    {
        ensureReadable(1);
        return m_data[m_readCursor++];
    }

    int8_t ByteBuffer::ReadSignedByte()
    {
        ensureReadable(1);
        return static_cast<int8_t>(m_data[m_readCursor++]);
    }

    int16_t  ByteBuffer::ReadShort()         { return readIntegral<int16_t>(); }
    uint16_t ByteBuffer::ReadUnsignedShort() { return readIntegral<uint16_t>(); }
    int32_t  ByteBuffer::ReadInt()           { return readIntegral<int32_t>(); }
    uint32_t ByteBuffer::ReadUnsignedInt()   { return readIntegral<uint32_t>(); }
    int64_t  ByteBuffer::ReadLong()          { return readIntegral<int64_t>(); }
    uint64_t ByteBuffer::ReadUnsignedLong()  { return readIntegral<uint64_t>(); }
    float    ByteBuffer::ReadFloat()         { return readFloating<float>(); }
    double   ByteBuffer::ReadDouble()        { return readFloating<double>(); }

    //===========================================================================================
    // String Operations
    //===========================================================================================

    void ByteBuffer::WriteString(std::string_view str)
    {
        WriteUnsignedInt(static_cast<uint32_t>(str.size()));
        WriteRawBytes(str.data(), str.size());
    }

    void ByteBuffer::WriteShortString(std::string_view str)
    {
        WriteUnsignedShort(static_cast<uint16_t>(str.size()));
        WriteRawBytes(str.data(), str.size());
    }

    void ByteBuffer::WriteNullTerminatedString(std::string_view str)
    {
        WriteRawBytes(str.data(), str.size());
        WriteByte(0x00);
    }

    std::string ByteBuffer::ReadString()
    {
        uint32_t length = ReadUnsignedInt();
        ensureReadable(length);
        std::string result(reinterpret_cast<const char*>(m_data.data() + m_readCursor), length);
        m_readCursor += length;
        return result;
    }

    std::string ByteBuffer::ReadShortString()
    {
        uint16_t length = ReadUnsignedShort();
        ensureReadable(length);
        std::string result(reinterpret_cast<const char*>(m_data.data() + m_readCursor), length);
        m_readCursor += length;
        return result;
    }

    std::string ByteBuffer::ReadNullTerminatedString()
    {
        std::string result;
        while (true)
        {
            ensureReadable(1);
            byte_t ch = m_data[m_readCursor++];
            if (ch == 0x00)
                break;
            result.push_back(static_cast<char>(ch));
        }
        return result;
    }

    //===========================================================================================
    // Raw Bytes
    //===========================================================================================

    void ByteBuffer::WriteRawBytes(const void* data, size_t size)
    {
        const auto* bytes = static_cast<const byte_t*>(data);
        m_data.insert(m_data.end(), bytes, bytes + size);
    }

    void ByteBuffer::WriteRawBytes(const ByteArray& data)
    {
        m_data.insert(m_data.end(), data.begin(), data.end());
    }

    ByteArray ByteBuffer::ReadRawBytes(size_t count)
    {
        ensureReadable(count);
        ByteArray result(m_data.begin() + m_readCursor, m_data.begin() + m_readCursor + count);
        m_readCursor += count;
        return result;
    }

    void ByteBuffer::ReadRawBytesInto(void* dest, size_t count)
    {
        ensureReadable(count);
        std::memcpy(dest, m_data.data() + m_readCursor, count);
        m_readCursor += count;
    }

    //===========================================================================================
    // Cursor & State
    //===========================================================================================

    void ByteBuffer::Skip(size_t bytes)
    {
        ensureReadable(bytes);
        m_readCursor += bytes;
    }

    void ByteBuffer::Rewind()
    {
        m_readCursor = 0;
    }

    void ByteBuffer::Seek(size_t position)
    {
        if (position > m_data.size())
            throw BufferUnderflowException(position, m_data.size(), 0);
        m_readCursor = position;
    }

    void ByteBuffer::Clear()
    {
        m_data.clear();
        m_readCursor = 0;
    }

    void ByteBuffer::Compact()
    {
        if (m_readCursor == 0)
            return;
        m_data.erase(m_data.begin(), m_data.begin() + m_readCursor);
        m_readCursor = 0;
    }

    size_t ByteBuffer::ReadableBytes() const
    {
        return m_data.size() - m_readCursor;
    }

    size_t ByteBuffer::WrittenBytes() const
    {
        return m_data.size();
    }

    size_t ByteBuffer::GetReadCursor() const
    {
        return m_readCursor;
    }

    bool ByteBuffer::HasRemaining() const
    {
        return m_readCursor < m_data.size();
    }

    bool ByteBuffer::HasRemaining(size_t n) const
    {
        return (m_readCursor + n) <= m_data.size();
    }

    ByteOrder ByteBuffer::GetByteOrder() const
    {
        return m_byteOrder;
    }

    void ByteBuffer::SetByteOrder(ByteOrder order)
    {
        m_byteOrder = order;
    }

    //===========================================================================================
    // Data Access
    //===========================================================================================

    const byte_t* ByteBuffer::Data() const
    {
        return m_data.data();
    }

    const ByteArray& ByteBuffer::GetBuffer() const
    {
        return m_data;
    }

    ByteArray ByteBuffer::Release()
    {
        ByteArray result = std::move(m_data);
        m_data.clear();
        m_readCursor = 0;
        return result;
    }

    //===========================================================================================
    // Internal
    //===========================================================================================

    void ByteBuffer::ensureReadable(size_t bytes) const
    {
        if (m_readCursor + bytes > m_data.size())
            throw BufferUnderflowException(m_readCursor, m_data.size(), bytes);
    }
} // namespace enigma::core
