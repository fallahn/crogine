/*-----------------------------------------------------------------------

Matt Marchant - 2022
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
#include "BilliardsInput.hpp"
#include "GameConsts.hpp"
#include "server/ServerPacketData.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/UniformBuffer.hpp>
#include <crogine/gui/GuiClient.hpp>

#ifdef USE_GNS
namespace gns
{
    struct NetEvent;
}
namespace net = gns;
#else
namespace cro
{
    struct NetEvent;
}
namespace net = cro;
#endif

struct SharedStateData;

class BilliardsState final : public cro::State , public cro::GuiClient
{
public:
    BilliardsState(cro::StateStack&, cro::State::Context, SharedStateData&);
    ~BilliardsState();

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    cro::StateID getStateID() const override { return StateID::Billiards; }

private:

    SharedStateData& m_sharedData;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::CubemapTexture m_reflectionMap;
    cro::CubemapTexture m_trophyReflectionMap;

    struct LocalPlayerInfo final
    {
        std::int32_t score = -1;
        std::int8_t targetBall = -1;
    };
    std::array<std::array<LocalPlayerInfo, 2u>, 2u> m_localPlayerInfo = {};

    BilliardsInput m_inputParser;
    ActivePlayer m_currentPlayer;

    cro::RenderTexture m_gameSceneTexture;
    cro::RenderTexture m_topspinTexture;
    cro::RenderTexture m_trophyTexture;
    cro::RenderTexture m_pocketedTexture;
    std::array<cro::RenderTexture, 2u> m_targetTextures;
    cro::Entity m_topspinCamera;
    cro::Entity m_targetCamera;
    cro::Entity m_trophyCamera;
    cro::Entity m_pocketedCamera;
    cro::Entity m_targetBall;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    glm::vec2 m_viewScale;

    cro::ModelDefinition m_ballDefinition;
    cro::ModelDefinition m_fleaDefinition;
    cro::AudioScape m_audioScape;
    cro::EmitterSettings m_smokePuff;

    bool m_wantsGameState;
    bool m_wantsNotify;
    bool m_gameEnded;
    cro::Clock m_readyClock;

    struct
    {
        std::uint32_t shaderID = 0;
        std::int32_t timeUniform = -1;
    }m_indicatorProperties;


    struct CameraID final
    {
        enum
        {
            Spectate,
            Overhead,
            Player,
            Transition,

            Count
        };
    };
    std::array<cro::Entity, CameraID::Count> m_cameras;
    cro::Entity m_cameraController; //player cam is parented to this
    std::int32_t m_activeCamera;

#ifdef CRO_DEBUG_
    cro::RenderTexture m_debugBuffer;
#endif

    std::int32_t m_gameMode;
    cro::TextureID m_ballTexture;
    TableInfo m_tableInfo;
    cro::Entity m_cueball;
    cro::Entity m_localCue;
    cro::Entity m_remoteCue;
    std::uint8_t m_readyQuitFlags;

    struct MaterialID final
    {
        enum
        {
            WireFrame,
            Table,
            Ball,
            Cue,
            Trophy,
            TrophyBase,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    void loadAssets();
    void addSystems();
    void buildScene();

    void handleNetEvent(const net::NetEvent&);
    void removeClient(std::uint8_t);
    void spawnBall(const ActorInfo&);
    void updateBall(const BilliardsUpdate&);
    void updateGhost(const BilliardsUpdate&);
    void setPlayer(const BilliardsPlayer&);
    void sendReadyNotify();

    void setActiveCamera(std::int32_t);
    void toggleOverhead();
    void resizeBuffers();

    glm::vec4 getSubrect(std::int8_t) const;
    void addPocketBall(std::int8_t);
    void spawnFlea();
    void spawnFace();
    void spawnPuff(glm::vec3);
    void spawnSnail();

    //**BilliardsStateUI.cpp**//
    struct SpriteID final
    {
        enum
        {
            QuitReady, QuitNotReady,
            Face, Snail,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};
    struct AnimationID final
    {
        enum
        {
            In, Cycle, Out,
            Count
        };
    };
    std::array<std::int32_t, AnimationID::Count> m_faceAnimationIDs = {};
    std::array<std::int32_t, AnimationID::Count> m_snailAnimationIDs = {};

    void createUI();
    void showReadyNotify(const BilliardsPlayer&);
    void showNotification(const cro::String&);
    void showGameEnd(const BilliardsPlayer&);
    void toggleQuitReady();
    void createMiniballScenes();
    void updateTargetTexture(std::int32_t targetIndex, std::int8_t);
};