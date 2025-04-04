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
#include <crogine/graphics/ImageArray.hpp>

//component on an entity, used to
//update the entity final position
struct RopeNode final
{
    glm::vec3 position = glm::vec3(0.f);
    glm::vec3 prevPosition = glm::vec3(0.f);
    //this is used as a fixed position in the wind map to sample from
    glm::vec3 samplePosition = glm::vec3(0.f);
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
    /*
    start and end are fixed points. Slack is a multiplier which
    increases the length of the rope between the start and end point
    EG slack of 0 between two points 2m apart creates a 2m rope,
    and a slack of 1 adds 1x 2m to create a 4m rope. 0.5 adds 0.5 x 2m
    to create a 3m rope and so on.
    */
    Rope(glm::vec3 start, glm::vec3 end, float slack, cro::Scene&, std::size_t id);
    
    void addNode(cro::Entity);

    void removeNode(cro::Entity);

    //takes the noise map offset
    void simulate(float, glm::vec3);

    const std::vector<glm::vec3>& getNodePositions() const { return m_nodePositions; }

    void setNoiseMap(const cro::ImageArray<float>&, float scale);

private:
    
    glm::vec3 m_startPoint;
    glm::vec3 m_endPoint;
    float m_slack;

    const cro::ImageArray<float>* m_noiseMap;
    glm::vec3 m_pixelsPerMetre;

    //we iterate over these in the simulation rather than the system's entity list
    std::vector<cro::Entity> m_nodes;
    std::vector<glm::vec3> m_nodePositions; //track these so we can EG draw the rope in a shader
    float m_nodeSpacing;
    void recalculate(); //called when adding a new node

    void integrate(float dt, glm::vec3);
    void constrain();
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

    //note that these are local space relative to the root node
    const std::vector<glm::vec3>& getNodePositions(std::size_t ropeID) const;


    //sets a tileable noise texture to use as a wind effect
    //ideally each channel should contains a slightly different
    //noise pattern, as RGB channels map to XYZ in world space.
    //Size is the number of metres in world space which this 
    //texture covers before it starts to tile.
    void setNoiseTexture(const std::string& path, float size = 4.f);

    //sets the wind strength and direction
    void setWind(glm::vec3 w) { m_windDirection = w; }

private:
    std::vector<Rope> m_ropes;

    cro::ImageArray<float> m_windImage;
    float m_imageScale;
    glm::vec3 m_windDirection;
    glm::vec3 m_windOffset;

    //validates the rope in the component ID
    //exists and adds it to the rope if it does
    void onEntityAdded(cro::Entity) override;

    //removes the entity from a rope
    void onEntityRemoved(cro::Entity) override;
};