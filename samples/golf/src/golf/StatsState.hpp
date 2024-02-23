/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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
#include "../sqlite/ProfileDB.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

struct SharedStateData;
namespace cro
{
    class SpriteSheet;
}

class PieChart final
{
public:
    PieChart();

    void reset(); //removes all values
    void addValue(float); 

    void updateVerts(); //recalcs the verts based on current values
    void setEntity(cro::Entity); //entity with drawable to update

    float getPercentage(std::uint32_t) const;
    float getTotal() const { return m_total; }
    float getValue(std::uint32_t i) { return i < m_values.size() ? m_values[i] : 0.f; }

private:
    float m_total;
    std::vector<float> m_values;
    cro::Entity m_entity;
};

class BarChart final
{
public:
    BarChart();

    void addBar(float);
    void updateVerts();
    void setEntity(cro::Entity);
    float getMaxValue() const { return m_maxValue; }

private:
    float m_maxValue;
    std::vector<float> m_values;
    cro::Entity m_entity;
    static constexpr float MaxHeight = 36.f;
};

class StatsState final : public cro::State, public cro::ConsoleClient
{
public:
    StatsState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Stats; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

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

    struct TabID final
    {
        enum
        {
            ClubStats, Performance, History, Awards,

            Count
        };
        static constexpr std::int32_t Max = 4;
    };
    std::int32_t m_currentTab;
    cro::Entity m_tabEntity;
    std::array<cro::Entity, TabID::Count> m_tabButtons = {};   
    std::array<cro::Entity, TabID::Count> m_tabNodes = {};
    cro::Entity m_closeButton;

    bool m_imperialMeasurements;
    std::array<PieChart, 2u> m_pieCharts = {};
    BarChart m_playTimeChart;
    std::vector<std::pair<std::string, cro::String>> m_courseStrings;

    struct ProfileData final
    {
        cro::String name;
        std::string dbPath;
    };
    std::vector<ProfileData> m_profileData;
    std::size_t m_profileIndex;
    std::size_t m_courseIndex;

    struct DateRange final
    {
        enum {Week, Month, Quarter, Half, Year, Count};
    };
    std::int32_t m_dateRange;
    bool m_showCPUStat;

    std::array<cro::Entity, 18u> m_graphEntities = {};
    cro::Entity m_gridEntity;
    cro::Entity m_recordCountEntity;
    bool m_holeDetailSelected;
    std::array<cro::Entity, 18u> m_holeDetailEntities = {};

    struct HoleDetail final
    {
        cro::Entity background;
        cro::Entity text;
        static constexpr glm::vec2 Bottom = glm::vec2(27.f, 18.f);
        static constexpr glm::vec2 Top = glm::vec2(27.f, 126.f);
    }m_holeDetail;

    ProfileDB m_profileDB;

    cro::RenderTexture m_awardsTexture;
    cro::SimpleQuad m_awardQuad;
    cro::SimpleText m_awardText;

    std::int32_t m_awardPageIndex;
    std::int32_t m_awardPageCount;

    struct AwardNavigation final
    {
        cro::Entity label;
        cro::Entity buttonLeft;
        cro::Entity buttonRight;
    }m_awardNavigation;

    struct SpriteID final
    {
        enum
        {
            BronzeShield,
            SilverShield,
            GoldShield,
            BronzeSpike,
            SilverSpike,
            GoldSpike,
            TeeBall,

            BronzeBall,
            SilverBall,
            GoldBall,
            PlatinumBall,
            DiamondBall,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_awardSprites = {};


    void buildScene();
    void parseCourseData();
    void parseProfileData();
    void createClubStatsTab(cro::Entity, const cro::SpriteSheet&);
    void createPerformanceTab(cro::Entity, const cro::SpriteSheet&);
    void createHistoryTab(cro::Entity);
    void createAwardsTab(cro::Entity, const cro::SpriteSheet&);
    void refreshAwardsTab(std::int32_t page);
    void activateTab(std::int32_t);
    void refreshPerformanceTab(bool newProfile);
    void quitState();
};