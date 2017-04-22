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
#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/SceneNode.hpp>
#include <crogine/ecs/components/Transform.hpp>

using namespace cro;

SceneGraph::SceneGraph(MessageBus& mb)
    : System(mb, this)
{
    requireComponent<Transform>();
    requireComponent<SceneNode>();
}

//public
void SceneGraph::process(Time dt)
{
    //when updating world transform make sure we have no children so we're only ever working from the bottom up
    //ie the recursive nature updates ALL world transforms it touches

    //scene node dirty flags needs to say if parent / child was updated
    //so we can update the new parent with a child and vice versa
}

void SceneGraph::handleMessage(const Message& msg)
{
    //remove all children when an entity dies
}

//private
void SceneGraph::onEntityAdded(Entity entity)
{
    //nab our entity's index
    entity.getComponent<SceneNode>().m_id = entity.getIndex();
}