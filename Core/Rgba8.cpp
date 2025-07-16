#include "Rgba8.hpp"

#include "ErrorWarningAssert.hpp"
#include "StringUtils.hpp"
#include "../Math/MathUtils.hpp"

const Rgba8 Rgba8::RED                     = Rgba8(255, 0, 0);
const Rgba8 Rgba8::GREEN                   = Rgba8(0, 255, 0);
const Rgba8 Rgba8::WHITE                   = Rgba8(255, 255, 255);
const Rgba8 Rgba8::GRAY                    = Rgba8(100, 100, 100);
const Rgba8 Rgba8::ORANGE                  = Rgba8(255, 160, 0);
const Rgba8 Rgba8::YELLOW                  = Rgba8(255, 255, 0);
const Rgba8 Rgba8::BLUE                    = Rgba8(0, 0, 255);
const Rgba8 Rgba8::MAGENTA                 = Rgba8(255, 0, 255);
const Rgba8 Rgba8::CYAN                    = Rgba8(0, 255, 255);
const Rgba8 Rgba8::DEBUG_BLUE              = Rgba8(50, 80, 150);
const Rgba8 Rgba8::DEBUG_GREEN             = Rgba8(100, 255, 200);
const Rgba8 Rgba8::DEBUG_WHITE_TRANSLUCENT = Rgba8(255, 255, 255, 30);

Rgba8::Rgba8()
{
    r = 255;
    g = 255;
    b = 255;
    a = 255;
}

Rgba8::Rgba8(const unsigned char r, const unsigned char g, const unsigned char b,
             const unsigned char a) : r(r), g(g), b(b), a(a)
{
}

Rgba8::Rgba8(Vec4 vec4Color)
{
    r = static_cast<char>(GetClamped(static_cast<float>(vec4Color.x) * 255.0f, 0, 255));
    g = static_cast<char>(GetClamped(static_cast<float>(vec4Color.y) * 255.0f, 0, 255));
    b = static_cast<char>(GetClamped(static_cast<float>(vec4Color.z) * 255.0f, 0, 255));
    a = static_cast<char>(GetClamped(static_cast<float>(vec4Color.w) * 255.0f, 0, 255));
}

Rgba8::Rgba8(const Rgba8& copyFrom)
{
    this->r = copyFrom.r;
    this->g = copyFrom.g;
    this->b = copyFrom.b;
    this->a = copyFrom.a;
}

Rgba8 Rgba8::operator*(float multiplier)
{
    Rgba8 rgba8 = *this;
    rgba8.r     = static_cast<char>(GetClamped(static_cast<float>(this->r) * multiplier, 0, 255));
    rgba8.g     = static_cast<char>(GetClamped(static_cast<float>(this->g) * multiplier, 0, 255));
    rgba8.b     = static_cast<char>(GetClamped(static_cast<float>(this->b) * multiplier, 0, 255));
    return rgba8;
}

void Rgba8::SetFromText(const char* text)
{
    if (text == nullptr)
    {
        return;
    }

    Strings parts = SplitStringOnDelimiter(text, ',');

    // Ensure that we have between 3 and 4 parts (r, g, b, [a])
    if (parts.size() < 3 || parts.size() > 4)
    {
        return;
    }

    for (std::string& part : parts)
    {
        for (size_t i = 0; i < part.length(); ++i)
        {
            if (isspace(part[i]))
            {
                part.erase(i, 1);
                --i;
            }
        }
    }

    // Convert the strings to unsigned chars and set r, g, b, a
    r = static_cast<unsigned char>(atoi(parts[0].c_str()));
    g = static_cast<unsigned char>(atoi(parts[1].c_str()));
    b = static_cast<unsigned char>(atoi(parts[2].c_str()));
    a = (parts.size() == 4) ? static_cast<unsigned char>(atoi(parts[3].c_str())) : 255;
}

void Rgba8::GetAsFloats(float* colorAsFloats) const
{
    colorAsFloats[0] = static_cast<float>(r) / 255.0f;
    colorAsFloats[1] = static_cast<float>(g) / 255.0f;
    colorAsFloats[2] = static_cast<float>(b) / 255.0f;
    colorAsFloats[3] = static_cast<float>(a) / 255.0f;
}

Rgba8 Interpolate(Rgba8 from, Rgba8 to, float fractionOfEnd)
{
    float r = Interpolate(NormalizeByte(from.r), NormalizeByte(to.r), fractionOfEnd);
    float g = Interpolate(NormalizeByte(from.g), NormalizeByte(to.g), fractionOfEnd);
    float b = Interpolate(NormalizeByte(from.b), NormalizeByte(to.b), fractionOfEnd);
    float a = Interpolate(NormalizeByte(from.a), NormalizeByte(to.a), fractionOfEnd);
    return Rgba8(DenormalizeByte(r), DenormalizeByte(g), DenormalizeByte(b), DenormalizeByte(a));
}

bool operator==(const Rgba8& lhs, const Rgba8& rhs)
{
    return lhs.r == rhs.r
        && lhs.g == rhs.g
        && lhs.b == rhs.b
        && lhs.a == rhs.a;
}

bool operator!=(const Rgba8& lhs, const Rgba8& rhs)
{
    return !(lhs == rhs);
}
