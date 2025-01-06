#include "ScrubBackgroundState.hpp"

ScrubBackgroundState::ScrubBackgroundState(cro::StateStack& ss, cro::State::Context ctx)
    :cro::State(ss, ctx)
{
    ctx.mainWindow.loadResources([this]() 
        {
            //addSystems();
            //loadAssets();
            //createScene();
            //createUI();

            cacheState(States::ScratchPad::Scrub);
            cacheState(States::ScratchPad::ScrubAttract);
            cacheState(States::ScratchPad::ScrubPause);
        });

    requestStackPush(States::ScratchPad::ScrubAttract);
}

//public
bool ScrubBackgroundState::handleEvent(const cro::Event&)
{
    return false;
}

void ScrubBackgroundState::handleMessage(const cro::Message&)
{

}

bool ScrubBackgroundState::simulate(float)
{
    return false;
}

void ScrubBackgroundState::render()
{

}