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

#include <crogine/system/Colour.hpp>

#include <algorithm>

const cro::Colour cro::Colour::Red(1.f, 0.f, 0.f);
const cro::Colour cro::Colour::Green(0.f, 1.f, 0.f);
const cro::Colour cro::Colour::Blue(0.f, 0.f, 1.f);
const cro::Colour cro::Colour::Cyan(0.f, 1.f, 1.f);
const cro::Colour cro::Colour::Magenta(1.f, 0.f, 1.f);
const cro::Colour cro::Colour::Yellow(1.f, 1.f, 0.f);
const cro::Colour cro::Colour::Black(0.f, 0.f, 0.f);
const cro::Colour cro::Colour::White(1.f, 1.f, 1.f);
const cro::Colour cro::Colour::Transparent(0.f, 0.f, 0.f, 0.f);

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