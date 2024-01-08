/*-----------------------------------------------------------------------

Matt Marchant 2023
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
#include "MenuConsts.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/gui/GuiClient.hpp>

struct SharedStateData;

class LeaderboardState final : public cro::State, public cro::GuiClient
{
public:
    LeaderboardState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Leaderboard; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;
    cro::ResourceCollection m_resources;

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

    struct BoardIndex final
    {
        enum
        {
            Course,
            Hio, Rank, Streak,

            Count
        };
    };
    struct DynamicButton final
    {
        enum
        {
            Close,
            Course,
            HIO,
            Rank,
            Streak,
            Date,

            Count
        };
    };
    std::array<cro::Entity, DynamicButton::Count> m_buttons = {};

    struct DisplayContext final
    {
        cro::Entity courseTitle;
        cro::Entity leaderboardText;
        cro::Entity leaderboardScores;
        cro::Entity personalBest;
        cro::Entity thumbnail;
        cro::Entity holeText;
        cro::Entity monthText;

        std::size_t courseIndex = 0;
        std::int32_t page = 0;
        std::int32_t holeCount = 0;
        bool showNearest = false;

        std::int32_t boardIndex = BoardIndex::Course;
    }m_displayContext;

    std::vector<std::pair<std::string, cro::String>> m_courseStrings;
    std::vector<const cro::Texture*> m_courseThumbs;

    FlyoutMenu m_flyout;

    void parseCourseDirectory();
    void buildScene();
    void createFlyout(cro::Entity);

    void refreshDisplay();
    void updateButtonIndices();
    void quitState();
};