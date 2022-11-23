/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "../StateIDs.hpp"

#include <Social.hpp>

#include <crogine/core/State.hpp>

class EventOverlayState final : public cro::State
{
public:
    EventOverlayState(cro::StateStack& ss, cro::State::Context ctx)
        : cro::State(ss, ctx) {}

    bool handleEvent(const cro::Event&) override { return false; }
    void handleMessage(const cro::Message& msg) override
    {
        if (msg.id == Social::MessageID::SocialMessage)
        {
            const auto& data = msg.getData<Social::SocialEvent>();
            if (data.type == Social::SocialEvent::OverlayActivated
                && data.level == 0)
            {
                requestStackPop();
            }
        }
    }

    bool simulate(float) override { return true; }
    void render() override {}

    std::int32_t getStateID() const override { return StateID::EventOverlay; }

private:

};