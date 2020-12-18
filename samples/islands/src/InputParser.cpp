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
#include "ClientPacketData.hpp"
#include "PacketIDs.hpp"

#include <crogine/core/Console.hpp>
#include <crogine/network/NetClient.hpp>

InputParser::InputParser(cro::NetClient& nc, InputBinding binding)
    : m_netClient   (nc),
    m_inputFlags    (0),
    m_inputBinding  (binding)
{

}

//public
void InputParser::handleEvent(const SDL_Event& evt)
{
    //apply to input mask
    if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags |= InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags |= InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags |= InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags |= InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::CarryDrop])
        {
            m_inputFlags |= InputFlag::CarryDrop;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags |= InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::WeaponPrev])
        {
            m_inputFlags |= InputFlag::WeaponPrev;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::WeaponNext])
        {
            m_inputFlags |= InputFlag::WeaponNext;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Strafe])
        {
            m_inputFlags |= InputFlag::Strafe;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Up])
        {
            m_inputFlags &= ~InputFlag::Up;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags &= ~InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags &= ~InputFlag::Right;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Down])
        {
            m_inputFlags &= ~InputFlag::Down;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::CarryDrop])
        {
            m_inputFlags &= ~InputFlag::CarryDrop;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Action])
        {
            m_inputFlags &= ~InputFlag::Action;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::WeaponNext])
        {
            m_inputFlags &= ~InputFlag::WeaponNext;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::WeaponPrev])
        {
            m_inputFlags &= ~InputFlag::WeaponPrev;
        }
        else if (evt.key.keysym.sym == m_inputBinding.keys[InputBinding::Strafe])
        {
            m_inputFlags &= ~InputFlag::Strafe;
        }

    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == m_inputBinding.controllerID)
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags |= InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CarryDrop])
            {
                m_inputFlags |= InputFlag::CarryDrop;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::WeaponNext])
            {
                m_inputFlags |= InputFlag::WeaponNext;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::WeaponPrev])
            {
                m_inputFlags |= InputFlag::WeaponPrev;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Strafe])
            {
                m_inputFlags |= InputFlag::Strafe;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == m_inputBinding.controllerID)
        {
            if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Action])
            {
                m_inputFlags &= ~InputFlag::Action;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::CarryDrop])
            {
                m_inputFlags &= ~InputFlag::CarryDrop;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::WeaponNext])
            {
                m_inputFlags &= ~InputFlag::WeaponNext;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::WeaponPrev])
            {
                m_inputFlags &= ~InputFlag::WeaponPrev;
            }
            else if (evt.cbutton.button == m_inputBinding.buttons[InputBinding::Strafe])
            {
                m_inputFlags &= ~InputFlag::Strafe;
            }
        }
    }
}

void InputParser::update()
{
    if (m_entity.isValid())
    {
        auto& player = m_entity.getComponent<Player>();

        Input input;
        if (!player.waitResync)
        {
            input.buttonFlags = m_inputFlags;
        }
        input.timeStamp = m_timestampClock.elapsed().asMilliseconds();

        //apply to local entity
        player.inputStack[player.nextFreeInput] = input;
        player.nextFreeInput = (player.nextFreeInput + 1) % Player::HistorySize;
        cro::Console::printStat("Flags", std::to_string(m_inputFlags));

        //broadcast packet
        InputUpdate update;
        update.input = input;
        update.playerID = player.id;
        update.connectionID = player.connectionID;

        m_netClient.sendPacket(PacketID::InputUpdate, update, cro::NetFlag::Unreliable);
    }
}

void InputParser::setEntity(cro::Entity e)
{
    CRO_ASSERT(e.hasComponent<Player>(), "Player component is missing");
    m_entity = e;
}

