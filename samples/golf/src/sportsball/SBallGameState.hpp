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

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/gui/GuiClient.hpp>

struct SharedMinigameData;
struct SharedStateData;
class SBallGameState final : public cro::State, public cro::GuiClient
{
public:

    SBallGameState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedMinigameData&);

    cro::StateID getStateID() const override { return StateID::SBallGame; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    SharedMinigameData& m_sharedGameData;

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    std::vector<cro::Entity> m_previewModels;
    std::vector<cro::Entity> m_nextModels;

    std::int32_t m_currentID;
    std::int32_t m_nextID;

    cro::Entity m_wheelEnt;
    cro::Entity m_cursor;
    std::uint16_t m_inputFlags;
    float m_inputMultiplier; //speed ramp for finer input adjustment

    void loadAssets();
    void addSystems();
    void buildScene();
    void buildUI();

    cro::Clock m_dropClock;
    void dropBall();

    cro::Entity m_scoreEntity;
    cro::Entity m_roundEndEntity;
    void floatingScore(std::int32_t score, glm::vec3 pos);

    bool m_gameEnded;
    cro::Clock m_roundEndClock;
    void endGame();

    void onCachedPush() override;
    void onCachedPop() override;
};
