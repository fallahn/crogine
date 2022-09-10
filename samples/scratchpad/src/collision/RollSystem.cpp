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

#include "RollSystem.hpp"
#include "Utils.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

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
}

RollingSystem::RollingSystem(cro::MessageBus& mb, std::unique_ptr<btCollisionWorld>& cw, std::unique_ptr<btCollisionDispatcher>& cd)
    : cro::System           (mb, typeid(RollingSystem)),
    m_collisionWorld        (cw),
    m_collisionDispatcher   (cd)
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
void RollingSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& roller = entity.getComponent<Roller>();
        auto& tx = entity.getComponent<cro::Transform>();

        roller.prevPosition = tx.getPosition();
        roller.prevVelocity = roller.velocity;

        tx.move(roller.velocity * dt);

        if (roller.state == Roller::Air)
        {
            roller.velocity += Gravity;

            auto worldPos = fromGLM(tx.getPosition());
            auto res = collisionTest(worldPos, m_collisionWorld.get());

            if (res.hasHit())
            {
                //as we're effectively moving back up the arc to the collision
                //point we need to recalc the velocity as it would have been, 
                //through interpolation.
                auto targetPos = toGLM(res.m_hitPointWorld);
                auto travel = glm::length(tx.getPosition() - roller.prevPosition);
                auto targetTravel = glm::length(targetPos - roller.prevPosition);
                float interp = std::min(1.f, travel / targetTravel);

                roller.velocity = roller.prevVelocity + (Gravity * interp);
                tx.setPosition(targetPos);

                roller.velocity = glm::reflect(roller.velocity, -toGLM(res.m_hitNormalWorld));
                roller.velocity *= 0.55f;
            }

            if (roller.velocity.y < 0.1f)
            {
                worldPos = fromGLM(tx.getPosition() - glm::vec3(0.f, 0.02f, 0.f));
                res = collisionTest(worldPos, m_collisionWorld.get());
                
                if (res.hasHit())
                {
                    roller.state = Roller::Roll;
                }
            }
        }
        else if (roller.state == Roller::Roll)
        {
            auto worldPos = fromGLM(tx.getPosition() - glm::vec3(0.f, 0.5f, 0.f));
            auto res = collisionTest(worldPos, m_collisionWorld.get());

            if (res.hasHit())
            {
                auto normal = toGLM(res.m_hitNormalWorld);

                float gravityForce = roller.mass * -Gravity.y;
                float amountX = -glm::dot(cro::Transform::X_AXIS, normal);
                float amountZ = -glm::dot(cro::Transform::Z_AXIS, normal);

                //a = f/m baby
                glm::vec3 accel((gravityForce * amountX) / roller.mass, 0.f, (gravityForce * amountZ) / roller.mass);
                roller.velocity += accel;

                roller.velocity *= roller.friction;

                tx.setPosition(toGLM(res.m_hitPointWorld));

                if (glm::length(roller.velocity) < 0.05f)
                {
                    roller.velocity = glm::vec3(0.f);
                    roller.state = Roller::Sleep;
                }
            }
            else
            {
                roller.state = Roller::Air;
            }
        }


        debugVel = glm::length(roller.velocity);
        debugDir = roller.velocity;
        debugState = roller.state;

        if (tx.getPosition().y < -10.f)
        {
            tx.setPosition(roller.resetPosition);
            roller.state = Roller::Air;
            roller.velocity = glm::vec3(0.f);
        }
    }
}