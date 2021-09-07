/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "InputBinding.hpp"

#include <crogine/core/App.hpp>
#include <crogine/detail/glm/vec3.hpp>

namespace cro
{
    class MessageBus;
}

class InputParser final
{
public:
       
    InputParser(const InputBinding&, cro::MessageBus&);

    void handleEvent(const cro::Event&);
    void setHoleDirection(glm::vec3, bool);
    float getYaw() const;

    float getPower() const; //0-1 multiplied by selected club
    float getHook() const; //-1 to -1 * some angle, probably club defined

    std::int32_t getClub() const;

    void setActive(bool);
    void setSuspended(bool);
    void resetPower();
    void update(float);

    bool inProgress() const;

private:
    const InputBinding& m_inputBinding;
    cro::MessageBus& m_messageBus;

    std::uint16_t m_inputFlags;
    std::uint16_t m_prevFlags;
    std::uint16_t m_prevStick;
    float m_analogueAmount;

    std::int32_t m_mouseWheel;
    std::int32_t m_prevMouseWheel;
    std::int32_t m_mouseMove;
    std::int32_t m_prevMouseMove;


    float m_holeDirection; //radians
    float m_rotation; //+- max rads

    float m_power;
    float m_hook;
    float m_powerbarDirection;

    bool m_active;
    bool m_suspended;

    enum class State
    {
        Aim, Power, Stroke,
        Flight
    }m_state;

    std::int32_t m_currentClub;

    void setClub(float); //picks closest club to given distance
    void rotate(float);
    void checkControllerInput();
    void checkMouseInput();
};