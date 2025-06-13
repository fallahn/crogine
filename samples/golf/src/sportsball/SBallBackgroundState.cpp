/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "../scrub/ScrubSharedData.hpp"
#include "SBallBackgroundState.hpp"

#include <Social.hpp>

SBallBackgroundState::SBallBackgroundState(cro::StateStack& stack, cro::State::Context ctx, SharedMinigameData& sd)
    : cro::State        (stack, ctx),
    m_sharedGameData    (sd)
{
    ctx.mainWindow.loadResources([&]()
        {
            sd.initFonts();

            cacheState(StateID::SBallGame);
            cacheState(StateID::SBallAttract);
            cacheState(StateID::ScrubPause);
        });
#ifdef CRO_DEBUG_
    //requestStackPush(StateID::SBallGame);
    requestStackPush(StateID::SBallAttract);
#else
    requestStackPush(StateID::SBallAttract);
#endif

    Social::refreshSBallScore();
    Social::setStatus(Social::InfoID::Menu, { "Playing Sports Ball" });
}

SBallBackgroundState::~SBallBackgroundState()
{
    m_sharedGameData.backgroundTexture = nullptr;

    //reset any controllers here in case we're just quitting the game
    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, {});
    }
}

//public
bool SBallBackgroundState::handleEvent(const cro::Event&)
{
    return false;
}

void SBallBackgroundState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && data.id == StateID::SBallGame)
        {
            requestStackPush(StateID::SBallAttract);
        }
    }
}

bool SBallBackgroundState::simulate(float)
{
    return false;
}

void SBallBackgroundState::render()
{

}