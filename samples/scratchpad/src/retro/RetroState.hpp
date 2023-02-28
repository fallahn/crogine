/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

namespace cro
{
    struct Camera;
}

class RetroState final : public cro::State, public cro::GuiClient
{
public:
    RetroState(cro::StateStack&, cro::State::Context);
    ~RetroState() = default;

    cro::StateID getStateID() const override { return States::ScratchPad::Retro; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::ResourceCollection m_resources;
    cro::Texture m_paletteTexture;
    cro::RenderTexture m_renderTexture;
    cro::SimpleQuad m_quad;

    cro::Entity m_lightSource;
    cro::Entity m_lightRoot;
    struct LensFlare final
    {
        glm::vec3 srcPos = glm::vec3(0.f);
        glm::vec3 testPos = glm::vec3(0.f);
        glm::vec2 screenPos = glm::vec2(0.f);
        bool visible = false;
    }m_lensFlare;
   void updateLensFlare();

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    //assigned to camera resize callback
    void updateView(cro::Camera&);
};