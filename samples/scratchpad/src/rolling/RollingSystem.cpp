/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "RollingSystem.hpp"
#include "Utility.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include <reactphysics3d/reactphysics3d.h>

namespace rp = reactphysics3d;

namespace
{
    float debugVel = 0.f;
    glm::vec3 debugDir = glm::vec3(0.f);
    std::int32_t debugState = 0;

    std::array StateNames =
    {
        std::string("Air"),
        std::string("Rolling"),
        std::string("Sleep")
    };

    struct RollerCallback final : public rp::CollisionCallback
    {
        explicit RollerCallback(Roller& roller)
            : m_roller(roller){}

        void onContact(const CallbackData& callbackData) override
        {
            if (callbackData.getNbContactPairs())
            {
                auto pair = callbackData.getContactPair(0);
                //if (pair.getEventType() == rp::CollisionCallback::ContactPair::EventType::ContactStart)
                {
                    m_roller.manifold.penetration = pair.getContactPoint(0).getPenetrationDepth();
                    m_roller.manifold.normal = toGLM(pair.getContactPoint(0).getWorldNormal());
                    m_roller.manifold.colliding = true;
                
                    if (pair.getBody1() == m_roller.body)
                    {
                        m_roller.manifold.normal *= -1.f;
                    }
                }
            }
        }

        Roller& m_roller;
    };
}

RollSystem::RollSystem(cro::MessageBus& mb, rp::PhysicsWorld& pw)
    : cro::System   (mb, typeid(RollSystem)),
    m_physWorld     (pw)
{
    requireComponent<Roller>();
    requireComponent<cro::Transform>();

    registerWindow([&]()
        {
            if (ImGui::Begin("They see me rollin'"))
            {
                ImGui::Text("Velocity: %3.3f m/s", debugVel);
                ImGui::Text("Dir: %3.3f, %3.3f, %3.3f", debugDir.x, debugDir.y, debugDir.z);
                ImGui::Text("State: %s", StateNames[debugState].c_str());
            }
            ImGui::End();
        });
}

//public
void RollSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& roller = entity.getComponent<Roller>();
        auto& tx = entity.getComponent<cro::Transform>();

        roller.prevVelocity = roller.velocity;

        if (roller.state == Roller::Air)
        {
            roller.velocity += Gravity;

            RollerCallback cb(roller);
            m_physWorld.testCollision(roller.body, cb);

            if (roller.manifold.colliding)
            {
                //as we're effectively moving back up the arc to the collision
                //point we need to recalc the velocity as it would have been, 
                //through interpolation.
                auto targetPos = tx.getPosition() + (roller.manifold.normal * roller.manifold.penetration);
                auto travel = glm::length(tx.getPosition() - roller.prevPosition);
                auto targetTravel = glm::length(targetPos - roller.prevPosition);
                float interp = std::min(1.f, travel / targetTravel);

                roller.velocity = roller.prevVelocity + (Gravity * interp);
                tx.setPosition(targetPos);

                roller.velocity = glm::reflect(roller.velocity, roller.manifold.normal);
                roller.velocity *= 0.15f;

                roller.manifold.colliding = false;

                if (roller.velocity.y < 0.1f)
                {
                    roller.state = Roller::Roll;
                }
            }
        }
        else if (roller.state == Roller::Roll)
        {
            RollerCallback cb(roller);
            m_physWorld.testCollision(roller.body, cb);

            //funny how this is copy/pasted from the collision scratch
            //but behaves *completely* differently

            if (roller.manifold.colliding)
            {
                auto normal = roller.manifold.normal;

                float gravityForce = roller.mass * -Gravity.y;
                float amountX = glm::dot(cro::Transform::X_AXIS, normal);
                float amountZ = glm::dot(cro::Transform::Z_AXIS, normal);

                //a = f/m baby
                glm::vec3 accel((gravityForce * amountX) / roller.mass, 0.f, (gravityForce * amountZ) / roller.mass);
                roller.velocity += accel;

                //roller.velocity *= roller.friction;

                tx.move(roller.manifold.normal * roller.manifold.penetration);

                if (glm::length(roller.velocity) < 0.05f)
                {
                    roller.velocity = glm::vec3(0.f);
                    roller.state = Roller::Sleep;
                }

                roller.manifold.colliding = false;
            }
            /*else
            {
                roller.state = Roller::Air;
            }*/
        }


        debugVel = glm::length(roller.velocity);
        debugDir = roller.velocity;
        debugState = roller.state;

        roller.prevPosition = tx.getPosition();
        tx.move(roller.velocity * dt);
        roller.body->setTransform({ toR3D(tx.getPosition()), toR3D(tx.getRotation()) });

        if (tx.getPosition().y < -40.f)
        {
            tx.setPosition(roller.resetPosition);
            roller.prevPosition = roller.resetPosition;
            roller.state = Roller::Air;
            roller.velocity = glm::vec3(0.f);
            roller.prevVelocity = roller.velocity;
        }
    }
}

//private
void RollSystem::onEntityRemoved(cro::Entity e)
{
    m_physWorld.destroyCollisionBody(e.getComponent<Roller>().body);
}