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

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

namespace els
{
    struct SharedStateData;
}

struct SharedStateData;
class EndlessAttractState final : public cro::State
{
public:
    EndlessAttractState(cro::StateStack&, cro::State::Context, SharedStateData&, els::SharedStateData&);

    cro::StateID getStateID() const override { return StateID::EndlessAttract; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    els::SharedStateData& m_sharedGameData;

    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::Entity m_rootNode;
    
    struct PromptID final
    {
        enum
        {
            Keyboard, Xbox, PS,
            Count
        };
    };
    std::array<cro::Entity, PromptID::Count> m_startPrompts = {};
    std::array<cro::Entity, PromptID::Count> m_quitPrompts = {};

    std::size_t m_cycleIndex;
    cro::Clock m_cycleClock;

    struct CycleEnt final
    {
        enum
        {
            GameOver, HighScore, Rules,

            Count
        };
    };
    std::array<cro::Entity, CycleEnt::Count> m_cycleEnts = {};
    std::array<cro::Entity, 3u> m_highScoreEnts = {};
    cro::Entity m_gameOverScore;

    void addSystems();
    void loadAssets();
    void createUI();

    void refreshHighScores();
    void refreshCycle(std::size_t);
    void refreshPrompt();
};