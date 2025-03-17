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

#include <crogine/ecs/Component.hpp>
#include <crogine/ecs/Entity.hpp>

using namespace cro;

namespace
{
    constexpr std::uint32_t IndexMask = (1 << Detail::IndexBits) - 1;
    constexpr std::uint32_t GenerationMask = (1 << Detail::GenerationBits) - 1;
}

Entity::Entity()
    : m_id          ((0 << Detail::IndexBits) | std::numeric_limits<std::uint32_t>::max()),
    m_entityManager (nullptr)
{

}

Entity::Entity(std::uint32_t index, std::uint8_t generation)
    : m_id          ((generation << Detail::IndexBits) | index),
    m_entityManager (nullptr)
{

}

//public
std::uint32_t Entity::getIndex() const
{
    return m_id & IndexMask;
}

std::uint8_t Entity::getGeneration() const
{
    return (m_id >> Detail::IndexBits) & GenerationMask;
}

bool Entity::destroyed() const
{
    CRO_ASSERT(m_entityManager, "Invalid Entity instance");
    return m_entityManager->entityDestroyed(*this);
}

const ComponentMask& Entity::getComponentMask() const
{
    CRO_ASSERT(isValid(), "Invalid Entity instance");
    return m_entityManager->getComponentMask(*this);
}

bool Entity::isValid() const
{
    return (m_entityManager != nullptr && m_entityManager->entityValid(*this));
}

void Entity::setLabel(const std::string& label)
{
    CRO_ASSERT(isValid(), "Not a valid entity");
    m_entityManager->setLabel(*this, label);
}

const std::string& Entity::getLabel() const
{
    CRO_ASSERT(isValid(), "Not a valid entity");
    return m_entityManager->getLabel(*this);
}