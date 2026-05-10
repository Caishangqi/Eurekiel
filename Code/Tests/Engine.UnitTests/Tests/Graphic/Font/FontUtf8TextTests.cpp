#include <gtest/gtest.h>

#include "Engine/Graphic/Font/Layout/Utf8Text.hpp"
#include "Engine/Graphic/Font/Exception/FontException.hpp"

#include <string>

using namespace enigma::graphic;

TEST(FontUtf8TextTests, DecodesAsciiWithByteOffsets)
{
    std::vector<DecodedCodepoint> decoded = DecodeUtf8Text("Wi\n");

    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_EQ(decoded[0].codepoint, static_cast<uint32_t>('W'));
    EXPECT_EQ(decoded[0].byteOffset, 0u);
    EXPECT_EQ(decoded[1].codepoint, static_cast<uint32_t>('i'));
    EXPECT_EQ(decoded[1].byteOffset, 1u);
    EXPECT_EQ(decoded[2].codepoint, static_cast<uint32_t>('\n'));
    EXPECT_EQ(decoded[2].byteOffset, 2u);
}

TEST(FontUtf8TextTests, DecodesMultibyteSequences)
{
    const std::string text = u8"A\u00A2\u20AC\U0001F600";

    std::vector<DecodedCodepoint> decoded = DecodeUtf8Text(text);

    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[0].codepoint, 0x41u);
    EXPECT_EQ(decoded[0].byteOffset, 0u);
    EXPECT_EQ(decoded[1].codepoint, 0x00A2u);
    EXPECT_EQ(decoded[1].byteOffset, 1u);
    EXPECT_EQ(decoded[2].codepoint, 0x20ACu);
    EXPECT_EQ(decoded[2].byteOffset, 3u);
    EXPECT_EQ(decoded[3].codepoint, 0x1F600u);
    EXPECT_EQ(decoded[3].byteOffset, 6u);
}

TEST(FontUtf8TextTests, UniqueCodepointsPreserveFirstSeenOrder)
{
    std::vector<uint32_t> unique = DecodeUniqueCodepoints(u8"WiWi\u00A2W");

    ASSERT_EQ(unique.size(), 3u);
    EXPECT_EQ(unique[0], static_cast<uint32_t>('W'));
    EXPECT_EQ(unique[1], static_cast<uint32_t>('i'));
    EXPECT_EQ(unique[2], 0x00A2u);
}

TEST(FontUtf8TextTests, PreservesNewlineAsCodepoint)
{
    std::vector<uint32_t> unique = DecodeUniqueCodepoints("A\nB");

    ASSERT_EQ(unique.size(), 3u);
    EXPECT_EQ(unique[0], static_cast<uint32_t>('A'));
    EXPECT_EQ(unique[1], static_cast<uint32_t>('\n'));
    EXPECT_EQ(unique[2], static_cast<uint32_t>('B'));
}

TEST(FontUtf8TextTests, RejectsInvalidUtf8)
{
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\x80", 1)), FontConfigurationException);
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\xC0\x80", 2)), FontConfigurationException);
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\xE0\x80\x80", 3)), FontConfigurationException);
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\xED\xA0\x80", 3)), FontConfigurationException);
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\xF4\x90\x80\x80", 4)), FontConfigurationException);
    EXPECT_THROW((void)DecodeUtf8Text(std::string("\xE2\x82", 2)), FontConfigurationException);
}
