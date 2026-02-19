#pragma once
//-----------------------------------------------------------------------------------------------
// ByteBuffer.hpp
//
// Dual-cursor binary serialization buffer inspired by Netty ByteBuf.
// Write cursor is implicit (m_data.size()), read cursor is explicit (m_readCursor).
// All multi-byte operations respect the configured ByteOrder.
//
// Move-only: copy disabled, use Clone() for explicit deep copy.
//-----------------------------------------------------------------------------------------------

#include "Engine/Core/Buffer/Endian.hpp"
#include "Engine/Core/Buffer/BufferExceptions.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace enigma::core
{
    using byte_t    = uint8_t;
    using ByteArray = std::vector<byte_t>;

    // Forward declaration for BufferSerializable traits
    template<typename T, typename = void>
    struct BufferSerializable;

    class ByteBuffer
    {
    public:
        //=== Construction & Factories ===
        explicit ByteBuffer(ByteOrder order = ByteOrder::Big, size_t initialCapacity = 256);
        static ByteBuffer Wrap(ByteArray data, ByteOrder order = ByteOrder::Big);
        static ByteBuffer Wrap(const byte_t* data, size_t size, ByteOrder order = ByteOrder::Big);
        ByteBuffer Clone() const;

        // Move-only
        ByteBuffer(ByteBuffer&&) noexcept = default;
        ByteBuffer& operator=(ByteBuffer&&) noexcept = default;
        ByteBuffer(const ByteBuffer&) = delete;
        ByteBuffer& operator=(const ByteBuffer&) = delete;

        //=== Primitive Type Writes (append to end) ===
        void WriteBool(bool value);
        void WriteByte(byte_t value);
        void WriteSignedByte(int8_t value);
        void WriteShort(int16_t value);
        void WriteUnsignedShort(uint16_t value);
        void WriteInt(int32_t value);
        void WriteUnsignedInt(uint32_t value);
        void WriteLong(int64_t value);
        void WriteUnsignedLong(uint64_t value);
        void WriteFloat(float value);
        void WriteDouble(double value);

        //=== Primitive Type Reads (from m_readCursor, advances cursor) ===
        bool     ReadBool();
        byte_t   ReadByte();
        int8_t   ReadSignedByte();
        int16_t  ReadShort();
        uint16_t ReadUnsignedShort();
        int32_t  ReadInt();
        uint32_t ReadUnsignedInt();
        int64_t  ReadLong();
        uint64_t ReadUnsignedLong();
        float    ReadFloat();
        double   ReadDouble();

        //=== String Operations ===
        void        WriteString(std::string_view str);              // uint32 length + UTF-8
        void        WriteShortString(std::string_view str);         // uint16 length + UTF-8
        void        WriteNullTerminatedString(std::string_view str);// content + 0x00
        std::string ReadString();
        std::string ReadShortString();
        std::string ReadNullTerminatedString();

        //=== Raw Bytes ===
        void      WriteRawBytes(const void* data, size_t size);
        void      WriteRawBytes(const ByteArray& data);
        ByteArray ReadRawBytes(size_t count);
        void      ReadRawBytesInto(void* dest, size_t count);

        //=== Raw Trivially-Copyable ===
        template<typename T> void WriteRaw(const T& value);
        template<typename T> T    ReadRaw();

        //=== Generic Traits Interface (semi-primitive types) ===
        template<typename T> void Write(const T& value);
        template<typename T> T    Read();

        //=== Peek (preview without advancing cursor) ===
        template<typename T>
        [[nodiscard]] std::optional<T> Peek() const;

        //=== Random-Access Overwrite ===
        template<typename T>
        void OverwriteAt(size_t offset, T value);

        //=== Cursor & State ===
        void   Skip(size_t bytes);
        void   Rewind();
        void   Seek(size_t position);
        void   Clear();
        void   Compact();
        size_t ReadableBytes()  const;
        size_t WrittenBytes()   const;
        size_t GetReadCursor()  const;
        bool   HasRemaining()   const;
        bool   HasRemaining(size_t n) const;
        ByteOrder GetByteOrder() const;
        void   SetByteOrder(ByteOrder order);

        //=== Data Access ===
        const byte_t*    Data()      const;
        const ByteArray& GetBuffer() const;
        ByteArray        Release();

    private:
        ByteArray m_data;
        size_t    m_readCursor = 0;
        ByteOrder m_byteOrder;

        void ensureReadable(size_t bytes) const;

        template<typename T> void writeIntegral(T value);
        template<typename T> void writeFloating(T value);
        template<typename T> T    readIntegral();
        template<typename T> T    readFloating();
    };

    //===========================================================================================
    // Template implementations (must be in header)
    //===========================================================================================

    template<typename T>
    void ByteBuffer::WriteRaw(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "WriteRaw requires trivially copyable type");
        const auto* bytes = reinterpret_cast<const byte_t*>(&value);
        m_data.insert(m_data.end(), bytes, bytes + sizeof(T));
    }

    template<typename T>
    T ByteBuffer::ReadRaw()
    {
        static_assert(std::is_trivially_copyable_v<T>, "ReadRaw requires trivially copyable type");
        ensureReadable(sizeof(T));
        T value;
        std::memcpy(&value, m_data.data() + m_readCursor, sizeof(T));
        m_readCursor += sizeof(T);
        return value;
    }

    template<typename T>
    void ByteBuffer::Write(const T& value)
    {
        BufferSerializable<T>::Serialize(*this, value);
    }

    template<typename T>
    T ByteBuffer::Read()
    {
        return BufferSerializable<T>::Deserialize(*this);
    }

    template<typename T>
    std::optional<T> ByteBuffer::Peek() const
    {
        if (m_readCursor + sizeof(T) > m_data.size())
            return std::nullopt;

        // Create a temporary copy of relevant state to read without side effects
        T value;
        std::memcpy(&value, m_data.data() + m_readCursor, sizeof(T));
        value = ToByteOrder(value, m_byteOrder);
        return value;
    }

    template<typename T>
    void ByteBuffer::OverwriteAt(size_t offset, T value)
    {
        static_assert(std::is_arithmetic_v<T>, "OverwriteAt only supports arithmetic types");
        if (offset + sizeof(T) > m_data.size())
            throw BufferUnderflowException(offset, m_data.size(), sizeof(T));

        value = ToByteOrder(value, m_byteOrder);
        std::memcpy(m_data.data() + offset, &value, sizeof(T));
    }

    template<typename T>
    void ByteBuffer::writeIntegral(T value)
    {
        value = ToByteOrder(value, m_byteOrder);
        const auto* bytes = reinterpret_cast<const byte_t*>(&value);
        m_data.insert(m_data.end(), bytes, bytes + sizeof(T));
    }

    template<typename T>
    void ByteBuffer::writeFloating(T value)
    {
        value = ToByteOrder(value, m_byteOrder);
        const auto* bytes = reinterpret_cast<const byte_t*>(&value);
        m_data.insert(m_data.end(), bytes, bytes + sizeof(T));
    }

    template<typename T>
    T ByteBuffer::readIntegral()
    {
        ensureReadable(sizeof(T));
        T value;
        std::memcpy(&value, m_data.data() + m_readCursor, sizeof(T));
        m_readCursor += sizeof(T);
        return ToByteOrder(value, m_byteOrder);
    }

    template<typename T>
    T ByteBuffer::readFloating()
    {
        ensureReadable(sizeof(T));
        T value;
        std::memcpy(&value, m_data.data() + m_readCursor, sizeof(T));
        m_readCursor += sizeof(T);
        return ToByteOrder(value, m_byteOrder);
    }
} // namespace enigma::core
