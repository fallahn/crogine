/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include <crogine/graphics/Colour.hpp>

#include <algorithm>


cro::Colour cro::Colour::Red() { return cro::Colour(1.f, 0.f, 0.f ); }
cro::Colour cro::Colour::Green() { return cro::Colour(0.f, 1.f, 0.f); }
cro::Colour cro::Colour::Blue() { return cro::Colour(0.f, 0.f, 1.f); }
cro::Colour cro::Colour::Cyan() { return cro::Colour(0.f, 1.f, 1.f); }
cro::Colour cro::Colour::Magenta() { return cro::Colour(1.f, 0.f, 1.f); }
cro::Colour cro::Colour::Yellow() { return cro::Colour(1.f, 1.f, 0.f); }
cro::Colour cro::Colour::Black() { return cro::Colour(0.f, 0.f, 0.f); }
cro::Colour cro::Colour::White() { return cro::Colour(1.f, 1.f, 1.f); }
cro::Colour cro::Colour::Transparent() { return cro::Colour(0.f, 0.f, 0.f, 0.f); }

cro::Colour::Colour()
    : r(0.f), g(0.f), b(0.f), a(1.f) {}

cro::Colour::Colour(cro::uint8 red, cro::uint8 green, cro::uint8 blue, cro::uint8 alpha)
    : r(static_cast<float>(red) / 255.f),
    g(static_cast<float>(green) / 255.f),
    b(static_cast<float>(blue) / 255.f),
    a(static_cast<float>(alpha) / 255.f) {}

cro::Colour::Colour(cro::uint32 mask)
    : r(static_cast<float>((mask >> 24) & 0xFF) / 255.f),
    g(static_cast<float>((mask >> 16) & 0xFF) / 255.f),
    b(static_cast<float>((mask >> 8) & 0xFF) / 255.f),
    a(static_cast<float>(mask & 0xFF) / 255.f) {}

cro::Colour::Colour(float red, float green, float blue, float alpha)
    : r(red), g(green), b(blue), a(alpha)
{
    CRO_ASSERT((r >= 0 && r <= 1) && (g >= 0 && g <= 1) && (b >= 0 && b <= 1) && (a >= 0 && a <= 1), "Values must be normalised");
}

//public
void cro::Colour::setRed(cro::uint8 red)
{
    r = static_cast<float>(red) / 255.f;
}

void cro::Colour::setRed(float red)
{
    CRO_ASSERT(red >= 0 && red <= 1, "Value must be normalised");
    r = red;
}

void cro::Colour::setGreen(cro::uint8 green)
{
    g = static_cast<float>(green) / 255.f;
}

void cro::Colour::setGreen(float green)
{
    CRO_ASSERT(green >= 0 && green <= 1, "Value must be normalised");
    g = green;
}

void cro::Colour::setBlue(cro::uint8 blue)
{
    b = static_cast<float>(blue) / 255.f;
}

void cro::Colour::setBlue(float blue)
{
    CRO_ASSERT(blue >= 0 && blue <= 1, "Value must be normalised");
    b = blue;
}

void cro::Colour::setAlpha(cro::uint8 alpha)
{
    a = static_cast<float>(alpha) / 255.f;
}

void cro::Colour::setAlpha(float alpha)
{
    CRO_ASSERT(alpha >= 0 && alpha <= 1, "Value must be normalised");
    a = alpha;
}

cro::uint8 cro::Colour::getRedByte() const
{
    return static_cast<uint8>(255.f * r);
}

cro::uint8 cro::Colour::getGreenByte() const
{
    return static_cast<uint8>(255.f * g);
}

cro::uint8 cro::Colour::getBlueByte() const
{
    return static_cast<uint8>(255.f * b);
}

cro::uint8 cro::Colour::getAlphaByte() const
{
    return static_cast<uint8>(255.f * a);
}

float cro::Colour::getRed() const
{
    return r;
}

float cro::Colour::getGreen() const
{
    return g;
}

float cro::Colour::getBlue() const
{
    return b;
}

float cro::Colour::getAlpha() const
{
    return a;
}


cro::int32 cro::Colour::getPacked() const
{
    return (getRedByte() << 24 | getGreenByte() << 16 | getBlueByte() << 8 | getAlphaByte());
}

//operators
bool cro::operator == (const Colour& l, const Colour& r)
{
    return ((l.r == r.r) &&
        (l.g == r.g) &&
        (l.b == r.b) &&
        (l.a == r.a));
}

bool cro::operator != (const Colour& l, const Colour& r)
{
    return !(l == r);
}

cro::Colour cro::operator + (const Colour& l, const Colour& r)
{
    return Colour(std::min(l.r + r.r, 1.f),
        std::min(l.g + r.g, 1.f),
        std::min(l.b + r.b, 1.f),
        std::min(l.a + r.a, 1.f));
}

cro::Colour cro::operator - (const Colour& l, const Colour& r)
{
    return Colour(std::max(l.r - r.r, 0.f),
        std::max(l.g - r.g, 0.f),
        std::max(l.b - r.b, 0.f),
        std::max(l.a - r.a, 0.f));
}

cro::Colour cro::operator * (const Colour& l, const Colour& r)
{
    return Colour(l.r * r.r, l.g * r.g, l.b * r.b, l.a * r.a);
}

cro::Colour& cro::operator += (Colour& l, const Colour& r)
{
    return l = l + r;
}

cro::Colour& cro::operator -= (Colour& l, const Colour& r)
{
    return l = l - r;
}

cro::Colour& cro::operator *= (Colour& l, const Colour& r)
{
    return l = l * r;
}