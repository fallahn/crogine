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
#include "CPUGolfer.hpp"
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
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/graphics/CubemapTexture.hpp>

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

    cro::StateID getStateID() const override { return StateID::Golf; }

private:
    cro::ResourceCollection m_resources;    
    
    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_skyScene;
    cro::Scene m_uiScene;
    cro::Scene m_trophyScene;

    cro::Clock m_mouseClock;
    bool m_mouseVisible;

    InputParser m_inputParser;
    CPUGolfer m_cpuGolfer;

    bool m_wantsGameState;
    cro::Clock m_readyClock; //pings ready state until ack'd

    cro::RenderTexture m_gameSceneTexture;
    cro::RenderTexture m_trophySceneTexture;
    cro::CubemapTexture m_reflectionMap;

    cro::UniformBuffer m_scaleBuffer;
    cro::UniformBuffer m_resolutionBuffer;
    cro::UniformBuffer m_windBuffer;

    struct WindUpdate final
    {
        float currentWindSpeed = 0.f;
        glm::vec3 currentWindVector = glm::vec3(0.f);
        glm::vec3 windVector = glm::vec3(0.f);
    }m_windUpdate;

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
            ShadowMap,
            Leaderboard,
            Player,
            Hair,
            Course,
            Ball,
            Billboard,
            Trophy,
            Beacon,

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
    std::string m_courseTitle;

    void loadAssets();
    void addSystems();
    void buildScene();
    void initAudio();

    void createWeather(); //weather.cpp
    void createClouds(const ThemeSettings&);
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
            Thinking,
            MessageBoard,
            Bunker,
            Foul,
            QuitReady,
            QuitNotReady,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    struct Avatar final
    {
        bool flipped = false;
        cro::Entity model;
        cro::Attachment* hands = nullptr;
        std::array<std::size_t, AnimationID::Count> animationIDs = {};
        cro::Entity ballModel;
    };
    std::array<std::array<Avatar, ConnectionData::MaxPlayers>, ConstVal::MaxClients> m_avatars;
    Avatar* m_activeAvatar;

    struct ClubModel final
    {
        enum { Wood, Iron, Count };
    };
    std::array<cro::Entity, ClubModel::Count> m_clubModels = {};

    float m_camRotation; //used to offset the rotation of the wind indicator
    bool m_roundEnded;
    glm::vec2 m_viewScale;
    std::size_t m_scoreColumnCount;
    LeaderboardTexture m_leaderboardTexture;

    cro::Entity m_courseEnt;
    cro::Entity m_waterEnt;
    cro::Entity m_uiReflectionCam;
    std::uint8_t m_readyQuitFlags;

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
    void notifyAchievement(const std::array<std::uint8_t, 2u>&);
    void showNotification(const cro::String&);
    void toggleQuitReady();

    void buildTrophyScene();
    std::array<cro::Entity, 3u> m_trophies = {};
    std::array<cro::Entity, 3u> m_trophyLabels = {};

    //-----------

    cro::Entity m_mapCam;
    cro::RenderTexture m_mapBuffer;
    cro::RenderTexture m_mapTexture;
    cro::SimpleQuad m_mapQuad;
    cro::SimpleQuad m_flagQuad;
    void updateMiniMap();

    cro::Entity m_greenCam;
    cro::RenderTexture m_greenBuffer;

    //------------

    bool m_hadFoul; //tracks 'boomerang' stat

    //for tracking scoreboard based stats
    struct StatBoardEntry final
    {
        std::uint8_t client = 0;
        std::uint8_t player = 0;
        std::int32_t score = 0;
    };
    std::vector<StatBoardEntry> m_statBoardScores;
};