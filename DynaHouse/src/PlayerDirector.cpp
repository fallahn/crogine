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

    const glm::vec3 gravity(0.f, -9.f, 0.f);
}

PlayerDirector::PlayerDirector()
    : m_flags           (0),
    m_accumulator       (0.f),
    m_playerRotation    (maxRotation),
    m_playerXPosition   (0.f),
    m_canJump           (true)
{

}

//private
void PlayerDirector::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_a:
            m_flags |= Left;
            break;
        case SDLK_d:
            m_flags |= Right;
            break;
        case SDLK_w:
            m_flags |= Up;
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
            break;
        case SDLK_d:
            m_flags &= ~Right;
            break;
        case SDLK_w:
            m_flags &= ~Up;
            m_canJump = true;
            break;
        }
    }
}

void PlayerDirector::handleMessage(const cro::Message& msg)
{

}

void PlayerDirector::process(cro::Time dt)
{  
    static cro::int32 animID = -1;    
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
            //update orientation
            auto& tx = entity.getComponent<cro::Transform>();
            auto& playerState = entity.getComponent<Player>();
            float rotation = m_playerRotation - tx.getRotation().y;

            if (std::abs(rotation) < 0.1f)
            {
                rotation = 0.f;
            }

            tx.rotate({ 0.f, 0.f, 1.f }, fixedUpdate * rotation * turnSpeed);
            //update if jumping
            if (playerState.state == Player::State::Jumping)
            {
                tx.move(m_playerVelocity * fixedUpdate);
                m_playerVelocity += (gravity * fixedUpdate);
                if (tx.getWorldPosition().y < 0)
                {
                    tx.move({ 0.f, -tx.getWorldPosition().y, 0.f });
                    playerState.state = Player::State::Idle;
                    animID = AnimationID::BatCat::Idle;
                }
            }


            //update input
            float movement = 0.f;
            if (m_flags & Right) movement = 1.f;
            if (m_flags & Left) movement -= 1.f;

            tx.move({ movement * fixedUpdate, 0.f, 0.f });
            m_playerXPosition = tx.getWorldPosition().x;


            //update state           
            auto oldState = playerState.state;
            if ((m_flags & Up) && m_canJump)
            {
                playerState.state = Player::State::Jumping;
                m_canJump = false;
            }

            if (playerState.state != Player::State::Jumping)
            {
                if (movement != 0)
                {
                    playerState.state = Player::State::Running;
                    m_playerRotation = maxRotation * movement;
                }
                else
                {
                    playerState.state = Player::State::Idle;
                }
            }
            else
            {
                if (movement != 0)
                {
                    m_playerRotation = maxRotation * movement;
                }
            }

            if (playerState.state != oldState)
            {
                switch (playerState.state)
                {
                default: break;
                case Player::State::Idle:
                    animID = AnimationID::BatCat::Idle;
                    break;
                case Player::State::Running:
                    animID = AnimationID::BatCat::Run;
                    break;
                case Player::State::Jumping:
                    animID = AnimationID::BatCat::Jump;
                    m_playerVelocity.y = 3.f;
                    break;
                }
            }

            //DPRINT("Player State", std::to_string((int)playerState.state));
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
}