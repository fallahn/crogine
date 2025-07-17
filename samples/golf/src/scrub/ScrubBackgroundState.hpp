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
#include "../golf/Path.hpp"
#include "../golf/CollisionMesh.hpp"
#include "../golf/Billboard.hpp"

#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>

#include <array>

struct SharedMinigameData;
class ScrubBackgroundState final : public cro::State, public cro::GuiClient
{
public:
    ScrubBackgroundState(cro::StateStack&, cro::State::Context, SharedMinigameData&);
    ~ScrubBackgroundState();

    ScrubBackgroundState(const ScrubBackgroundState&) = delete;
    ScrubBackgroundState(ScrubBackgroundState&&) = delete;

    ScrubBackgroundState& operator = (const ScrubBackgroundState&) = delete;
    ScrubBackgroundState& operator = (ScrubBackgroundState&&) = delete;

    cro::StateID getStateID() const override { return StateID::ScrubBackground; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    cro::Scene m_scene;
    cro::ResourceCollection m_resources;
    SharedMinigameData& m_sharedScrubData;

    cro::RenderTexture m_renderTexture;
    cro::SimpleQuad m_renderQuad;
    cro::RenderTexture m_blurTexture;
    cro::SimpleQuad m_blurQuad;

    struct BlurShader final
    {
        cro::Shader shader;
        std::uint32_t id = 0;
        std::int32_t uniform = -1;
    }m_blurShader;

    std::vector<cro::Entity> m_spectatorModels;
    std::vector<Path> m_paths;

    CollisionMesh m_collisionMesh;

    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    void addSystems();
    void loadAssets();
    void createScene();
    void loadSpectators();
    void loadClouds();
};
