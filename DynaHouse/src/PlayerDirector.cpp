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

#include <crogine/detail/Types.hpp>
#include <crogine/core/Message.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>

namespace
{
    constexpr float fixedUpdate = 1.f / 60.f;

    const float walkSpeed = 1.f;
    const float turnSpeed = 10.f;
    const float maxRotation = cro::Util::Const::PI / 2.f;

    const float cameraSpeed = 12.f;

    enum
    {
        Up = 0x1, Down = 0x2, Left = 0x4, Right = 0x8
    };
}

PlayerDirector::PlayerDirector()
    : m_flags           (0),
    m_accumulator       (0.f),
    m_playerRotation    (maxRotation),
    m_playerXPosition   (0.f)
{

}

//private
void PlayerDirector::handleEvent(const cro::Event& evt)
{
    cro::int32 animID = -1;

    if (evt.type == SDL_KEYDOWN && evt.key.repeat == 0)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_a:
            m_flags |= Left;
            //set rotation dest left
            m_playerRotation = -maxRotation;
            animID = AnimationID::BatCat::Run;
            break;
        case SDLK_d:
            m_flags |= Right;
            //set rotation dest right
            m_playerRotation = maxRotation;
            animID = AnimationID::BatCat::Run;
            break;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_a:
            m_flags &= ~Left;
            animID = AnimationID::BatCat::Idle;
            break;
        case SDLK_d:
            m_flags &= ~Right;
            animID = AnimationID::BatCat::Idle;
            break;
        }
    }
        
    if (animID > -1)
    {
        //play new anim
        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [animID](cro::Entity entity, cro::Time)
        {
            entity.getComponent<cro::Skeleton>().play(animID, 0.1f);
        };
        sendCommand(cmd);
    }
}

void PlayerDirector::handleMessage(const cro::Message& msg)
{

}

void PlayerDirector::process(cro::Time dt)
{  
    //do fixed update for player input

    //we need to set a limit because if there's a long loading
    //time the initial DT will be a HUGE dump 0.0
    m_accumulator += std::min(dt.asSeconds(), 1.f);
    while (m_accumulator > fixedUpdate)
    {
        m_accumulator -= fixedUpdate;

        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [this](cro::Entity entity, cro::Time)
        {
            auto& tx = entity.getComponent<cro::Transform>();
            float rotation = m_playerRotation - tx.getRotation().y;

            if (std::abs(rotation) < 0.1f)
            {
                rotation = 0.f;
            }

            tx.rotate({ 0.f, 0.f, 1.f }, fixedUpdate * rotation * turnSpeed);

            float movement = 0.f;
            if (m_flags & Right) movement = 1.f;
            if (m_flags & Left) movement -= 1.f;

            tx.move({ movement * fixedUpdate, 0.f, 0.f });

            m_playerXPosition = tx.getWorldPosition().x;
        };
        sendCommand(cmd);
    }

    //get the camera to follow player on X axis only
    //DPRINT("X pos", std::to_string(m_playerXPosition));
    cro::Command cmd;
    cmd.targetFlags = CommandID::Camera;
    cmd.action = [this](cro::Entity entity, cro::Time time)
    {
        auto& tx = entity.getComponent<cro::Transform>();
        float travel = m_playerXPosition - tx.getWorldPosition().x;
        tx.move({ travel * cameraSpeed * time.asSeconds(), 0.f, 0.f });
    };
    sendCommand(cmd);
}