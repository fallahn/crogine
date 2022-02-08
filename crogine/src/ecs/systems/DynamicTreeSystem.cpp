/*-----------------------------------------------------------------------

Copyright (c) 2009 Erin Catto http://www.box2d.org
Matt Marchant 2017 - 2022
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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/DynamicTreeComponent.hpp>
#include <crogine/ecs/systems/DynamicTreeSystem.hpp>

using namespace cro;

DynamicTreeSystem::DynamicTreeSystem(MessageBus& mb, float unitsPerMetre)
    : System(mb, typeid(DynamicTreeSystem)),
    m_tree  (unitsPerMetre)
{
    requireComponent<DynamicTreeComponent>();
    requireComponent<Transform>();
}

//public
void DynamicTreeSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        if (!entity.destroyed())
        {
            auto& bpc = entity.getComponent<DynamicTreeComponent>();
            const auto& tx = entity.getComponent<Transform>();
            auto worldPosition = tx.getWorldPosition();
            auto worldBounds = bpc.m_bounds;

            worldBounds += tx.getOrigin();
            worldBounds = tx.getWorldTransform() * worldBounds;

            m_tree.moveNode(bpc.m_treeID, worldBounds, worldPosition - bpc.m_lastWorldPosition);

            bpc.m_lastWorldPosition = worldPosition;
        }
    }
}

void DynamicTreeSystem::onEntityAdded(Entity entity)
{
    entity.getComponent<DynamicTreeComponent>().m_treeID = m_tree.addToTree(entity);
}

void DynamicTreeSystem::onEntityRemoved(Entity entity)
{
    m_tree.removeFromTree(entity.getComponent<DynamicTreeComponent>().m_treeID);
}

std::vector<Entity> DynamicTreeSystem::query(Box area, std::uint64_t filter) const
{
    Detail::FixedStack<std::int32_t, 256> stack;
    stack.push(m_tree.getRoot());

    std::vector<Entity> retVal;
    retVal.reserve(256);

    while (stack.size() > 0)
    {
        auto treeID = stack.pop();
        if (treeID == Detail::TreeNode::Null)
        {
            continue;
        }

        const auto& node = m_tree.getNodes()[treeID];
        if (area.intersects(node.fatBounds))
        {
            //TODO it would be nice to precache the filter fetch, but it would miss changes at the component level
            if (node.isLeaf() && node.entity.isValid()
                && (node.entity.getComponent<DynamicTreeComponent>().m_filterFlags & filter)) 
            {
                //we have a candidate, stash
                retVal.push_back(node.entity);
            }
            else
            {
                stack.push(node.childA);
                stack.push(node.childB);
            }
        }
    }
    return retVal;
}

//private