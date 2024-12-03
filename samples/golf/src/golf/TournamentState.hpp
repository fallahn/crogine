/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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
#include "Career.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <functional>

namespace cro
{
    class SpriteSheet;
}

struct SharedStateData;

class TournamentState final : public cro::State
{
public:
    TournamentState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~TournamentState();

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Tournament; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

    std::size_t m_maxLeagueIndex;
    std::array<std::uint64_t, Career::MaxLeagues> m_progressPositions = {};

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    cro::Entity m_playerName;
    cro::RenderTexture m_clubTexture;

    struct LeagueDetails final
    {
        cro::Entity courseTitle;
        cro::Entity courseDescription;
        cro::Entity holeCount;
        cro::Entity leagueDetails; //round number, current position and previous best
        cro::Entity thumbnail;
        cro::Entity highlight;
    }m_leagueDetails;

    struct SettingsDetails final
    {
        cro::Entity gimme;
        cro::Entity night;
        cro::Entity clubset;
    }m_settingsDetails;

    std::size_t m_currentMenu;
    std::function<void()> enterConfirmCallback;
    std::function<void()> quitConfirmCallback;

    std::function<void()> enterInfoCallback;
    std::function<void()> quitInfoCallback;

    void addSystems();
    void buildScene();
    void createConfirmMenu(cro::Entity);
    void createInfoMenu(cro::Entity);
    void createProfileLayout(cro::Entity bgEnt, const cro::SpriteSheet&);

    void selectLeague(std::size_t);
    void applySettingsValues();

    void quitState();

    void loadConfig();
    void saveConfig() const;
};