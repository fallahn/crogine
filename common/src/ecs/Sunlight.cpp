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

#include <crogine/ecs/Sunlight.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace cro;

Sunlight::Sunlight()
    : m_colour(1.f, 1.f, 1.f),
    m_direction(0.f, -1.f, 0.f)
{
    m_projection = glm::perspective(1.6f, 1.f, 0.1f, 100.f);
}

//public
void Sunlight::setColour(cro::Colour colour)
{
    m_colour = colour;
}

cro::Colour Sunlight::getColour() const
{
    return m_colour;
}

void Sunlight::setDirection(glm::vec3 direction)
{
    m_direction = direction;
}

glm::vec3 Sunlight::getDirection() const
{
    return m_direction;
}

const glm::mat4& Sunlight::getProjectionMatrix() const
{
    return m_projection;
}

void Sunlight::setViewProjectionMatrix(const glm::mat4& mat)
{
    m_viewProjection = mat;
}

const glm::mat4& Sunlight::getViewProjectionMatrix() const
{
    return m_viewProjection;
}