/*-----------------------------------------------------------------------

Matt Marchant 2023
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
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>

#include <crogine/gui/GuiClient.hpp>

namespace cro
{
    struct Camera;
}

class SSAOState final : public cro::State, public cro::GuiClient
{
public:
    SSAOState(cro::StateStack&, cro::State::Context);
    ~SSAOState() = default;

    cro::StateID getStateID() const override { return States::ScratchPad::SSAO; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    cro::MultiRenderTexture m_renderBuffer;
    cro::SimpleQuad m_colourQuad;
    cro::SimpleQuad m_normalQuad;
    cro::SimpleQuad m_positionQuad;
    cro::SimpleQuad m_depthQuad;
    cro::SimpleQuad m_lightQuad;

    cro::SimpleText m_textA;
    cro::SimpleText m_textB;

    cro::RenderTexture m_ssaoBuffer;
    cro::RenderTexture m_ssaoBlurBuffer;
    cro::SimpleQuad m_ssaoQuad;

    struct OutputData final
    {
        std::uint32_t shader = 0;
        std::int32_t aoMap = -1;
        std::int32_t lightMap = -1;
    }m_outputData;
    cro::SimpleQuad m_outputQuad;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    std::vector<glm::vec3> m_sampleKernel;
    cro::Texture m_noiseTexture;
    struct SSAOData final
    {
        std::uint32_t shader = 0;
        std::int32_t normal = -1;
        std::int32_t noise = -1;
        std::int32_t kernel = -1;
        std::int32_t projectionMatrix = -1;
    }m_ssaoData;
    cro::SimpleQuad m_ssaoBufferQuad;
    cro::SimpleQuad m_ssaoBlurQuad;
    void generateSSAOData();
    void updateSSAOData();
};