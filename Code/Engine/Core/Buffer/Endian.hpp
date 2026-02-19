#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

// C++20 std::endian support detection
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include <bit>
#define ENIGMA_HAS_STD_ENDIAN 1
#else
#define ENIGMA_HAS_STD_ENDIAN 0
#endif

namespace enigma::core
{
    //--------------------------------------------------------------
    // ByteOrder enum
    //--------------------------------------------------------------
    enum class ByteOrder : uint8_t
    {
        Native = 0,
        Little = 1,
        Big    = 2
    };

    //--------------------------------------------------------------
    // Compile-time native byte order detection
    //--------------------------------------------------------------
    constexpr ByteOrder NativeByteOrder()
    {
#if ENIGMA_HAS_STD_ENDIAN
        if constexpr (std::endian::native == std::endian::little)
            return ByteOrder::Little;
        else
            return ByteOrder::Big;
#elif defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM) \
   || defined(__x86_64__) || defined(__i386__) \
   || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        return ByteOrder::Little;
#elif (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        return ByteOrder::Big;
#else
        #error "Unable to determine native byte order at compile time"
#endif
    }

    //--------------------------------------------------------------
    // Resolve ByteOrder::Native to actual endianness
    //--------------------------------------------------------------
    constexpr ByteOrder ResolveByteOrder(ByteOrder order)
    {
        return (order == ByteOrder::Native) ? NativeByteOrder() : order;
    }

    //--------------------------------------------------------------
    // detail:: ByteSwap overloads
    //--------------------------------------------------------------
    namespace detail
    {
        // --- Integer swaps (constexpr) ---

        constexpr uint16_t ByteSwap(uint16_t v)
        {
            return static_cast<uint16_t>((v << 8) | (v >> 8));
        }

        constexpr uint32_t ByteSwap(uint32_t v)
        {
            return ((v & 0x000000FFu) << 24)
                 | ((v & 0x0000FF00u) << 8)
                 | ((v & 0x00FF0000u) >> 8)
                 | ((v & 0xFF000000u) >> 24);
        }

        constexpr uint64_t ByteSwap(uint64_t v)
        {
            return ((v & 0x00000000000000FFull) << 56)
                 | ((v & 0x000000000000FF00ull) << 40)
                 | ((v & 0x0000000000FF0000ull) << 24)
                 | ((v & 0x00000000FF000000ull) << 8)
                 | ((v & 0x000000FF00000000ull) >> 8)
                 | ((v & 0x0000FF0000000000ull) >> 24)
                 | ((v & 0x00FF000000000000ull) >> 40)
                 | ((v & 0xFF00000000000000ull) >> 56);
        }

        constexpr int16_t ByteSwap(int16_t v)
        {
            return static_cast<int16_t>(ByteSwap(static_cast<uint16_t>(v)));
        }

        constexpr int32_t ByteSwap(int32_t v)
        {
            return static_cast<int32_t>(ByteSwap(static_cast<uint32_t>(v)));
        }

        constexpr int64_t ByteSwap(int64_t v)
        {
            return static_cast<int64_t>(ByteSwap(static_cast<uint64_t>(v)));
        }

        // --- Floating-point swaps (memcpy type-punning, no UB) ---

        inline float ByteSwap(float v)
        {
            static_assert(sizeof(float) == sizeof(uint32_t), "float must be 4 bytes");
            uint32_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            bits = ByteSwap(bits);
            float result;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }

        inline double ByteSwap(double v)
        {
            static_assert(sizeof(double) == sizeof(uint64_t), "double must be 8 bytes");
            uint64_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            bits = ByteSwap(bits);
            double result;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }
    } // namespace detail

    //--------------------------------------------------------------
    // ToByteOrder<T> - entry point for byte order conversion
    //
    // Converts 'value' from Native byte order to 'target' byte order
    // (or vice versa). If target matches Native, returns value unchanged.
    // Single-byte types (uint8_t, int8_t) are always no-ops.
    //--------------------------------------------------------------

    // Single-byte overloads: always no-op
    constexpr uint8_t ToByteOrder(uint8_t value, ByteOrder /*target*/) { return value; }
    constexpr int8_t  ToByteOrder(int8_t  value, ByteOrder /*target*/) { return value; }
    constexpr bool    ToByteOrder(bool    value, ByteOrder /*target*/) { return value; }

    // Multi-byte types
    template<typename T>
    inline T ToByteOrder(T value, ByteOrder target)
    {
        static_assert(
            std::is_arithmetic_v<T>,
            "ToByteOrder only supports arithmetic types"
        );

        ByteOrder resolved = ResolveByteOrder(target);
        if (resolved == NativeByteOrder())
            return value;

        return detail::ByteSwap(value);
    }
}
