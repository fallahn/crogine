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

#include "PlayerDirector.hpp"
#include "ResourceIDs.hpp"
#include "Messages.hpp"

#include <crogine/detail/Types.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/PhysicsObject.hpp>
#include <crogine/ecs/Scene.hpp>

namespace
{
    constexpr float fixedUpdate = 1.f / 60.f;

    const float walkSpeed = 16.f;
    const float turnSpeed = 10.f;
    const float jumpVelocity = 40.f;
    const float maxRotation = cro::Util::Const::PI / 2.f;
}

PlayerDirector::PlayerDirector()
    : m_flags           (0),
    m_previousFlags     (0),
    m_accumulator       (0.f),
    m_playerRotation    (maxRotation)
{
    //registerStatusControls([]()
    //{

    //});
}

//private
void PlayerDirector::handleEvent(const cro::Event& evt)
{
    
}

void PlayerDirector::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::UIMessage)
    {
        const auto& data = msg.getData<UIEvent>();
        if (data.type == UIEvent::ButtonPressed)
        {

        }
        else
        {

        }
    }
}

void PlayerDirector::process(cro::Time dt)
{  
    static cro::int32 animID = -1;    
    cro::uint16 changed = m_flags ^ m_previousFlags;

    //do fixed update for player input

    //we need to set a limit because if there's a long loading
    //time the initial DT will be a HUGE dump 0.0
    m_accumulator += dt.asSeconds();// std::min(dt.asSeconds(), 1.f);
    while (m_accumulator > fixedUpdate)
    {
        m_accumulator -= fixedUpdate;

        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [this, changed](cro::Entity entity, cro::Time)
        {
            auto& tx = entity.getComponent<cro::Transform>();
            auto& playerState = entity.getComponent<Player>();
            auto& phys = entity.getComponent<cro::PhysicsObject>();

            //update any collision
            if (phys.getCollisionCount() > 0)
            {
                const auto& ids = phys.getCollisionIDs();
                const auto& manifolds = phys.getManifolds();

                for (auto i = 0u; i < phys.getCollisionCount(); ++i)
                {
                    auto otherEnt = getScene().getEntity(ids[i]);
                    const auto& otherPhys = otherEnt.getComponent<cro::PhysicsObject>();
                    if (otherPhys.getCollisionGroups() & CollisionID::Wall)
                    {
                        const auto& manifold = manifolds[i];
                        if (manifold.pointCount > 0)
                        {
                            //we have a collision
                        }
                    }
                }
            }


            //update orientation
            float rotation = m_playerRotation - tx.getRotation().y;
            if (std::abs(rotation) < 0.1f)
            {
                rotation = 0.f;
            }

            tx.rotate({ 0.f, 0.f, 1.f }, fixedUpdate * rotation * turnSpeed);
        };
    }

    if (animID != -1)
    {
        auto id = animID;
        //play new anim
        cro::Command animCommand;
        animCommand.targetFlags = CommandID::Player;
        animCommand.action = [id](cro::Entity entity, cro::Time)
        {
            entity.getComponent<cro::Skeleton>().play(id, 0.1f);
        };
        sendCommand(animCommand);
        animID = -1;
    }


    m_previousFlags = m_flags;
}