/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include "StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Texture.hpp>

struct SharedStateData;
namespace cro
{
    struct EmitterSettings;
}

class ParticleState final : public cro::State, public cro::GuiClient
{
public:
    ParticleState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~ParticleState();

    cro::StateID getStateID() const override { return States::ParticleEditor; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    cro::Scene m_scene;
    cro::EnvironmentMap m_environmentMap;
    cro::ResourceCollection m_resources;

    float m_viewportRatio;
    float m_fov;
    ImVec4 m_messageColour;

    void loadAssets();
    void addSystems();
    void setupScene();

    void loadPrefs();
    void savePrefs();

    struct EntityID final
    {
        enum
        {
            ArcBall,
            Emitter,
            Model,

            Count
        };
    };

    std::array<cro::Entity, EntityID::Count> m_entities = {};
    cro::Entity m_selectedEntity;
    std::int32_t m_gizmoMode;

    cro::EmitterSettings* m_particleSettings;
    std::int32_t m_selectedBlendMode;
    cro::Texture m_texture;

    void initUI();
    void drawMenuBar();
    void drawInspector();
    void drawBrowser();
    void drawInfo();
    void drawGizmo();
    void updateLayout(std::int32_t, std::int32_t);
    void updateMouseInput(const cro::Event&);

    void openModel(const std::string&);
};