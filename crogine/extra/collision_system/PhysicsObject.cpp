/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "PhysicsObject.hpp"

using namespace cro;

PhysicsObject::PhysicsObject(float mass, float density)
    : m_mass            (mass),
    m_density           (density),
    m_collisionGroups   (1),
    m_collisionFlags    (-1),
    m_shapeCount        (0),
    m_collisionCount    (0)
{

}

//public
bool PhysicsObject::addShape(const PhysicsShape& shape)
{
    if (m_shapeCount < m_shapes.size())
    {
        m_shapes[m_shapeCount++] = shape;
        return true;
    }
    LOG("Max shapes for this object reached", Logger::Type::Warning);
    return false;
}