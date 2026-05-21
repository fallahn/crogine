/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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
#include "Thumbsticks.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

#include <crogine/gui/GuiClient.hpp>

struct SharedStateData;

//TODO move this to own file if we use it elsewhere
struct TrackpadFinger final
{
    glm::vec2 prevPosition = glm::vec2(0.f);
    glm::vec2 currPosition = glm::vec2(0.f);
};

class MapOverviewState final : public cro::State, public cro::GuiClient
{
public:
    MapOverviewState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::MapOverview; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

    std::int32_t m_previousMap;

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

    cro::Entity m_mapCamera;
    cro::RenderTexture m_mapBuffer;

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    cro::Entity m_mapTitleText;

    cro::Entity m_controlIcon;
    cro::Entity m_controlText;

    float m_heatTarget;
    float m_heatAmount;

    float m_zoomScale;
    bool m_transitionActive;

    std::array<TrackpadFinger, 2u> m_trackpadFingers = {};
    std::int32_t m_fingerCount;

    Thumbsticks m_thumbsticks;

    cro::Shader m_ditherShader;
    std::int32_t m_ditherUniform;

    void addSystems();
    void loadAssets();
    void buildScene();
    void quitState();

    void recentreMap();
    void updateNormals();
    void onCachedPush() override;
    void onCachedPop() override;

    void zoomCamera();
    void panCamera(glm::vec2);

    float pixelsPerMetre() const;
    void gotoTarget();
};