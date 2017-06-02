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
#include "VelocitySystem.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/GameController.hpp>
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

    const float playerAcceleration = 0.5f;
    const float playerJoyAcceleration = playerAcceleration * 60.f;
    const float playerMaxSpeeedSqr = 25.f;
    const float maxRotation = 1.f;

    const cro::uint16 JoyThresh = 2500;
    const cro::uint16 JoyMax = 32767;
    const float JoyMaxSqr = static_cast<float>(JoyMax * JoyMax);
    const float JoySpeedMin = static_cast<float>(JoyThresh) / JoyMax;
}

PlayerDirector::PlayerDirector()
    : m_currentInput(0),
    m_fingerDown    (false)
{

}

//public
void PlayerDirector::handleEvent(const cro::Event& evt)
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
        
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        if (evt.cbutton.which == 0)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                m_currentInput |= Up;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                m_currentInput |= Down;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                m_currentInput |= Left;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                m_currentInput |= Right;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                m_currentInput |= Fire;
                break;
            }
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (evt.cbutton.which == 0)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                m_currentInput &= ~Up;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                m_currentInput &= ~Down;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                m_currentInput &= ~Left;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                m_currentInput &= ~Right;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                m_currentInput &= ~Fire;
                break;
            }
        }
        break;


#ifndef PLATFORM_DESKTOP //handle touch separately
    case SDL_FINGERDOWN:
        m_fingerDown = true;
        m_currentInput |= Fire;
    case SDL_FINGERMOTION:
        m_touchCoords.x = evt.tfinger.x;
        m_touchCoords.y = evt.tfinger.y;
        break;
    case SDL_FINGERUP:
        m_fingerDown = false;
        m_currentInput &= ~Fire;
        break;
#endif //PLATFORM_DESKTOP

    }
}

void PlayerDirector::process(cro::Time)
{
    //controller analogue
    glm::vec3 joyVec = {
        static_cast<float>(cro::GameController::getAxis(0, cro::GameController::AxisLeftX)),
        static_cast<float>(cro::GameController::getAxis(0, cro::GameController::AxisLeftY)), 0.f };
    joyVec.y = -joyVec.y;
    float joystickAmount = glm::length2(joyVec) / JoyMaxSqr;
    if (joystickAmount < JoySpeedMin) joystickAmount = 0.f;

    if (joystickAmount > 0)
    {
        joyVec = glm::normalize(joyVec);
        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [=](cro::Entity entity, cro::Time dt)
        {
            static const float updateTime = 1.f / 60.f;
            static float accumulator = 0.f;

            accumulator += dt.asSeconds();
            while (accumulator > updateTime)
            {
                auto& velocity = entity.getComponent<Velocity>();
                velocity.velocity += joyVec * joystickAmount * playerJoyAcceleration * updateTime;
                accumulator -= updateTime;
            }

            float rotation = -maxRotation * joyVec.y;
            auto& tx = entity.getComponent<cro::Transform>();
            const float currRotation = tx.getRotation().x;
            tx.setRotation({ currRotation + ((rotation - currRotation) * (dt.asSeconds() * 4.f)), 0.f, 0.f });

        };
        sendCommand(cmd);
    }

    //keyboard input
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
            velocity.velocity += acceleration * (playerAcceleration/* * dtSec*/);

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
        sendCommand(cmd);
        m_currentInput &= ~StateChanged;
    }

#ifdef PLATFORM_MOBILE
    if (m_fingerDown)
    {
        auto worldTarget = getWorldCoords();
        cro::Command cmd;
        cmd.targetFlags = CommandID::Player;
        cmd.action = [worldTarget](cro::Entity entity, cro::Time dt)
        {
            auto& tx = entity.getComponent<cro::Transform>();
            auto dist = worldTarget - tx.getWorldPosition();
            auto length = glm::length2(dist);
            if (length > 0 && length < 5) //only move if touch is within a 1 unit radius of player
            {
                //dist /= std::sqrt(length);
                entity.getComponent<Velocity>().velocity += dist;// *playerAcceleration;
                //entity.getComponent<cro::Transform>().move(dist);
            }
        };
        sendCommand(cmd);

        DPRINT("Touch", std::to_string(worldTarget.x) + ", " + std::to_string(worldTarget.y));
    }
#endif //PLATFORM_MOBILE

    //TODO is there a better way to broadcast player position?
    cro::Command cmd;
    cmd.targetFlags = CommandID::Player;
    cmd.action = [this](cro::Entity entity, cro::Time)
    {
        auto msg = postMessage<PlayerEvent>(MessageID::PlayerMessage);
        msg->type = PlayerEvent::Moved;
        msg->position = entity.getComponent<cro::Transform>().getWorldPosition();
    };
    sendCommand(cmd);
}

//private
glm::vec3 PlayerDirector::getWorldCoords()
{
    auto& camera = getScene().getActiveCamera().getComponent<cro::Camera>();
    
    //invert Y
    auto y = 1.f - m_touchCoords.y;

    //scale to vp
    auto vp = camera.viewport;
    y -= vp.bottom;
    y /= vp.height;

    //convert to NDC
    auto x = m_touchCoords.x;
    x *= 2.f; x -= 1.f;
    y *= 2.f; y -= 1.f;


    //depth needs to be player depth relative to near/far plane (-1, 1)
    //for now we kludge player depth at 9.25 with a farplane of 150
    float z = 9.25f / 150.f;
    z *= 2.f; z -= 1.f;

    //and unproject - remember view matrix if we decide to move the camera!!
    auto worldPos = glm::inverse(camera.projection) * glm::vec4(x, y, z, 1.f);
    glm::vec3 retVal(worldPos);
    retVal *= worldPos.w; //sure this is supposed to be division, but what the fudge.
    return retVal;
    //return { worldPos };
}