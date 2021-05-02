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

#include "StateIDs.hpp"
#include "ResourceIDs.hpp"
#include "InputParser.hpp"
#include "ServerPacketData.hpp"
#include "GameConsts.hpp"
#include "ActorIDs.hpp"
#include "UIDirector.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/network/NetData.hpp>

#include <unordered_map>
#include <array>

namespace cro
{
    struct Camera;
}

struct SharedStateData;
class GameState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    GameState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~GameState() = default;

    cro::StateID getStateID() const override { return States::Game; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    std::uint32_t m_requestFlags;
    std::uint32_t m_dataRequestCount;

    cro::ResourceCollection m_resources;
    std::array<std::size_t, MeshID::Count> m_meshIDs = {};
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    std::array<cro::ModelDefinition, GameModelID::Count> m_modelDefs = {};
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    cro::EnvironmentMap m_environmentMap;
    cro::EnvironmentMap m_skyMap;

    std::unordered_map<std::uint8_t, InputParser> m_inputParsers;
    std::vector<cro::Entity> m_cameras;

    cro::Clock m_bitrateClock; //< updates the bitrate display in the debug window
    cro::Clock m_sceneRequestClock; //< spaces the request for initial scene data


    cro::Entity m_splitScreenNode; //< draws the lines between screen via the UI scene
    std::array<cro::Entity, 4u> m_avatars = {};
    std::array<cro::Entity, 4u> m_spawnerEntities = {};

#ifdef CRO_DEBUG_
    cro::RenderTexture m_debugViewTexture;
    cro::Entity m_debugCam;
#endif


    void addSystems();
    void loadAssets();
    void createScene();
    void createDayCycle();
    void loadMap();

    void handlePacket(const cro::NetEvent::Packet&);
    void spawnPlayer(PlayerInfo);
    void removePlayer(std::uint8_t);
    void spawnActor(ActorSpawn);
    void updateActor(ActorUpdate);
    void updateIdleActor(ActorIdleUpdate);
    void updateView(cro::Camera&);

    void startGame();

    void crateUpdate(const CrateState&);
    void snailUpdate(const SnailState&);
    void avatarUpdate(const PlayerStateChange&);
    void removeEntity(ActorRemoved);

    std::array<PlayerUI, 4u> m_playerUIs = {};

    void createUI();
    void updatePlayerUI();
    void updateRoundStats(const RoundStats&);
};