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

#pragma once

/*
Input parser. In charge of reading input events and applying
them to both the local player controller and sending to the server.
*/

#include "InputBinding.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Entity.hpp>

struct Input final
{
    std::int32_t timeStamp = 0;
    std::uint16_t buttonFlags = 0;
    std::uint16_t analogueMultiplier = 1;
};

namespace cro
{
    class NetClient;
}

class InputParser final
{
public:
    InputParser(cro::NetClient&, InputBinding);

    void handleEvent(const SDL_Event&);

    void update();

    void setEntity(cro::Entity);

    cro::Entity getEntity() const { return m_entity; }

    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    cro::NetClient& m_netClient;
    InputBinding m_inputBinding;  

    bool m_enabled;

    std::uint16_t m_inputFlags;
    cro::Entity m_entity;

    cro::Clock m_timestampClock;

    std::uint16_t m_prevStick;
    float m_analogueAmount;
    void checkControllerInput();
};
