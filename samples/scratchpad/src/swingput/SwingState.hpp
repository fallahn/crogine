/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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
#include "Swingput.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/RenderTexture.hpp>

namespace cro
{
    struct Camera;
}

class SwingState final : public cro::State, public cro::GuiClient
{
public:
    SwingState(cro::StateStack&, cro::State::Context);
    ~SwingState();

    cro::StateID getStateID() const override { return States::ScratchPad::Swing; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::ResourceCollection m_resources;
    cro::CubemapTexture m_cubemap;
    cro::CubemapTexture m_cubemapArray;

    Swingput m_inputParser;

    cro::SimpleVertexArray m_target;
    cro::SimpleVertexArray m_follower;

    cro::SimpleQuad m_rainQuad;

    cro::RenderTexture m_ballTexture;
    cro::SimpleQuad m_ballQuad;
    cro::Entity m_ballCam;

    struct RainShder final
    {
        std::uint32_t shaderID = 0;
        std::uint32_t textureID = 0;
        std::int32_t rainUniform = -1;
        std::int32_t rectUniform = -1;
    }m_rainShader;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void loadSettings();
    void saveSettings();

    //assigned to camera resize callback
    void updateView(cro::Camera&);
};