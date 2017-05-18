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

#include "PlayerController.hpp"
#include "ResourceIDs.hpp"
#include "VelocitySystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/util/Maths.hpp>

#include <SDL_events.h>

#include <glm/gtx/norm.hpp>

namespace
{
    enum Input
    {
        Up = 0x1,
        Down = 0x2,
        Left = 0x4,
        Right = 0x8,
        Fire = 0x10,
        StateChanged = (1 << 15)
    };

    const float playerAcceleration = 30.f;
    const float playerMaxSpeeedSqr = 25.f;
    const float maxRotation = 1.f;
}

PlayerController::PlayerController()
    : m_currentInput(0)
{

}

//public
void PlayerController::handleEvent(const cro::Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_w:
            m_currentInput |= Up;
            break;
        case SDLK_s:
            m_currentInput |= Down;
            break;
        case SDLK_a:
            m_currentInput |= Left;
            break;
        case SDLK_d:
            m_currentInput |= Right;
            break;
        case SDLK_SPACE:
            m_currentInput |= Fire;
            break;
        }
        //m_currentInput |= StateChanged;
        break;
    case SDL_KEYUP:
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_w:
            m_currentInput &= ~Up;
            break;
        case SDLK_s:
            m_currentInput &= ~Down;
            break;
        case SDLK_a:
            m_currentInput &= ~Left;
            break;
        case SDLK_d:
            m_currentInput &= ~Right;
            break;
        case SDLK_SPACE:
            m_currentInput &= ~Fire;
            break;
        }
        //m_currentInput |= StateChanged;
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (evt.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
            DPRINT("Axis value", std::to_string(evt.caxis.value));
        }
        break;
    }

#ifndef PLATFORM_DESKTOP //handle touch separately


#endif //PLATFORM_DESKTOP
}

void PlayerController::update(cro::CommandSystem* commandSystem)
{
    CRO_ASSERT(commandSystem, "Missing command system");

    if ((m_currentInput & 0xf) != 0) //only want movement input
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [this](cro::Entity entity, cro::Time dt)
        {
            glm::vec3 acceleration;
            float rotation = 0.f;
            const float dtSec = dt.asSeconds();

            if (m_currentInput & Up)
            {
                acceleration.y += 1.f;
                rotation = -maxRotation;
            }
            if (m_currentInput & Down)
            {
                acceleration.y -= 1.f;
                rotation += maxRotation;
            }
            if (m_currentInput & Left)
            {
                acceleration.x -= 1.f;
            }
            if (m_currentInput & Right)
            {
                acceleration.x += 1.f;
            }
            
            float len = glm::length2(acceleration);
            if (len > 1)
            {
                acceleration /= std::sqrt(len);
            }
            auto& velocity = entity.getComponent<Velocity>();
            velocity.velocity += acceleration * playerAcceleration * dtSec;

            const float currSpeed = glm::length2(velocity.velocity);
            //DPRINT("Current speed", std::to_string(currSpeed));
            if (currSpeed > playerMaxSpeeedSqr)
            {
                float ratio = playerMaxSpeeedSqr / currSpeed;
                velocity.velocity *= ratio;
            }

            auto& tx = entity.getComponent<cro::Transform>();
            const float currRotation = tx.getRotation().x;
            tx.setRotation({ currRotation + ((rotation - currRotation) * (dtSec * 4.f)), 0.f, 0.f });
        };
        commandSystem->sendCommand(cmd);
        m_currentInput &= ~StateChanged;
    }
}