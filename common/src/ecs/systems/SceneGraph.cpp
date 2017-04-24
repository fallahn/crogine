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

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <functional>

using namespace cro;

SceneGraph::SceneGraph(MessageBus& mb)
    : System(mb, this)
{
    requireComponent<Transform>();
}

//public
void SceneGraph::process(Time dt)
{
    std::vector<int32> updateList;

    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        //get entity tx node
        auto& tx = entity.getComponent<Transform>();
        bool addToList = false;

        //if node has dirty parent flag update parent with new child
        if (tx.m_dirtyFlags & Transform::Parent)
        {
            if (tx.m_parent > -1) //parent was added
            {
                if (!getScene()->getEntity(tx.m_parent).getComponent<Transform>().addChild(entity.getIndex()))
                {
                    tx.m_parent = -1;
                    LOG("Failed adding tx to parent - too many children already", Logger::Type::Error);
                }
            }
            if (tx.m_lastParent > -1) //remove old parent
            {
                //remove the parent from all the children
                auto& children = getScene()->getEntity(tx.m_lastParent).getComponent<Transform>().m_children;
                for (auto& c : children)
                {
                    getScene()->getEntity(c).getComponent<Transform>().removeParent();
                }

                tx.m_lastParent = -1;
            }
            addToList = true;
            tx.m_dirtyFlags &= ~Transform::Parent;
        }

        //if node has dirty child find child and update its parent
        if (tx.m_dirtyFlags & Transform::Child)
        {
            auto& children = tx.m_children;
            for (auto c : children)
            {
                if (c == -1)
                {
                    break;
                }
                getScene()->getEntity(c).getComponent<Transform>().setParent(entity);
            }

            auto& removedChildren = tx.m_removedChildren;
            for (auto c : removedChildren)
            {
                getScene()->getEntity(c).getComponent<Transform>().removeParent();
            }
            removedChildren.clear();
            addToList = true;
            tx.m_dirtyFlags &= ~Transform::Child;
        }

        //check transform for dirty flag        
        if (tx.m_dirtyFlags & Transform::Tx || addToList)
        {
            //walk to bottom and add to list
            std::function<void (Transform&)> getLastNode = 
                [&](Transform& xform)
            {
                if (xform.m_children[0] == -1)
                {
                    updateList.push_back(xform.m_id);
                    return;
                }
                for (auto c : xform.m_children)
                {
                    if (c == -1)
                    {
                        break;
                    }
                    getLastNode(getScene()->getEntity(c).getComponent<Transform>());
                }
            };
            getLastNode(tx);
        }
    }

    //for each ID in the list recursively update transform (TODO could find a way to only go as far as last dirty)
    //make sure to remove all dirty flags as we go
    std::sort(updateList.begin(), updateList.end());
    auto end = std::unique(updateList.begin(), updateList.end());
    for (auto i = updateList.begin(); i != end; ++i)
    {
        std::function<glm::mat4 (Transform&)> getWorldTransform = 
            [&, this](Transform& xform)->glm::mat4
        {
            if (xform.m_parent > -1)
            {
                auto wtx = getWorldTransform(getScene()->getEntity(xform.m_parent).getComponent<Transform>()) * xform.getLocalTransform();
                xform.m_dirtyFlags &= ~Transform::Tx;
                return wtx;
            }
            auto ltx = xform.getLocalTransform();
            xform.m_dirtyFlags &= ~Transform::Tx;
            return ltx;
        };
        auto& tx = getScene()->getEntity(*i).getComponent<Transform>();
        tx.m_worldTransform = getWorldTransform(tx);
    }
    /*int32 dbid = updateList.empty() ? 0 : updateList[0];
    DPRINT("update Entity", std::to_string(std::distance(updateList.begin(), end)));*/
}

void SceneGraph::handleMessage(const Message& msg)
{
    std::function<void(std::array<int32, Transform::MaxChildren>&)> destroyChildren = 
        [&, this](std::array<int32, Transform::MaxChildren>& children)
    {
        for (auto& c : children)
        {
            if (c > -1)
            {
                auto entity = getScene()->getEntity(c);
                destroyChildren(entity.getComponent<Transform>().m_children);
                entity.destroy();
                c = -1; //mark the child as gone else the message from destroying this
                //entity will become infinitely recursive...
            }
            else
            {
                break;
            }
        }
    };
    
    //remove all children when an entity dies
    //TODO does this entity still exist by the time we get here?
    if (msg.id == Message::SceneMessage)
    {
        const auto& data = msg.getData<Message::SceneEvent>();
        auto entity = getScene()->getEntity(data.entityID);
        if (entity.hasComponent<Transform>())
        {
            auto& children = entity.getComponent<Transform>().m_children;
            destroyChildren(children);
        }
        LOG("Removing children from dead entity", Logger::Type::Info);
    }
}

//private
void SceneGraph::onEntityAdded(Entity entity)
{
    //nab our entity's index
    entity.getComponent<Transform>().m_id = entity.getIndex();
}
