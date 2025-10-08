/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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


const cro::Colour cro::Colour::Red         = cro::Colour(1.f, 0.f, 0.f );
const cro::Colour cro::Colour::Green       = cro::Colour(0.f, 1.f, 0.f);
const cro::Colour cro::Colour::Blue        = cro::Colour(0.f, 0.f, 1.f);
const cro::Colour cro::Colour::Cyan        = cro::Colour(0.f, 1.f, 1.f);
const cro::Colour cro::Colour::Magenta     = cro::Colour(1.f, 0.f, 1.f);
const cro::Colour cro::Colour::Yellow      = cro::Colour(1.f, 1.f, 0.f);
const cro::Colour cro::Colour::Black       = cro::Colour(0.f, 0.f, 0.f);
const cro::Colour cro::Colour::White       = cro::Colour(1.f, 1.f, 1.f);
const cro::Colour cro::Colour::Transparent = cro::Colour(0.f, 0.f, 0.f, 0.f);


const cro::Colour cro::Colour::AliceBlue      = cro::Colour(0x00F0F8FF);
const cro::Colour cro::Colour::CornflowerBlue = cro::Colour(0x6495EDFF);
const cro::Colour cro::Colour::DarkGrey       = cro::Colour(0xA9A9A9FF);
const cro::Colour cro::Colour::Gainsboro      = cro::Colour(0xDCDCDCFF);
const cro::Colour cro::Colour::LightGrey      = cro::Colour(0xD3D3D3FF);
const cro::Colour cro::Colour::Plum           = cro::Colour(0xDDA0DDFF);
const cro::Colour cro::Colour::Teal           = cro::Colour(0x008080FF);

cro::Colour::Colour(glm::vec3 vector)
    : r(0.f), g(0.f), b(0.f), a(1.f)
{
    setRed(vector.r);
    setGreen(vector.g);
    setBlue(vector.b);
}

cro::Colour::Colour(glm::vec4 vector)
    : r(0.f), g(0.f), b(0.f), a(0.f)
{
    setRed(vector.r);
    setGreen(vector.g);
    setBlue(vector.b);
    setAlpha(vector.a);
}

cro::Colour& cro::Colour::operator = (glm::vec3 rhs)
{
    setRed(rhs.r);
    setGreen(rhs.g);
    setBlue(rhs.b);
    a = 1.f;
    return *this;
}

cro::Colour& cro::Colour::operator = (glm::vec4 rhs)
{
    setRed(rhs.r);
    setGreen(rhs.g);
    setBlue(rhs.b);
    setAlpha(rhs.a);
    return *this;
}

//public
void cro::Colour::setRed(std::uint8_t red)
{
    r = static_cast<float>(red) / 255.f;
}

void cro::Colour::setRed(float red)
{
    CRO_ASSERT(red >= 0 && red <= 1, "Value must be normalised");
    r = red;
}

void cro::Colour::setGreen(std::uint8_t green)
{
    g = static_cast<float>(green) / 255.f;
}

void cro::Colour::setGreen(float green)
{
    CRO_ASSERT(green >= 0 && green <= 1, "Value must be normalised");
    g = green;
}

void cro::Colour::setBlue(std::uint8_t blue)
{
    b = static_cast<float>(blue) / 255.f;
}

void cro::Colour::setBlue(float blue)
{
    CRO_ASSERT(blue >= 0 && blue <= 1, "Value must be normalised");
    b = blue;
}

void cro::Colour::setAlpha(std::uint8_t alpha)
{
    a = static_cast<float>(alpha) / 255.f;
}

void cro::Colour::setAlpha(float alpha)
{
    CRO_ASSERT(alpha >= 0 && alpha <= 1, "Value must be normalised");
    a = alpha;
}

//constexpr std::uint8_t cro::Colour::getRedByte() const
//{
//    return static_cast<std::uint8_t>(255.f * r);
//}
//
//constexpr std::uint8_t cro::Colour::getGreenByte() const
//{
//    return static_cast<std::uint8_t>(255.f * g);
//}
//
//constexpr std::uint8_t cro::Colour::getBlueByte() const
//{
//    return static_cast<std::uint8_t>(255.f * b);
//}
//
//constexpr std::uint8_t cro::Colour::getAlphaByte() const
//{
//    return static_cast<std::uint8_t>(255.f * a);
//}

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

std::uint32_t cro::Colour::getPacked() const
{
    return (getRedByte() << 24 | getGreenByte() << 16 | getBlueByte() << 8 | getAlphaByte());
}

glm::vec4 cro::Colour::getVec4() const
{
    return { r,g,b,a };
}

//operators
cro::Colour::operator std::uint32_t() const
{
    return (getRedByte() << 24 | getGreenByte() << 16 | getBlueByte() << 8 | getAlphaByte());
}

cro::Colour::operator glm::vec4() const
{
    return { r,g,b,a };
}

cro::Colour::operator float*()
{
    return &r;
}

cro::Colour::operator const float* () const
{
    return &r;
}

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