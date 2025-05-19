/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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
#include "CameraFollowSystem.hpp"
#include "InputParser.hpp"
#include "HoleData.hpp"
#include "Billboard.hpp"
#include "GameConsts.hpp"
#include "BallTrail.hpp"
#include "ClubModels.hpp"

#include <crogine/audio/DynamicAudioStream.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/Console.hpp>
#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/UniformBuffer.hpp>

#include <crogine/detail/glm/vec2.hpp>

namespace inv
{
    struct Loadout;
}

//callback data for anim/self destruction
//of messages / options window
struct PopupAnim final
{
    enum
    {
        Delay, Open, Hold, Close,
        Abort //used to remove open messages when forcefully restarting
    }state = Delay;
    float currentTime = 0.5f;
};

//*sigh* multiple structs with the same name and different defs...
using WindCallbackData = std::pair<float, float>;

struct SharedStateData;
struct SharedProfileData;
class DrivingState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    DrivingState(cro::StateStack&, cro::State::Context, SharedStateData&, const SharedProfileData&);
    ~DrivingState();

    cro::StateID getStateID() const override { return StateID::DrivingRange; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

    struct MenuID final
    {
        enum
        {
            Dummy,
            Options,
            Summary,
            Leaderboard,

            Count
        };
    };


private:
    SharedStateData& m_sharedData;
    const SharedProfileData& m_profileData;
    InputParser m_inputParser;
    const inv::Loadout* m_loadout;

    cro::DynamicAudioStream m_musicStream;

    cro::Scene m_gameScene;
    cro::Scene m_skyScene;
    cro::Scene m_uiScene;
    glm::vec2 m_viewScale;

    cro::ResourceCollection m_resources;
    cro::RenderTexture m_backgroundTexture;
    cro::CubemapTexture m_reflectionMap;
    cro::Texture m_defaultMaskMap;
    cro::RenderTexture m_planeTexture;

    cro::UniformBuffer<float> m_scaleBuffer;
    cro::UniformBuffer<ResolutionData> m_resolutionBuffer;
    cro::UniformBuffer<WindData> m_windBuffer;

    cro::Clock m_idleTimer;
    BallTrail m_ballTrail;
    cro::Entity m_minimapIndicatorEnt;

    struct WindUpdate final
    {
        float currentWindSpeed = 0.f;
        glm::vec3 currentWindVector = glm::vec3(0.f);
    }m_windUpdate;


    struct MaterialID final
    {
        enum
        {
            BallBumped,
            Billboard,
            Cel,
            CelSkinned,
            CelTextured,
            CelTexturedSkinned,
            Course,
            Hair,
            HairReflect,
            HairGlass,
            Wireframe,
            WireframeCulled,
            WireframeCulledPoint,
            Beacon,
            Horizon,
            Trophy,
            BallTrail,
            Flag,

            Count
        };
    };
    std::array<std::int32_t, MaterialID::Count> m_materialIDs = {};

    std::vector<HoleData> m_holeData;
    std::int32_t m_targetIndex;
    std::array<cro::Billboard, BillboardID::Count> m_billboardTemplates = {};

    struct SpriteID final
    {
        enum
        {
            PowerBar,
            PowerBar10,
            PowerBarInner,
            PowerBarInnerHC,
            HookBar,
            PowerBarDouble,
            PowerBarDouble10,
            PowerBarDoubleInner,
            PowerBarDoubleInnerHC,
            HookBarDouble,

            WindIndicator,
            WindTextBg,
            WindSpeed,
            MessageBoard,
            Stars,
            SpinBg,
            SpinFg,

            Count
        };
    };
    std::array<cro::Sprite, SpriteID::Count> m_sprites = {};

    struct TextureID final
    {
        enum
        {
            Flag,

            Count
        };
    };

    Avatar m_avatar;
    ClubModels m_clubModels;

    std::array<std::int32_t, 3u> m_strokeCounts = { 5,9,18 };
    std::size_t m_strokeCountIndex;

    std::array<cro::Entity, CameraID::Count> m_cameras = {};
    std::int32_t m_currentCamera;

    cro::Entity m_defaultCam;
    cro::Entity m_freeCam;
    void toggleFreeCam();

    void addSystems();
    void loadAssets();
    void initAudio();
    void createScene();
    void createFoliage(cro::Entity);
    void createClouds();
    void createPlayer();
    void createBall();
    void createFlag();
    void startTransition();

    void hitBall();
    void setHole(std::int32_t);
    void setActiveCamera(std::int32_t);
    void forceRestart();
    void triggerGC(glm::vec3);
    
    //DrivingStateUI.cpp
#ifdef USE_GNS
    cro::Entity m_leaderboardEntity;
#endif
    cro::Entity m_courseEntity;
    cro::Shader m_saturationShader;
    std::int32_t m_saturationUniform;

    cro::RenderTexture m_mapTexture;
    cro::Entity m_mapCam;
    cro::Entity m_mapRoot;
    cro::SimpleQuad m_flagQuad;
    void createUI();
    void createPowerBars(cro::Entity);
    void createGameOptions();
    void createSummary();
    void updateMinimap();
    void updateWindDisplay(glm::vec3);    
    void showMessage(float);
    void floatingMessage(const std::string&);

    SkipState m_skipState;
    void updateSkipMessage(float);

    //create the summary screen with its own
    //encapsulation just to update the text more easily
    struct SummaryScreen final
    {
        cro::Entity root;
        cro::Entity fadeEnt;
        cro::Entity text01;
        cro::Entity text02;
        cro::Entity summary;
        cro::Entity bestMessage;
        cro::Entity roundName;
        std::array<cro::Entity, 3u> stars = {};
        cro::Entity audioEnt;
    }m_summaryScreen;
    static constexpr float FadeDepth = 1.f;

    std::array<float, 3u> m_topScores = {};
    std::array<cro::String, 3u> m_tickerStrings = {};
    void loadScores();
    void saveScores();

    cro::Clock m_statClock;

#ifdef CRO_DEBUG_
    cro::Texture m_debugHeightmap;
#endif
};