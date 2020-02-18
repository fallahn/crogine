/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "InputParser.hpp"
#include "PlayerSystem.hpp"

#include <crogine/core/Console.hpp>

InputParser::InputParser()
    : m_inputFlags  (0),
    m_mouseMoveX    (0),
    m_mouseMoveY    (0)
{

}

//public
void InputParser::handleEvent(const SDL_Event& evt)
{
    //TODO worry about keybinds at some point
    switch (evt.type)
    {
    default: break;
    case SDL_KEYDOWN:
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_w:
            m_inputFlags |= Input::Forward;
            break;
        case SDLK_s:
            m_inputFlags |= Input::Backward;
            break;
        case SDLK_a:
            m_inputFlags |= Input::Left;
            break;
        case SDLK_d:
            m_inputFlags |= Input::Right;
            break;
        }
        break;
    case SDL_KEYUP:
        switch (evt.key.keysym.sym)
        {
        default:break;
        case SDLK_w:
            m_inputFlags &= ~Input::Forward;
            break;
        case SDLK_s:
            m_inputFlags &= ~Input::Backward;
            break;
        case SDLK_a:
            m_inputFlags &= ~Input::Left;
            break;
        case SDLK_d:
            m_inputFlags &= ~Input::Right;
            break;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        switch (evt.button.button)
        {
        default: break;
        case 0:
            m_inputFlags |= Input::LeftMouse;
            break;
        case 1:
            m_inputFlags |= Input::RightMouse;
            break;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        switch (evt.button.button)
        {
        default: break;
        case 0:
            m_inputFlags &= ~Input::LeftMouse;
            break;
        case 1:
            m_inputFlags &= ~Input::RightMouse;
            break;
        }
        break;
    case SDL_MOUSEMOTION:
        if (evt.motion.xrel != 0)
        {
            m_mouseMoveX = evt.motion.xrel;
        }
        if (evt.motion.yrel != 0)
        {
            m_mouseMoveY = evt.motion.yrel;
        }
        break;
    }

}

void InputParser::update()
{
    if (m_entity.isValid())
    {
        Input input;
        input.buttonFlags = m_inputFlags;
        input.xMove = static_cast<std::uint8_t>(m_mouseMoveX);
        input.yMove = static_cast<std::uint8_t>(m_mouseMoveY);
        input.timeStamp = m_timestampClock.elapsed().asMilliseconds();

        //apply to local entity
        auto& player = m_entity.getComponent<Player>();
        player.inputStack[player.nextFreeInput] = input;
        player.nextFreeInput = (player.nextFreeInput + 1) % Player::HistorySize;


        //TODO broadcast packet
        
    }
    m_mouseMoveX = m_mouseMoveY = 0;
}

void InputParser::setEntity(cro::Entity e)
{
    CRO_ASSERT(e.hasComponent<Player>(), "Player component is missing");
    m_entity = e;
}

