/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/core/State.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

struct SharedMinigameData;
class ScrubPauseState final : public cro::State
{
public:
    ScrubPauseState(cro::StateStack&, cro::State::Context, SharedMinigameData&);

    cro::StateID getStateID() const override { return StateID::ScrubPause; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedMinigameData& m_sharedScrubData;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    std::int32_t m_controllerIndex;

    void addSystems();
    void buildScene();
};
