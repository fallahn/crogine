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

#include "RopeSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    constexpr glm::vec3 Gravity(0.f, -9.f, 0.f);
}

RopeSystem::RopeSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(RopeSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<RopeNode>();
}

//public
void RopeSystem::process(float dt)
{
    for (auto& rope : m_ropes)
    {
        rope.simulate(dt);
    }
}

std::size_t RopeSystem::addRope(glm::vec3 start, glm::vec3 end, float slack)
{
    auto ret = m_ropes.size();
    m_ropes.emplace_back(start, end, slack, *getScene(), ret);

    registerWindow([&]()
        {
            ImGui::Begin("sdfsd");
            for (auto n : m_ropes.back().m_nodes)
            {
                const auto& node = n.getComponent<RopeNode>();
                ImGui::Text("Pos: %3.2f, %3.2f, %3.2f", node.position.x, node.position.y, node.position.z);
                ImGui::Text("Fixed: %s", node.fixed ? "true" : "false");
            }
            ImGui::End();
        });

    return ret;
}

//private
void RopeSystem::onEntityAdded(cro::Entity e)
{
    const auto& node = e.getComponent<RopeNode>();
    if (node.ropeID < m_ropes.size())
    {
        m_ropes[node.ropeID].addNode(e);
    }
    else
    {
        LogW << "Tried adding node to rope " << node.ropeID << " which doesn't exist." << std::endl;
        getScene()->destroyEntity(e);
    }
}

void RopeSystem::onEntityRemoved(cro::Entity e)
{
    const auto& node = e.getComponent<RopeNode>();
    CRO_ASSERT(node.ropeID < m_ropes.size(), "");

    m_ropes[node.ropeID].removeNode(e);
}


//-------rope class-------//
Rope::Rope(glm::vec3 start, glm::vec3 end, float /*slack*/, cro::Scene& scene, std::size_t id)
    : m_startPoint  (start),
    m_endPoint      (end),
    m_nodeSpacing   (0.f)
{
    auto ent = scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<RopeNode>().position = start;
    ent.getComponent<RopeNode>().prevPosition = start;
    ent.getComponent<RopeNode>().ropeID = id;

    ent = scene.createEntity();
    ent.addComponent<cro::Transform>();
    ent.addComponent<RopeNode>().position = end;
    ent.getComponent<RopeNode>().prevPosition = end;
    ent.getComponent<RopeNode>().ropeID = id;
}

//public
void Rope::addNode(cro::Entity e)
{
    m_nodes.push_back(e);
    recalculate();
}

void Rope::removeNode(cro::Entity e)
{
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(), 
        [e](cro::Entity ent)
        {
            return e == ent;
        }), m_nodes.end());
    recalculate();
}

void Rope::simulate(float dt)
{
    if (m_nodes.size() > 2)
    {
        integrate(dt);
        constrain();
    }
}

//private
void Rope::recalculate()
{
    if (!m_nodes.empty())
    {
        if (m_nodes.size() > 1)
        {
            auto position = m_startPoint;
            auto stride = m_endPoint - position;

            if (glm::length2(stride) != 0)
            {
                float len = glm::length(stride) + 0.001f;

                //TODO add any slack we want to stride
                //which should be a RATIO not a fixed length
                m_nodeSpacing = len;

                stride = glm::normalize(stride) * (len / (m_nodes.size() - 1));
            }

            for (auto node : m_nodes)
            {
                auto& n = node.getComponent<RopeNode>();
                n.position = position;
                n.prevPosition = position;
                n.force = glm::vec3(0.f);
                n.fixed = false;

                position += stride;
            }
        }

        m_nodes.front().getComponent<RopeNode>().fixed = true;
        m_nodes.back().getComponent<RopeNode>().fixed = true;
    }    
}

void Rope::integrate(float dt)
{
    for (auto node : m_nodes)
    {
        auto& n = node.getComponent<RopeNode>();
        //this means we're delayed one frame, but it has to happen
        //after we apply the constraints from the previous integration
        node.getComponent<cro::Transform>().setPosition(n.position);

        if (!n.fixed)
        {
            auto old = n.position;
            n.position = 2.f * n.position - n.prevPosition + (n.force + Gravity) * dt * dt;
            n.prevPosition = old;
        }
    }
}

void Rope::constrain()
{
    static constexpr std::size_t MaxIterations = 50;
    for (auto i = 0u; i < MaxIterations; ++i)
    {
        for (auto j = 1u; j < m_nodes.size(); ++j)
        {
            auto& n1 = m_nodes[j - 1].getComponent<RopeNode>();
            auto& n2 = m_nodes[j].getComponent<RopeNode>();

            auto dir = n2.position - n1.position;
            const float dist = glm::length(dir);
            const float error = dist - m_nodeSpacing;

            dir /= dist;
            dir *= error;

            if (n1.fixed && !n2.fixed)
            {
                n2.position -= dir;
            }
            else if (n2.fixed && !n1.fixed)
            {
                n1.position += dir;
            }
            else
            {
                //we should never have 2 next to each other fixed as
                //we don't call this with fewer than 3 nodes
                dir *= 0.5f;
                n2.position -= dir;
                n1.position += dir;
            }
        }
    }
}