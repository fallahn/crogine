/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#pragma once

#include <crogine/ecs/System.hpp>
#include <crogine/gui/GuiClient.hpp>

//component on an entity, used to
//update the entity final position
struct RopeNode final
{
    glm::vec3 position = glm::vec3(0.f);
    glm::vec3 prevPosition = glm::vec3(0.f);
    bool fixed = false;

    //ID of the rope to which this node
    //will belong. Must be valid when ent
    //is created, editing afterwards has
    //no effect. ID is returned from
    //RopeSystem::addRope();
    std::size_t ropeID = std::numeric_limits<std::size_t>::max();

    //any external force added to this node
    glm::vec3 force = glm::vec3(0.f);
};

//represents a collection of nodes in
//a single verlet simulation. Multiple of
//these may exist in the system
class Rope final
{
public:
    Rope(glm::vec3 start, glm::vec3 end, float slack, cro::Scene&, std::size_t id);
    
    void addNode(cro::Entity);

    //it's possible to destroy entities outside
    //of this system - this func just tidies up loose
    //references - I have no idea how it'll behave though...
    void removeNode(cro::Entity);

    void simulate(float);

private:
    
    glm::vec3 m_startPoint;
    glm::vec3 m_endPoint;

    //we iterate over these in he simaultion rather than the system's entity list
    std::vector<cro::Entity> m_nodes;
    float m_nodeSpacing;
    void recalculate(); //called when adding a new node

    void integrate(float dt);
    void constrain();

    friend class RopeSystem; //TODO remove this when done debugging
};

class RopeSystem final : public cro::System, public cro::GuiClient
{
public:
    explicit RopeSystem(cro::MessageBus&);

    void process(float) override;

    //creates a new rope between the given fixed points.
    //returns an ID to use with components when assigning
    //those entities to a specific rope.
    //increase slack for a looser rope between fixed points
    std::size_t addRope(glm::vec3, glm::vec3, float slack = 0.f);

private:
    std::vector<Rope> m_ropes;

    //validates the rope in the component ID
    //exists and adds it to the rop if it does
    void onEntityAdded(cro::Entity) override;

    //removes the entity from a rope
    void onEntityRemoved(cro::Entity) override;
};