/*-----------------------------------------------------------------------

Matt Marchant 2021
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
#include "HoleData.hpp"
#include "GolfInputParser.hpp"
#include "TerrainBuilder.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Image.hpp>

#include <array>

static inline glm::vec2 calcVPSize()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    const float ratio = size.x / size.y;
    static constexpr float Widescreen = 16.f / 9.f;
    static constexpr float ViewportWidth = 640.f;

    return { ViewportWidth, ratio < Widescreen ? 300.f : 360.f };
}

namespace cro
{
    struct NetEvent;
}

class GolfState final : public cro::State, public cro::GuiClient
{
public:
    GolfState(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return States::Golf::Game; }

private:
    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    golf::InputParser m_inputParser;

    bool m_wantsGameState;
    cro::Clock m_readyClock; //pings ready state until ack'd

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_renderTexture;

    cro::Image m_currentMap;
    std::vector<HoleData> m_holeData;
    std::uint32_t m_currentHole;
    ActivePlayer m_currentPlayer;

    TerrainBuilder m_terrainBuilder;

    struct MaterialID final
    {
        enum
        {
            WireFrame,
            Water,
            Cel,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    struct ModelID final
    {
        enum
        {
            Ball,
            BallShadow,

            Count
        };
    };
    std::array<std::unique_ptr<cro::ModelDefinition>, ModelID::Count> m_modelDefs = {};


    struct WaterShader final
    {
        std::uint32_t shaderID = 0;
        std::int32_t timeUniform = 0;
    }m_waterShader;

    struct BallResource final
    {
        std::int32_t materialID = -1;
        std::size_t ballMeshID = 0;
        std::size_t shadowMeshID = 0;
    }m_ballResources;

    void loadAssets();
    void addSystems();
    void buildScene();

    void spawnBall(const struct ActorInfo&);

    void handleNetEvent(const cro::NetEvent&);
    void removeClient(std::uint8_t);

    void setCurrentHole(std::uint32_t);
    void setCameraPosition(glm::vec3, float, float);
    void setCurrentPlayer(const ActivePlayer&);
    void hitBall();
    void updateActor(const ActorInfo&);

    void createTransition(const ActivePlayer&);
    void updateWindDisplay(glm::vec3);
    std::int32_t getClub() const;


    //UI stuffs - found in GolfStateUI.cpp
    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBarInner,
            HookBar,
            WindIndicator,
            Player01,
            Player02,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    struct FontID final
    {
        enum
        {
            UI,

            Count
        };
    };
    float m_camRotation; //used to offset the rotation of the wind indicator
    bool m_roundEnded;
    glm::vec2 m_viewScale;

    void buildUI();
    void showCountdown(std::uint8_t);
    void createScoreboard();
    void updateScoreboard();
    void showScoreboard(bool);
    //-----------

#ifdef CRO_DEBUG_
    cro::Entity m_debugCam;
    cro::RenderTexture m_debugTexture;

    void setupDebug();
#endif
};