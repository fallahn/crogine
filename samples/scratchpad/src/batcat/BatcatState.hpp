/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
http://trederia.blogspot.com

crogine test application - Zlib license.

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
#include "ResourceIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/audio/AudioBuffer.hpp>

#include <memory>

namespace cro
{
    class UISystem;
}

class BatcatState final : public cro::State, public cro::GuiClient
{
public:
    BatcatState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::BatCat; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_scene;
    cro::Scene m_overlayScene;

    cro::ResourceCollection m_resources;
    std::array<std::unique_ptr<cro::ModelDefinition>, GameModelID::Count> m_modelDefs;

    cro::AudioBuffer m_audioBuffer;

    cro::RenderTexture m_sceneTexture;
    cro::RenderTexture m_outputTexture;
    cro::SimpleQuad m_tempQuad;


    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void calcViewport(cro::Camera&);
    void updateView(cro::Camera&);
};