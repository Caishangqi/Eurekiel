#include "Rgba8.hpp"

Rgba8::Rgba8()
{
}

Rgba8::Rgba8(unsigned char r, unsigned char g, unsigned char b, unsigned char a): r(r), g(g), b(b), a(a)
{
}

Rgba8::Rgba8(const Rgba8& copyFrom)
{
    this->r = copyFrom.r;
    this->g = copyFrom.g;
    this->b = copyFrom.b;
    this->a = copyFrom.a;
}
