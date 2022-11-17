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

#pragma once

#include <crogine/detail/Types.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/gui/GuiClient.hpp>

class InputHandler final : public cro::GuiClient
{
public:
    InputHandler();

    void handleEvent(const cro::Event&);

    void process(float);


    glm::vec2 getBackPoint() const { return m_backPoint; }
    glm::vec2 getFrontPoint() const { return m_frontPoint; }

private:

    glm::vec2 m_backPoint;
    glm::vec2 m_frontPoint;

    struct State final
    {
        enum
        {
            Inactive,
            Draw,
            Push,
            Summarise,

            Count
        };
    };    
    std::int32_t m_state = State::Inactive;
    const std::array<std::string, State::Count> StateStrings =
    {
        "Inactive", "Draw", "Push", "Summarise"
    };

    cro::Clock m_timer;
    cro::Time m_elapsedTime;
};