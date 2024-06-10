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
#include "League.hpp"
#include "CommonConsts.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/gui/GuiClient.hpp>

struct SharedStateData;
struct SharedProfileData;
class LeagueState final : public cro::State, public cro::ConsoleClient, public cro::GuiClient
{
public:
    LeagueState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedProfileData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::League; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;
    SharedProfileData& m_profileData;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back, No,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;

    struct TabID final
    {
        enum
        {
            League, Info,

            Count
        };
        static constexpr std::int32_t Max = 2;
    };
    std::int32_t m_currentTab;
    std::int32_t m_currentLeague;
    cro::Entity m_tabEntity;
    std::array<cro::Entity, TabID::Count> m_tabButtons = {};   
    std::array<cro::Entity, TabID::Count> m_tabNodes = {};   

    struct LeagueID final
    {
        enum
        {
            Club,
            S01,
            S02,
            S03,
            S04,
            S05,
            S06,
            Global,

            Count
        };
    };
    std::array<cro::Entity, LeagueID::Count> m_leagueNodes = {};

    void buildScene();
    bool createLeagueTab(cro::Entity, const cro::SpriteSheet&, std::int32_t);
    void createInfoTab(cro::Entity);

    std::array<cro::Entity, LeagueID::Count> m_nameLists = {};
    std::array<cro::Entity, LeagueID::Count> m_nameScrollers = {};
    void refreshNameList(std::int32_t leagueID, const League&);
    void refreshAllNameLists();

    bool m_editName;
    cro::String* m_activeName;
    std::array<char, ConstVal::MaxStringChars * 2>  m_nameBuffer = {}; //hmmm this has to allow for multi-byte chars


#ifdef USE_GNS

    struct LeagueText final
    {
        cro::Entity games;
        cro::Entity names;
        cro::Entity scores;
        cro::Entity personal;
        cro::Entity previous;
    }m_leagueText;

    void createGlobalLeagueTab(cro::Entity,const cro::SpriteSheet&);
    void updateLeagueText();
#endif
    void addLeagueButtons(const cro::SpriteSheet&);

    void activateTab(std::int32_t);

    struct Page final
    {
        enum
        {
            Forward, Back
        };
    };
    void switchLeague(std::int32_t);
    void quitState();
};