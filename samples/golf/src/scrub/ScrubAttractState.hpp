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
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <array>

struct SharedStateData;
struct SharedScrubData;
class ScrubAttractState final : public cro::State
#ifdef CRO_DEBUG_
    , public cro::GuiClient
#endif
{
public:
    ScrubAttractState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedScrubData&);

    cro::StateID getStateID() const override { return StateID::ScrubAttract; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    SharedScrubData& m_sharedScrubData;
    cro::Scene m_uiScene;
    cro::Scene m_gameScene;
    cro::ResourceCollection m_resources;

    cro::EnvironmentMap m_environmentMap;
    cro::RenderTexture m_scrubTexture;

    cro::Entity m_music;
    cro::Entity m_body0;
    cro::Entity m_body1;

    cro::Entity m_highScoreText;

    struct TabID final
    {
        enum
        {
            Title, HowTo, Scores,
            Count
        };
    };
    std::array<cro::Entity, TabID::Count> m_tabs = {};
    std::size_t m_currentTab;
    cro::String m_keyboardHelpString;
    std::int32_t m_controllerIndex;

    void addSystems();
    void loadAssets();
    void buildScene();
    void buildScrubScene();

    void nextTab();
    void prevTab();

    void onCachedPush() override;
    void onCachedPop() override;
};
