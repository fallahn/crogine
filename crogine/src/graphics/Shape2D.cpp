/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/graphics/Shape2D.hpp>

#include <crogine/util/Constants.hpp>

#include <cmath>

using namespace cro;

std::vector<Vertex2D> Shape::rectangle(glm::vec2 size, Colour colour)
{
    std::vector<Vertex2D> retval =
    {
        Vertex2D(glm::vec2(0.f), colour),
        Vertex2D(glm::vec2(0.f, size.y), colour),
        Vertex2D(size, colour),
        Vertex2D(glm::vec2(size.x, 0.f), colour),
        Vertex2D(glm::vec2(0.f), colour),
    };

    return retval;
}

std::vector<Vertex2D> Shape::circle(float radius, Colour colour, std::size_t pointCount)
{
    std::vector<Vertex2D> retval;

    float angle = cro::Util::Const::TAU / static_cast<float>(pointCount);
    for (auto i = 0u; i < pointCount; ++i) 
    {
        retval.emplace_back(glm::vec2(std::cos(angle * i), std::sin(angle * i)) * radius, colour);
    }
    retval.push_back(retval.front());

    return retval;
}