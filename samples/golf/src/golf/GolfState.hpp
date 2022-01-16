/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "InputParser.hpp"
#include "TerrainBuilder.hpp"
#include "CameraFollowSystem.hpp"
#include "CollisionMesh.hpp"
#include "LeaderboardTexture.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <array>
#include <unordered_map>

namespace cro
{
    struct NetEvent;
}

//sprite which carries green overhead view
struct GreenCallbackData final
{
    float currTime = 0.f;
    float state = 0;
    float targetScale = 1.f;
};

//callback data for green overhead view
struct MiniCamData final
{
    static constexpr float MaxSize = 2.25f;
    static constexpr float MinSize = 0.75f;
    float targetSize = MaxSize;
    float currentSize = MinSize;
};

//used in player sprite animation
struct PlayerCallbackData final
{
    std::int32_t direction = 0; //grow or shrink
    float scale = 0.f;
    float reflectionOffset = 0.f;
};

class GolfState final : public cro::State, public cro::GuiClient
{
public:
    GolfState(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Game; }

private:
    cro::ResourceCollection m_resources;    
    
    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;

    cro::Clock m_mouseClock;
    bool m_mouseVisible;

    InputParser m_inputParser;

    bool m_wantsGameState;
    cro::Clock m_readyClock; //pings ready state until ack'd

    cro::RenderTexture m_gameSceneTexture;
    std::vector<std::pair<std::int32_t, std::int32_t>> m_scaleUniforms;

    cro::Image m_currentMap;
    std::vector<HoleData> m_holeData;
    std::uint32_t m_currentHole;
    ActivePlayer m_currentPlayer;
    CollisionMesh m_collisionMesh;

    TerrainBuilder m_terrainBuilder;

    struct MaterialID final
    {
        enum
        {
            WireFrame,
            WireFrameCulled,
            Water,
            Cel,
            CelSkinned,
            CelTextured,
            CelTexturedSkinned,
            Player,
            Course,
            Ball,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    struct ModelID final
    {
        enum
        {
            BallShadow,
            PlayerShadow,

            Count
        };
    };
    std::array<std::unique_ptr<cro::ModelDefinition>, ModelID::Count> m_modelDefs = {};
    std::unordered_map<std::int32_t, std::unique_ptr<cro::ModelDefinition>> m_ballModels;

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

    std::string m_audioPath;

    void loadAssets();
    void addSystems();
    void buildScene();
    void initAudio();

    void createWeather();
    void spawnBall(const struct ActorInfo&);

    void handleNetEvent(const cro::NetEvent&);
    void removeClient(std::uint8_t);

    void setCurrentHole(std::uint32_t);
    void setCameraPosition(glm::vec3, float, float);
    void requestNextPlayer(const ActivePlayer&);
    void setCurrentPlayer(const ActivePlayer&);
    void hitBall();
    void updateActor(const ActorInfo&);

    void createTransition(const ActivePlayer&);
    void startFlyBy();
    std::int32_t getClub() const;

    //allows switching camera, TV style
    std::array<cro::Entity, CameraID::Count> m_cameras = {};
    std::int32_t m_currentCamera;
    void setActiveCamera(std::int32_t);
    void setPlayerPosition(cro::Entity, glm::vec3);

    cro::Entity m_defaultCam;
    cro::Entity m_freeCam;
    void toggleFreeCam();

    //UI stuffs - found in GolfStateUI.cpp
    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBarInner,
            HookBar,
            WindIndicator,
            WindSpeed,
            MessageBoard,
            Bunker,
            Foul,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    struct Avatar final
    {
        struct Sprite final
        {
            cro::Sprite sprite;
            std::array<std::size_t, AnimationID::Count> animIDs = {};

            enum
            {
                Wood, Iron, Count
            };
        };
        std::int32_t clubType = Sprite::Wood;
        std::array<Sprite, Sprite::Count> sprites = {};
        bool flipped = false;
    };
    std::array<std::array<Avatar, ConnectionData::MaxPlayers>, ConstVal::MaxClients> m_avatars;


    float m_camRotation; //used to offset the rotation of the wind indicator
    bool m_roundEnded;
    glm::vec2 m_viewScale;
    std::size_t m_scoreColumnCount;
    LeaderboardTexture m_leaderboardTexture;

    cro::Entity m_courseEnt;
    cro::Entity m_waterEnt;
    cro::Entity m_uiReflectionCam;

    void buildUI();
    void showCountdown(std::uint8_t);
    void createScoreboard();
    void updateScoreboard();
    void showScoreboard(bool);
    void updateWindDisplay(glm::vec3);

    enum class MessageBoardID
    {
        Bunker, Scrub, Water,
        PlayerName, HoleScore
    };
    void showMessageBoard(MessageBoardID);
    void floatingMessage(const std::string&);
    void createTransition();

    //-----------

    cro::Entity m_mapCam;
    cro::RenderTexture m_mapBuffer;
    cro::RenderTexture m_mapTexture;
    cro::SimpleQuad m_mapQuad;
    cro::SimpleQuad m_flagQuad;
    void updateMiniMap();

    cro::Entity m_greenCam;
    cro::RenderTexture m_greenBuffer;
};