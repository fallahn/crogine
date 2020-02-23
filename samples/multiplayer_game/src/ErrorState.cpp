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

#include "ErrorState.hpp"
#include "SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Window.hpp>

ErrorState::ErrorState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx)
{
    ctx.mainWindow.setMouseCaptured(false);

    auto msg = sd.errorMessage;
    registerWindow([&, msg, ctx]() 
        {
            auto windowSize = ctx.mainWindow.getSize();
            ImGui::SetNextWindowSize({ 200.f, 120.f });
            ImGui::SetNextWindowPos({ (windowSize.x - 200.f) / 2.f, (windowSize.y - 120.f) / 2.f });
            if (ImGui::Begin("Error"))
            {
                ImGui::Text("%s", msg.c_str());
                if (ImGui::Button("OK"))
                {
                    sd.serverInstance.stop();
                    sd.clientConnection.connected = false;
                    sd.clientConnection.netClient.disconnect();
                    sd.clientConnection.playerID = 4;
                    sd.clientConnection.ready = false;

                    requestStackClear();
                    requestStackPush(States::MainMenu);
                }
            }
            ImGui::End();
        
        });
}

//public
bool ErrorState::handleEvent(const cro::Event&)
{
    return false;
}

void ErrorState::handleMessage(const cro::Message&)
{

}

bool ErrorState::simulate(float)
{
    return false;
}

void ErrorState::render()
{

}