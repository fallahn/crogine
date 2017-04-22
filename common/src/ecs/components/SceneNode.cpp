/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/ecs/components/SceneNode.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/Assert.hpp>

using namespace cro;

SceneNode::SceneNode()
    : m_parent      (-1),
    m_id            (-1),
    m_needsUpdate   (true)
{

}

//public
void SceneNode::setParent(Entity parent)
{
    CRO_ASSERT(parent.hasComponent<Transform>(), "Parent must have a transform component");
    CRO_ASSERT(parent.hasComponent<SceneNode>(), "Parent must have a scene node component");
    CRO_ASSERT(parent.getIndex() != m_id, "Can't parent to ourself!");
    m_parent = parent.getIndex();

    m_needsUpdate = true;
}

void SceneNode::removeParent()
{
    m_parent = -1;
    m_needsUpdate = true;
}

void SceneNode::addChild(uint32 id)
{
    auto freeSlot = std::find(std::begin(m_children), std::end(m_children), -1);
    if (freeSlot != std::end(m_children))
    {
        *freeSlot = id;
    }

    std::sort(std::begin(m_children), std::end(m_children), [](int32 i, int32 j) { return i > j; });

    //mark for update
    m_needsUpdate = true;
}

void SceneNode::removeChild(uint32 id)
{
    auto freeSlot = std::find(std::begin(m_children), std::end(m_children), id);
    if (freeSlot != std::end(m_children))
    {
        *freeSlot = -1;
    }

    std::sort(std::begin(m_children), std::end(m_children), [](int32 i, int32 j) { return i > j; });

    //mark for update
    m_needsUpdate = true;
}