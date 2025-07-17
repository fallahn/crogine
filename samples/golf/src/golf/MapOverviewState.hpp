/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

struct SharedStateData;

//TODO move this to own file if we use it elsewhere
struct TrackpadFinger final
{
    glm::vec2 prevPosition = glm::vec2(0.f);
    glm::vec2 currPosition = glm::vec2(0.f);
};

class MapOverviewState final : public cro::State
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

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    cro::Entity m_mapEnt;
    cro::Entity m_mapText;
    cro::Entity m_mapNormals;
    cro::Entity m_ballLandingArea;

    cro::Entity m_controlIcon;
    cro::Entity m_controlText;

    cro::RenderTexture m_renderBuffer;
    cro::SimpleQuad m_mapQuad;
    cro::SimpleText m_mapString;

    cro::Shader m_mapShader;
    cro::Shader m_slopeShader;

    struct ShaderUniforms final
    {
        std::int32_t posMap = -1;
        std::int32_t maskMap = -1;
        std::int32_t normalMap = -1;
        std::int32_t transparency = -1;
        std::int32_t gridAmount = -1;
        std::int32_t gridScale = -1;
        std::int32_t heatAmount = -1;
    }m_shaderUniforms;

    static constexpr std::array<std::pair<float, float>, 2u> m_shaderValues =
    {
        std::make_pair<float, float>(0.f, 0.f),
        std::make_pair<float, float>(0.f, 1.f),
        //std::make_pair<float, float>(1.f, 0.f)
    };
    std::size_t m_shaderValueIndex;

    float m_zoomScale;
    bool m_transitionActive;

    std::array<TrackpadFinger, 2u> m_trackpadFingers = {};
    std::int32_t m_fingerCount;

    Thumbsticks m_thumbsticks;

    void addSystems();
    void loadAssets();
    void buildScene();
    void quitState();

    void recentreMap();
    void rescaleMap();
    void refreshMap();
    void updateNormals();
    void onCachedPush() override;

    void pan(glm::vec2);
    glm::vec2 toMapCoords(glm::vec3);
    void gotoTarget();
};