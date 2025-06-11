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

#include "SBallAttractState.hpp"

SBallAttractState::SBallAttractState(cro::StateStack& ss, cro::State::Context ctx, SharedMinigameData& sd)
    : cro::State    (ss, ctx),
    m_sharedGameData(sd),
    m_scene         (cro::App::getInstance().getMessageBus(), 512)
{
    loadAssets();
    addSystems();
    buildScene();
}

SBallAttractState::~SBallAttractState()
{
    LogI << "Find out why we have a dtor for attract state" << std::endl;
}

//public
bool SBallAttractState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_SPACE)
        {
            requestStackPop();
            requestStackPush(StateID::SBallGame);
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void SBallAttractState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool SBallAttractState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void SBallAttractState::render()
{
    m_scene.render();
}

//private
void SBallAttractState::loadAssets()
{

}

void SBallAttractState::addSystems()
{

}

void SBallAttractState::buildScene()
{

}