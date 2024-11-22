﻿/*-----------------------------------------------------------------------

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

#include "GolfState.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "SharedStateData.hpp"
#include "InterpolationSystem.hpp"
#include "ClientPacketData.hpp"
#include "MessageIDs.hpp"
#include "Clubs.hpp"
#include "TextAnimCallback.hpp"
#include "ClientCollisionSystem.hpp"
#include "GolfParticleDirector.hpp"
#include "PlayerAvatar.hpp"
#include "GolfSoundDirector.hpp"
#include "TutorialDirector.hpp"
#include "BallSystem.hpp"
#include "FpsCameraSystem.hpp"
#include "NotificationSystem.hpp"
#include "TrophyDisplaySystem.hpp"
#include "FloatingTextSystem.hpp"
#include "CloudSystem.hpp"
#include "VatAnimationSystem.hpp"
#include "BeaconCallback.hpp"
#include "SpectatorSystem.hpp"
#include "PropFollowSystem.hpp"
#include "BallAnimationSystem.hpp"
#include "LightAnimationSystem.hpp"
#include "WeatherAnimationSystem.hpp"
#include "SpectatorAnimCallback.hpp"
#include "MiniBallSystem.hpp"
#include "CallbackData.hpp"
#include "XPAwardStrings.hpp"
#include "ChunkVisSystem.hpp"
#include "AvatarRotationSystem.hpp"
#include "Career.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>
#include <Timeline.hpp>
#include <Input.hpp>

#include <crogine/audio/AudioScape.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/ecs/InfoFlags.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>
#include <crogine/ecs/systems/LightVolumeSystem.hpp>

#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/LightVolume.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/CircleMeshBuilder.hpp>

#include <crogine/network/NetClient.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/rotate_vector.hpp>
#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include "../ErrorCheck.hpp"

#include <sstream>

using namespace cl;

namespace
{
#ifdef CRO_DEBUG_
    std::int32_t debugFlags = 0;
    cro::Entity ballEntity;
    glm::vec4 topSky;
    glm::vec4 bottomSky;

#endif // CRO_DEBUG_

    std::int32_t depthUpdateCount = 1;

    float godmode = 1.f;
    std::int32_t survivorXP = 25;

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    constexpr float MaxRotation = 0.3f;// 0.13f;
    constexpr float MaxPuttRotation = 0.4f;// 0.24f;

    bool recordCam = false;

    bool isFastCPU(const SharedStateData& sd, const ActivePlayer& activePlayer)
    {
        return sd.connectionData[activePlayer.client].playerData[activePlayer.player].isCPU
            && sd.fastCPU;
    }
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State            (stack, context),
    m_musicStream           (2,48000),
    m_ntpPro                (sd.scoreType == ScoreType::NearestThePinPro),
    m_sharedData            (sd),
    m_gameScene             (context.appInstance.getMessageBus(), 1024/*, cro::INFO_FLAG_SYSTEM_TIME | cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_skyScene              (context.appInstance.getMessageBus(), 512),
    m_uiScene               (context.appInstance.getMessageBus(), 1536),
    m_trophyScene           (context.appInstance.getMessageBus()),
    m_textChat              (m_uiScene, sd),
    m_inputParser           (sd, &m_gameScene),
    m_cpuGolfer             (m_inputParser, m_currentPlayer, m_collisionMesh),
    m_humanCount            (0),
    m_wantsGameState        (true),
    m_groupIdle             (false),
    m_serverGroup           (0),
    m_allowAchievements     (false),
    m_lightVolumeDefinition (m_resources),
    m_scaleBuffer           ("PixelScale"),
    m_resolutionBuffer      ("ScaledResolution"),
    m_windBuffer            ("WindValues"),
    m_holeToModelRatio      (1.f),
    m_currentHole           (0),
    m_distanceToHole        (1.f), //don't init to 0 in case we get div0
    m_NTPDistance           (1.f), //don't init to 0 in case we get div0
    m_strokeTimer           (0.f),
    m_resumedFromSave       (false),
    m_mulliganCount         (0),
    m_terrainBuilder        (sd, m_holeData),
    m_audioPath             ("assets/golf/sound/ambience.xas"),
    m_currentCamera         (CameraID::Player),
    m_idleTime              (cro::seconds(180.f)),
    m_idleCameraIndex       (0),
    m_lastSpectatePosition  (0.f),
    m_photoMode             (false),
    m_useDOF                (false),
    m_restoreInput          (false),
    m_activeAvatar          (nullptr),
    m_terrainLevel          (0.f),
    m_camRotation           (0.f),
    m_roundEnded            (false),
    m_newHole               (true),
    m_suddenDeath           (false),
    m_viewScale             (1.f),
    m_scoreColumnCount      (2),
    m_readyQuitFlags        (0),
    m_courseIndex           (getCourseIndex(sd.mapDirectory.toAnsiString())),
    m_emoteWheel            (sd, m_currentPlayer, m_textChat)
{
    sd.quickplayOpponents = std::clamp(sd.quickplayOpponents, 0, 3);
    if (sd.quickplayOpponents != 0)
    {
        m_friendlyPlayers = std::make_unique<FriendlyPlayers>(sd.clubSet);

        const auto& league = League(LeagueRoundID::Club, m_sharedData);
        const auto stride = League::PlayerCount / sd.quickplayOpponents;
        
        //track these so we always ensure the next player has a greater index
        std::array<std::size_t, 3u> indices = {};
        indices[0] = cro::Util::Random::value(0u, stride);
        
        m_friendlyPlayers->addPlayer(league.getTable()[indices[0]]);
        for (auto i = 1; i < sd.quickplayOpponents; ++i)
        {
            auto idx = indices[i-1] + cro::Util::Random::value(1u, stride - 1);
            idx = std::clamp(idx, std::size_t(0), League::PlayerCount - 1);
            indices[i] = idx;

            m_friendlyPlayers->addPlayer(league.getTable()[idx]);
        }
    }
    
    if (sd.weatherType == WeatherType::Random)
    {
        sd.weatherType = cro::Util::Random::value(WeatherType::Clear, WeatherType::Mist);
    }

    switch (sd.scoreType)
    {
    default: break;
    case ScoreType::ClubShuffle:
        sd.inputBinding.clubset = ClubID::getRandomSet();
        break;
    case ScoreType::NearestThePinPro:
        sd.scoreType = ScoreType::NearestThePin;
        [[fallthrough]];
    case ScoreType::NearestThePin:
        //the server will have reset this so let's manually sync
        sd.gimmeRadius = 0;
        break;
    }

    sd.holesPlayed = 0;
    m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);

    survivorXP = 25;
    godmode = 1.f;
    registerCommand("god", [](const std::string&)
        {
            if (godmode == 1)
            {
                godmode = 4.f;
                cro::Console::print("godmode ON");
            }
            else
            {
                godmode = 1.f;
                cro::Console::print("godmode OFF");
            }
        });

    registerCommand("render_wavemap", [](const std::string& s) 
        {
            if (s == "false" || s == "0")
            {
                depthUpdateCount = 0;
                cro::Console::print("wavemap rendering is OFF");
            }
            else if(s == "true" || s == "1")
            {
                depthUpdateCount = 1;
                cro::Console::print("wavemap rendering is ON");
            }
            else
            {
                cro::Console::print("<0|1> or <false|true> enables or disables rendering of depthmap used by water waves");
            }
        });

    m_shadowQuality.update(sd.hqShadows);

    sd.baseState = StateID::Golf;

    std::int32_t humanCount = 0;
    std::int32_t humanIndex = 0;
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        if (!m_sharedData.localConnectionData.playerData[i].isCPU)
        {
            humanCount++;
            humanIndex = i;
        }
    }
    m_allowAchievements = (humanCount == 1) && (getCourseIndex(sd.mapDirectory) != -1);
    m_humanCount = humanCount;
    m_inputParser.setHumanCount(humanCount);

#ifndef USE_GNS
    if (humanCount == 1)
    {
        Social::setPlayerName(m_sharedData.localConnectionData.playerData[humanIndex].name);
    }
#endif

    for (auto& p : m_groupPlayerPositions)
    {
        p.client = 255;
    }

    //This is set when setting active player.
    Achievements::setActive(m_allowAchievements);
    Social::getMonthlyChallenge().refresh();

    //do this first so scores are reset before scoreboard
    //is first created.
    std::int32_t playerCount = 0;
    std::int32_t clientCount = 0;
    std::int32_t cpuCount = 0;
    std::uint8_t startLives = StartLives;
    for (auto& c : sd.connectionData)
    {
        if (c.playerCount != 0)
        {
            clientCount++;

            for (auto i = 0u; i < c.playerCount; ++i)
            {
                c.playerData[i].matchScore = 0;
                c.playerData[i].skinScore = 0;

                if (c.playerData[i].isCPU)
                {
                    cpuCount++;
                }
                startLives = std::max(startLives - 1, 2);

                playerCount++;
            }
        }
    }

    //this initial value doesn't come from the server
    //until that player takes their turn, so we set it here
    if (playerCount == 4)
    {
        startLives++;
    }

    if (playerCount > 3)
    {
        m_achievementTracker.plusFour = true;
    }

    if (sd.scoreType == ScoreType::Elimination)
    {
        for (auto& c : sd.connectionData)
        {
            for (auto& p : c.playerData)
            {
                p.skinScore = startLives;
            }
        }
    }

    if (clientCount > /*ConstVal::MaxClients*/3) //achievement is for 4
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::BetterWithFriends]);
    }
    m_cpuGolfer.setCPUCount(cpuCount, sd);
    

    //if we're playing hotseat unlock all the clubs unless
    //the clubset limit is set
    if (clientCount == 1
        && humanCount > 1)
    {
        if (!m_sharedData.clubLimit)
        {
            m_sharedData.clubSet = 2;
        }
    }


    Timeline::setGameMode(Timeline::GameMode::LoadingScreen);
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        buildTrophyScene();
        buildScene();

        createTransition();
        cacheState(StateID::Pause);
        cacheState(StateID::MapOverview);
        cacheState(StateID::Keyboard);
        });
    Timeline::setGameMode(Timeline::GameMode::Playing);

    //glLineWidth(1.5f);
#ifdef CRO_DEBUG_
    ballEntity = {};

#endif
    registerDebugWindows();
    registerDebugCommands(); //includes cubemap creation

    cro::App::getInstance().resetFrameTime();

    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createWeapon(0,1,2));
    }

    //prevents the non-steam account getting multiple writes from the same profile
    //when debugging networking
    //m_allowAchievements = false;
}

GolfState::~GolfState()
{
#ifdef USE_GNS
    Social::endStats();
#endif

    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, {});
    }

    //restore this so lobby uses correct mode again
    if (m_ntpPro)
    {
        m_sharedData.scoreType = ScoreType::NearestThePinPro;
    }
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (evt.type != SDL_MOUSEMOTION
        && evt.type != SDL_CONTROLLERBUTTONDOWN
        && evt.type != SDL_CONTROLLERBUTTONUP)
    {
        if (ImGui::GetIO().WantCaptureKeyboard
            || ImGui::GetIO().WantCaptureMouse)
        {
            if (evt.type == SDL_KEYUP)
            {
                switch (evt.key.keysym.sym)
                {
                default: break;
                case SDLK_ESCAPE:
                case SDLK_F4:
                    if (m_textChat.isVisible())
                    {
                        m_textChat.toggleWindow(false, true, false);
                    }
                    break;
                case SDLK_F8:
                    if (evt.key.keysym.mod & KMOD_SHIFT)
                    {
                        m_textChat.toggleWindow(false, true);
                    }
                    break;
                }                
            }
            return true;
        }
    }


    //handle this first in case the input parser is currently suspended
    bool emoteHandled = false;
    if (m_photoMode)
    {
        m_gameScene.getSystem<FpsCameraSystem>()->handleEvent(evt);
    }
    else
    {
        if (emoteHandled = m_emoteWheel.handleEvent(evt) || m_emoteWheel.currentScale != 0; 
            !emoteHandled && !m_textChat.isVisible())
        {
            m_inputParser.handleEvent(evt);
        }
    }



    const auto scrollScores = [&](std::int32_t step)
    {
        if (m_holeData.size() > 9)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ScoreScroll;
            cmd.action = [step](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<std::int32_t>() = step;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    };

    const auto hideMouse = [&]()
    {
        if (getStateCount() == 1)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto closeMessage = [&]()
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::MessageBoard;
        cmd.action = [](cro::Entity e, float)
        {
            auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<MessageAnim>();
            if (state == MessageAnim::Hold)
            {
                currTime = 1.f;
                state = MessageAnim::Close;
            }
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    /*const auto showMapOverview = [&]()
    {
        if (m_sharedData.minimapData.active)
        {
            requestStackPush(StateID::MapOverview);
        }
    };*/

    const auto toggleMiniZoom = [&]()
        {
            if (!m_photoMode && !emoteHandled
                && !m_inputParser.isSwingputActive())
            {
                auto& [_, dir] = m_mapRoot.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
                dir = (dir == 0) ? 1 : 0;
                m_mapRoot.getComponent<cro::Callback>().active = true;
            }
        };

    const auto setFreecamLayout = [&]()
        {
            switch (evt.type)
            {
            default: break;
            case SDL_MOUSEMOTION:
            case SDL_KEYDOWN:
                m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().play(0);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (hasPSLayout(cro::GameController::controllerID(evt.cbutton.which)))
                {
                    m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().play(2);
                }
                else
                {
                    m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().play(1);
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (hasPSLayout(cro::GameController::controllerID(evt.caxis.which)))
                {
                    m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().play(2);
                }
                else
                {
                    m_freecamMenuEnt.getComponent<cro::SpriteAnimation>().play(1);
                }
                break;
            }
        };

    const auto toggleFreecamMenu = [&]()
        {
            if (m_photoMode)
            {
                auto& [dir, _] = m_freecamMenuEnt.getComponent<cro::Callback>().getUserData<FreecamHideData>();
                dir = dir == 1 ? 0 : 1;
            }
        };

    const auto toggleDOF = [&]()
        {
            if (m_photoMode)
            {
                m_useDOF = !m_useDOF;
                enableDOF(m_useDOF);
            }
        };

    if (evt.type == SDL_KEYUP)
    {
        /*if (m_groupIdle)
        {
            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                spectateNextPlayer(1);
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                spectateNextPlayer(m_groupPlayerPositions.size() - 1);
            }
        }*/

        m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;
        //hideMouse(); //TODO this should only react to current keybindings
        switch (evt.key.keysym.sym)
        {
        default: break;
        //case SDLK_2:
        //    if (!m_textChat.isVisible()
        //        && !m_holeData[m_currentHole].puttFromTee)
        //    {
        //        if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
        //            && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
        //        {
        //            if (m_inputParser.getActive()
        //                && m_currentPlayer.terrain != TerrainID::Green)
        //            {
        //                //that's a lot of if's.

        //                if (m_currentCamera == CameraID::Player)
        //                {
        //                    setActiveCamera(CameraID::Bystander);
        //                }
        //                else if (m_currentCamera == CameraID::Bystander)
        //                {
        //                    setActiveCamera(CameraID::Player);
        //                }
        //            }
        //        }
        //    }
        //    break;
        //case SDLK_3:
        case FixedKey::FreeCam:
            if (!m_textChat.isVisible()
                && !emoteHandled)
            {
                toggleFreeCam();
            }
            break;
            //3&4 rotate camera
        case SDLK_TAB:
            showScoreboard(false);
            toggleFreecamMenu();
            break;
        case FixedKey::ZoomMinimap:
            toggleMiniZoom();
            break;
        case FixedKey::ToggleDOF:
            toggleDOF();
            break;
        case SDLK_SPACE: //TODO this should read the keymap... but it's not const
            closeMessage();
            break;
        case SDLK_F2:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::PuttingLabel;
            cmd.action =
                [](cro::Entity e, float)
                {
                    auto facing = e.getComponent<cro::Drawable2D>().getFacing();
                    facing = facing == cro::Drawable2D::Facing::Back ? cro::Drawable2D::Facing::Front : cro::Drawable2D::Facing::Back;
                    e.getComponent<cro::Drawable2D>().setFacing(facing);
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SDLK_F3:
            setUIHidden(m_courseEnt.getComponent<cro::Transform>().getOrigin().z > 0);
            break;
        case SDLK_F4:
            m_textChat.toggleWindow(false, true, false);
            break;
        case SDLK_F8:
            if (evt.key.keysym.mod & KMOD_SHIFT)
            {
                m_textChat.toggleWindow(false, true);
            }
            break;
        case SDLK_F9:
            if (evt.key.keysym.mod & KMOD_SHIFT)
            {
                cro::Console::doCommand("build_cubemaps");
            }
            break;

#ifdef CRO_DEBUG_
        case SDLK_F7:
            //m_sharedData.clientConnection.netClient.sendPacket(PacketID::SkipTurn, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable);
            
            m_sharedData.connectionData[0].playerData[0].skinScore = 10;
            m_sharedData.connectionData[0].playerData[1].skinScore = 1;
            showCountdown(30);
            //showMessageBoard(MessageBoardID::Scrub);
            //requestStackPush(StateID::Tutorial);
            //showNotification("buns");
            //Achievements::awardAchievement(AchievementStrings[AchievementID::SkinOfYourTeeth]);
            break;
        case SDLK_F10:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint16_t(ServerCommand::ChangeWind), net::NetFlag::Reliable);
            break;
        //case SDLK_KP_0: //used by emote quick key
        //    setActiveCamera(CameraID::Idle);
        //{
        //    /*static bool hidden = false;
        //    m_activeAvatar->model.getComponent<cro::Model>().setHidden(!hidden);
        //    hidden = !hidden;*/
        //}
        //    break;
        case SDLK_KP_1:
            //setActiveCamera(1);
            //m_cameras[CameraID::Sky].getComponent<CameraFollower>().state = CameraFollower::Zoom;
        {
            if (m_drone.isValid())
            {
                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::DroneHit;
                msg->position = m_drone.getComponent<cro::Transform>().getPosition();
            }
        }
            break;
        case SDLK_KP_2:
            //setActiveCamera(2);
            //showCountdown(10);
            m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Swing], 1.f, 1.2f);
            break;
        case SDLK_KP_3:
            //predictBall(0.85f);
            m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Celebrate], 1.f, 1.2f);
            break;
        case SDLK_KP_4:
        {
            static bool hidden = false;
            hidden = !hidden;
            m_holeData[m_currentHole].modelEntity.getComponent<cro::Model>().setHidden(hidden);
        }
            break;
        case SDLK_KP_5:
            gamepadNotify(GamepadNotify::HoleInOne);
            break;
        case SDLK_KP_6:
        {
            auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
            msg->type = Social::SocialEvent::LevelUp;
            msg->level = 19;
        }
            break;
        /*case SDLK_KP_7: //taken over by emote quick key
        {
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::BirdHit;
            msg2->position = m_currentPlayer.position;
            float rot = glm::eulerAngles(m_cameras[m_currentCamera].getComponent<cro::Transform>().getWorldRotation()).y;
            msg2->travelDistance = rot;
        }
            break;*/
            //used in font smoothing debug GolfGame.cpp
        /*case SDLK_KP_MULTIPLY:
        {
            auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::HoleInOne;
            msg->position = m_holeData[m_currentHole].pin;
        }
            showMessageBoard(MessageBoardID::HoleScore);*/
            break;
        case SDLK_PAGEUP:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Flag;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = FlagCallbackData::MaxHeight;
                e.getComponent<cro::Callback>().active = true;
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SDLK_PAGEDOWN:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Flag;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = 0.f;
                e.getComponent<cro::Callback>().active = true;
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        break;
        case SDLK_HOME:
            debugFlags = (debugFlags == 0) ? BulletDebug::DebugFlags : 0;
            m_collisionMesh.setDebugFlags(debugFlags);
            break;
        case SDLK_END:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ThinkBubble;
            cmd.action = [](cro::Entity e, float)
            {
                auto& [dir, _] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                dir = (dir == 0) ? 1 : 0;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SDLK_DELETE:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::Crowd;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<VatAnimation>().applaud();
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::Spectator;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().setUserData<bool>(true);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;

        if (evt.key.keysym.sym != SDLK_F12) //default screenshot key
        {
            resetIdle();
        }
        m_skipState.displayControllerMessage = false;
        setFreecamLayout();

        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            if (!evt.key.repeat
                && !m_textChat.isVisible())
            {
                showScoreboard(true);
            }
            break;
        case SDLK_UP:
        case SDLK_LEFT:
            scrollScores(-19);
            break;
        case SDLK_DOWN:
        case SDLK_RIGHT:
            scrollScores(19);
            break;
        case SDLK_RETURN:
            showScoreboard(false);
            break;
        case SDLK_ESCAPE:
            if (m_textChat.isVisible())
            {
                m_textChat.toggleWindow(false, true, false);
                break;
            }
            [[fallthrough]];
        case SDLK_PAUSE:
            if (evt.key.keysym.mod & KMOD_SHIFT)
            {
                if (Social::isAuth())
                {
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::CAT, std::uint8_t(0), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                break;
            }
            //[[fallthrough]];
        //case SDLK_p:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            toggleQuitReady();
            break;
        case FixedKey::EmoteApplaud:
        //case SDLK_KP_7: //don't do this, people use it as keybinds
            m_textChat.quickEmote(TextChat::Applaud);
            break;
        case FixedKey::EmoteHappy:
            m_textChat.quickEmote(TextChat::Happy);
            break;
        case FixedKey::EmoteLaughing:
            m_textChat.quickEmote(TextChat::Laughing);
            break;
        case FixedKey::EmoteAngry:
            m_textChat.quickEmote(TextChat::Angry);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        const auto activeID = activeControllerID(m_currentPlayer.player);
        if (evt.cbutton.which == cro::GameController::deviceID(activeID)
            || m_humanCount == 1)
        {
            m_sharedData.activeInput = cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.cbutton.which)) ?
                SharedStateData::ActiveInput::PS : SharedStateData::ActiveInput::XBox;
        }

        resetIdle();
        m_skipState.displayControllerMessage = true;
        setFreecamLayout();

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonTrackpad:
            m_textChat.toggleWindow(false, true, false);
            break;
        case cro::GameController::ButtonBack:
            if (!m_textChat.isVisible())
            {
                showScoreboard(true);
            }
            break;
        case cro::GameController::ButtonB:
            m_buttonStates.buttonB = true;
            showScoreboard(false);
            break;
        case cro::GameController::DPadUp:
        case cro::GameController::DPadLeft:
            scrollScores(-19);
            break;
        case cro::GameController::DPadDown:
        case cro::GameController::DPadRight:
            scrollScores(19);
            break;
        case cro::GameController::ButtonA:
            m_buttonStates.buttonA = true;
            toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        const auto activeID = activeControllerID(m_currentPlayer.player);
        if (evt.cbutton.which == cro::GameController::deviceID(activeID)
            || m_humanCount == 1)
        {
            m_sharedData.activeInput = cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.cbutton.which)) ?
                SharedStateData::ActiveInput::PS : SharedStateData::ActiveInput::XBox;

            /*if (m_groupIdle)
            {
                if (evt.cbutton.button == cro::GameController::ButtonRightShoulder)
                {
                    spectateNextPlayer(1);
                }
                else if (evt.cbutton.button == cro::GameController::ButtonLeftShoulder)
                {
                    spectateNextPlayer(m_groupPlayerPositions.size() - 1);
                }
            }*/
        }

        hideMouse();
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonBack:
            showScoreboard(false);
            break;
        case cro::GameController::ButtonStart:
        case cro::GameController::ButtonGuide:
            if (!m_textChat.isVisible())
            {
                requestStackPush(StateID::Pause);
            }
            break;
        case cro::GameController::ButtonA:
            if (evt.cbutton.which == cro::GameController::deviceID(activeID)
                || m_humanCount == 1)
            {
                closeMessage();
            }
            m_buttonStates.buttonA = false;
            break;
        case cro::GameController::ButtonB:
            m_buttonStates.buttonB = false;
            if (m_photoMode)
            {
                if (evt.cbutton.which == cro::GameController::deviceID(activeID)
                    || m_humanCount == 1)
                {
                    toggleFreeCam();
                }
            }
            break;
        case cro::GameController::ButtonRightStick:
            if (evt.cbutton.which == cro::GameController::deviceID(activeID)
                || m_humanCount == 1)
            {
                toggleMiniZoom();
            }
            break;
        case cro::GameController::ButtonX:
            if (evt.cbutton.which == cro::GameController::deviceID(activeID)
                || m_humanCount == 1)
            {
                toggleDOF();
            }
            break;
        case cro::GameController::ButtonY:
            if (evt.cbutton.which == cro::GameController::deviceID(activeID)
                || m_humanCount == 1)
            {
                toggleFreecamMenu();
            }
            break;
        }
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            scrollScores(-19);
        }
        else if (evt.wheel.y < 0)
        {
            scrollScores(19);
        }
        resetIdle();
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            toggleQuitReady();
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            closeMessage();
        }
    }

    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        m_emoteWheel.refreshLabels(); //displays labels if no controllers connected

        //pause the game
        requestStackPush(StateID::Pause);
    }
    else if (evt.type == SDL_CONTROLLERDEVICEADDED)
    {
        //hides labels
        m_emoteWheel.refreshLabels();
    }

    else if (evt.type == SDL_MOUSEMOTION)
    {
        setFreecamLayout();
        if (!m_photoMode
            && (evt.motion.state & SDL_BUTTON_RMASK) == 0)
        {
            cro::App::getWindow().setMouseCaptured(false);
        }
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        m_skipState.displayControllerMessage = true;

        if (std::abs(evt.caxis.value) > 10000)
        {
            const auto activeID = activeControllerID(m_currentPlayer.player);
            if (evt.caxis.which == cro::GameController::deviceID(activeID)
                || m_humanCount == 1)
            {
                m_sharedData.activeInput = cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.caxis.which)) ?
                    SharedStateData::ActiveInput::PS : SharedStateData::ActiveInput::XBox;
            }

            setFreecamLayout();
            hideMouse();
            resetIdle();
        }

        switch (evt.caxis.axis)
        {
        default: break;
        case cro::GameController::AxisRightY:
            if (std::abs(evt.caxis.value) > 10000)
            {
                scrollScores(cro::Util::Maths::sgn(evt.caxis.value) * 19);
            }
            break;
        }
    }

    //else if (evt.type == SDL_WINDOWEVENT)
    //{
    //    switch (evt.window.event)
    //    {
    //    default: break;
    //    case SDL_WINDOWEVENT_FOCUS_GAINED:
    //        //this needs to be delayed a frame so mouse clicking on the
    //        //open window doesn't get sent to the input parser
    //        if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
    //            && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
    //        {
    //            auto entity = m_uiScene.createEntity();
    //            entity.addComponent<cro::Callback>().active = true;
    //            entity.getComponent<cro::Callback>().function =
    //                [&](cro::Entity e, float)
    //                {
    //                    m_inputParser.setSuspended(false);
    //                    e.getComponent<cro::Callback>().active = false;
    //                    m_uiScene.destroyEntity(e);
    //                };
    //        }
    //        break;
    //    case SDL_WINDOWEVENT_FOCUS_LOST:
    //        if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
    //            && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
    //        {
    //            m_inputParser.setSuspended(true);
    //        }
    //        break;
    //    }
    //}

    m_gameScene.forwardEvent(evt);
    m_skyScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    m_trophyScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case Social::MessageID::SocialMessage:
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::LevelUp)
        {
            std::uint64_t packet = 0;
            packet |= (static_cast<std::uint64_t>(m_sharedData.clientConnection.connectionID) << 40);
            packet |= (static_cast<std::uint64_t>(m_currentPlayer.player) << 32);
            packet |= data.level;
            m_sharedData.clientConnection.netClient.sendPacket<std::uint64_t>(PacketID::LevelUp, packet, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else if (data.type == Social::SocialEvent::XPAwarded)
        {
            auto xpStr = std::to_string(data.level) + " XP";
            if (data.reason > -1)
            {
                CRO_ASSERT(data.reason < XPStringID::Count, "");
                floatingMessage(XPStrings[data.reason] + " " + xpStr);
            }
            else
            {
                floatingMessage(xpStr);
            }
        }
    }
        break;
    case MessageID::SystemMessage:
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::StateRequest)
        {
            requestStackPush(data.data);
        }
        else if (data.type == SystemEvent::ShadowQualityChanged)
        {
            applyShadowQuality();
        }
        else if (data.type == SystemEvent::TreeQualityChanged)
        {
            m_terrainBuilder.applyTreeQuality();
            m_gameScene.setSystemActive<ChunkVisSystem>(m_sharedData.treeQuality == SharedStateData::High);
        }
        else if (data.type == SystemEvent::CrowdDensityChanged)
        {
            m_terrainBuilder.applyCrowdDensity();
        }
    }
        break;
    case cro::Message::SkeletalAnimationMessage:
    {
        const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
        if (data.userType == SpriteAnimID::Swing)
        {
            //relay this message with the info needed for particle/sound effects
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::ClubSwing;
            msg2->position = m_currentPlayer.position;
            msg2->terrain = m_currentPlayer.terrain;
            msg2->club = static_cast<std::uint8_t>(getClub());

            m_gameScene.getSystem<ClientCollisionSystem>()->setActiveClub(getClub());

            auto isCPU = m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU;
            if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !isCPU)
            {
                auto strLow = static_cast<std::uint16_t>(50000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;
                auto strHigh = static_cast<std::uint16_t>(35000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;

                if (m_humanCount == 1)
                {
                    cro::GameController::rumbleStart(cro::GameController::controllerID(m_inputParser.getLastActiveController()), strLow, strHigh, 200);
                }
                else
                {
                    cro::GameController::rumbleStart(activeControllerID(m_sharedData.inputBinding.playerID), strLow, strHigh, 200);
                }
            }

            //auto skip if fast CPU is on
            if (m_sharedData.fastCPU && isCPU)
            {
                if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                    && getClub() != ClubID::Putter)
                {
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::SkipTurn, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable);
                    m_skipState.wasSkipped = true;
                }
            }

            //check if we hooked/sliced
            if (auto club = getClub(); club != ClubID::Putter
                && (!isCPU || (isCPU && !m_sharedData.fastCPU)))
            {
                //TODO this doesn't include any easing added when making the stroke
                //we should be using the value returned by getStroke() in hitBall()
                auto hook = m_inputParser.getHook() * m_activeAvatar->model.getComponent<cro::Transform>().getScale().x;
                if (hook < -MinHook)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::HookedBall;
                    floatingMessage("Hook");
                }
                else if (hook > MinHook)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::SlicedBall;
                    floatingMessage("Slice");
                }

                //LogI << "Hook is " << hook << std::endl;

                auto power = m_inputParser.getPower();
                hook *= 20.f;
                hook = std::round(hook);
                hook /= 20.f;
                static constexpr float  PowerShot = 0.97f;


                if (power > 0.59f //hmm not sure why power should factor into this?
                    && std::abs(hook) < 0.05f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = (power > PowerShot && club < ClubID::NineIron) ? GolfEvent::PowerShot : GolfEvent::NiceShot;
                    msg3->position = m_currentPlayer.position;

                    /*if (msg3->type == GolfEvent::PowerShot)
                    {
                        m_activeAvatar->ballModel.getComponent<cro::ParticleEmitter>().start();
                    }*/

                    //award more XP for aiming straight
                    float dirAmount = /*cro::Util::Easing::easeOutExpo*/((m_inputParser.getMaxRotation() - std::abs(m_inputParser.getRotation())) / m_inputParser.getMaxRotation());
                    auto xp = std::clamp(static_cast<std::int32_t>(6.f * dirAmount), 0, 6);
                    if (xp)
                    {
                        Social::awardXP(xp, XPStringID::GreatAccuracy);
                        Social::getMonthlyChallenge().updateChallenge(ChallengeID::Eight, 0);
                    }
                }
                else if (power > PowerShot
                    && club < ClubID::NineIron)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::PowerShot;
                    msg3->position = m_currentPlayer.position;

                    //m_activeAvatar->ballModel.getComponent<cro::ParticleEmitter>().start();
                }

                //hide the ball briefly to hack around the motion lag
                //(the callback automatically scales back)
                m_activeAvatar->ballModel.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                m_activeAvatar->ballModel.getComponent<cro::Callback>().active = true;
                m_activeAvatar->ballModel.getComponent<cro::Model>().setHidden(true);

                //see if we're doing something silly like facing the camera
                const auto camVec = cro::Util::Matrix::getForwardVector(m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldTransform());
                const auto playerVec = m_currentPlayer.position - m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition();
                const auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), m_inputParser.getYaw(), cro::Transform::Y_AXIS);
                const auto ballDir = glm::toMat3(rotation) * cro::Transform::X_AXIS;
                if (glm::dot(camVec, ballDir) < -0.9f && glm::dot(camVec, playerVec) > 0.f
                    && !Achievements::getAchievement(AchievementStrings[AchievementID::BadSport])->achieved)
                {
                    auto* shader = &m_resources.shaders.get(ShaderID::Noise);
                    m_courseEnt.getComponent<cro::Drawable2D>().setShader(shader);
                    for (auto& cam : m_cameras)
                    {
                        cam.getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];
                    }

                    auto entity = m_uiScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition({ 10.f, 32.f, 0.2f });
                    entity.addComponent<cro::Drawable2D>();
                    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString("FEED LOST");
                    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
                    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
                    entity.addComponent<cro::Callback>().active = true;
                    entity.getComponent<cro::Callback>().function =
                        [&](cro::Entity e, float dt)
                    {
                        static float state = 0.f;
                        static float accum = 0.f;
                        accum += dt;
                        if (accum > 0.5f)
                        {
                            accum -= 0.5f;
                            state = state == 1 ? 0.f : 1.f;
                        }

                        constexpr glm::vec2 pos(10.f, 32.f);
                        auto scale = m_viewScale / glm::vec2(m_courseEnt.getComponent<cro::Transform>().getScale());
                        e.getComponent<cro::Transform>().setScale(scale * state);
                        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos * scale, 0.2f));
                    };
                    m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

                    cro::App::postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::PlayerBad;
                    Achievements::awardAchievement(AchievementStrings[AchievementID::BadSport]);
                }
            }

            m_gameScene.setSystemActive<CameraFollowSystem>(!(isCPU && m_sharedData.fastCPU));

            //hide current terrain
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::TerrainType;
            cmd.action =
                [](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(" ");
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //show flight cam if not putting
            cmd.targetFlags = CommandID::UI::MiniGreen;
            cmd.action = [&, isCPU](cro::Entity e, float)
                {
                    if (m_currentPlayer.terrain != TerrainID::Green
                        && (!isCPU || (isCPU && !m_sharedData.fastCPU)))
                    {
                        e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 0;
                        e.getComponent<cro::Callback>().active = true;
                        e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                        m_flightCam.getComponent<cro::Camera>().active = true;
                    }
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //restore the ball origin if buried
            m_activeAvatar->ballModel.getComponent<cro::Transform>().setOrigin(glm::vec3(0.f));

            //reset the stroke timer for timeline - set this to a couple of seconds to allow for wind-up
            m_strokeTimer = 3.f;
        }
        else if (data.userType == cro::Message::SkeletalAnimationEvent::Stopped)
        {
            if (m_activeAvatar &&
                data.entity == m_activeAvatar->model)
            {
                //delayed ent to restore player cam
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(0.6f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt;

                    if (currTime < 0)
                    {
                        if (m_currentCamera == CameraID::Bystander)
                        {
                            setActiveCamera(CameraID::Player);
                        }

                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                };
            }
        }
    }
    break;
    case MessageID::SceneMessage:
    {
        const auto& data = msg.getData<SceneEvent>();
        switch(data.type)
        {
        default: break;
        case SceneEvent::RequestToggleFreecam:
            if (!m_textChat.isVisible())
            {
                toggleFreeCam();
                m_gameScene.getSystem<FpsCameraSystem>()->setControllerID(data.data);
            }
            break;
        case SceneEvent::PlayerRotate:
            if (m_activeAvatar)
            {
                m_activeAvatar->model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, data.rotation);
                m_activeAvatar->model.getComponent<AvatarRotation>().currentRotation = data.rotation;
                m_activeAvatar->model.getComponent<AvatarRotation>().targetRotation = data.rotation;
                
                auto rot = cro::Util::Net::compressFloat(data.rotation, 8);
                std::uint32_t data = (m_currentPlayer.client << 24);
                data |= (m_currentPlayer.player << 16);
                data |= static_cast<std::uint32_t>(rot & 0xffff);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::AvatarRotation, data, net::NetFlag::Unreliable);
            }
            break;
        case SceneEvent::TransitionComplete:
        {
            Timeline::addEvent(Timeline::Event::NewHole, holeNumberFromIndex());

            if (m_sharedData.leagueRoundID != LeagueRoundID::Club)
            {
                Progress::write(m_sharedData.leagueRoundID, m_currentHole, m_sharedData.connectionData[0].playerData[0].holeScores, m_mulliganCount);
            }

            if (m_resumedFromSave)
            {
                //make sure carts are properly aligned
                cro::Command cmd;
                cmd.targetFlags = CommandID::Cart;
                cmd.action = [&](cro::Entity e, float)
                    {
                        auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                        auto result = m_collisionMesh.getTerrain(pos);
                        float diff = result.height - pos.y;

                        e.getComponent<cro::Transform>().move({ 0.f, diff, 0.f });
                    };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }

            setUIHidden(false);
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Transform>().setScale(glm::vec3(0.f)); };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case SceneEvent::RequestSwitchCamera:
            
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;

            if (data.data == CameraID::Drone)
            {
                //hide the stroke indicator
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setHidden(true); 
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                setUIHidden(true);
                m_droneTextEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                m_droneTextEnt.getComponent<cro::Callback>().active = true;
            }
            else if (data.data == CameraID::Player
                && m_currentCamera == CameraID::Drone)
            {
                //show the stroke indicator if active player
                cmd.action = [&](cro::Entity e, float)
                {
                    auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                    e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                setUIHidden(false);
            }

            setActiveCamera(data.data);
        }
            break;
        case SceneEvent::RequestToggleMinimap:
        {
            auto& [_, dir] = m_mapRoot.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
            dir = (dir == 0) ? 1 : 0;
            m_mapRoot.getComponent<cro::Callback>().active = true;
        }
            break;
        }
    }
        break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();

        switch (data.type)
        {
        default: break;
        case GolfEvent::Mulligan:
        {
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::Mulligan,
                m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

            m_mulliganCount = 0;
            m_sharedData.hasMulligan = false;

            Achievements::awardAchievement(AchievementStrings[AchievementID::TryTryAgain]);

            //hide the stroke indicator
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
            cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setHidden(true);
                    e.getComponent<cro::Callback>().active = false;
                };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            m_inputParser.setActive(false, m_currentPlayer.terrain);
        }
        break;
        case GolfEvent::NiceTiming:
            Social::awardXP(10, XPStringID::NiceTiming);
            break;
        case GolfEvent::HitBall:
        {
            hitBall();
#ifdef PATH_TRACING
            beginBallDebug();
#endif
#ifdef CAMERA_TRACK
            recordCam = true;
            for (auto& v : m_cameraDebugPoints)
            {
                v.clear();
            }
#endif
        }
        break;
        case GolfEvent::ClubChanged:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float)
            {
                float scale = std::max(0.25f, Clubs[getClub()].getPower(m_distanceToHole, m_sharedData.imperialMeasurements) / Clubs[ClubID::Driver].getPower(m_distanceToHole, m_sharedData.imperialMeasurements));
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //update the player with correct club
            if (m_activeAvatar
                && m_activeAvatar->hands)
            {
                if (getClub() <= ClubID::FiveWood)
                {
                    m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Wood]);
                }
                else
                {
                    m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Iron]);
                }
                m_activeAvatar->hands->getModel().getComponent<cro::Model>().setFacing(m_activeAvatar->model.getComponent<cro::Model>().getFacing());
            }

            //update club text colour based on distance
            cmd.targetFlags = CommandID::UI::ClubName;
            cmd.action = [&](cro::Entity e, float)
            {
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    e.getComponent<cro::Text>().setString(Clubs[getClub()].getName(m_sharedData.imperialMeasurements, m_distanceToHole));

                    auto dist = m_distanceToHole * 1.67f;
                    if (getClub() < ClubID::NineIron &&
                        Clubs[getClub()].getTarget(m_distanceToHole) > dist)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                    }
                }
                else
                {
                    e.getComponent<cro::Text>().setString(" ");
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::UI::PuttPower;
            cmd.action = [&](cro::Entity e, float)
            {
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    auto club = getClub();
                    if (club == ClubID::Putter)
                    {
                        auto str = Clubs[ClubID::Putter].getName(m_sharedData.imperialMeasurements, m_distanceToHole);
                        e.getComponent<cro::Text>().setString(str.substr(str.find_last_of(' ') + 1));
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    }
                }
                else
                {
                    e.getComponent<cro::Text>().setString(" ");
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //hide wind indicator if club is less than min wind distance to hole
            cmd.targetFlags = CommandID::UI::WindHidden;
            cmd.action = [&](cro::Entity e, float)
            {
                std::int32_t dir = (getClub() > ClubID::PitchWedge) && (glm::length(m_holeData[m_currentHole].pin - m_currentPlayer.position) < 30.f) ? 0 : 1;
                e.getComponent<cro::Callback>().getUserData<WindHideData>().direction = dir;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


            if (m_currentPlayer.terrain != TerrainID::Green
                && m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
            {
                if (getClub() > ClubID::SevenIron)
                {
                    retargetMinimap(false);
                }
            }

            if (m_currentPlayer.terrain != TerrainID::Green)
            {
                auto club = getClub();
                togglePuttingView(club == ClubID::Putter);

                std::uint16_t p = (club & 0xff) << 8 | m_sharedData.localConnectionData.connectionID;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClubChanged, p, net::NetFlag::Reliable);
            }
        }
        break;
        case GolfEvent::BallLanded:
        {
            Timeline::setTimelineDesc("");

            bool oob = false;
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Scrub:
            case TerrainID::Water:
                oob = true;
                [[fallthrough]];
            case TerrainID::Bunker:
                if (data.travelDistance > 100.f
                    && !isFastCPU(m_sharedData, m_currentPlayer))
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.2f);
                }
                break;
            case TerrainID::Green:
                if (getClub() != ClubID::Putter)
                {
                    if ((data.pinDistance < 1.f
                        || data.travelDistance > 10000.f)
                        && !isFastCPU(m_sharedData, m_currentPlayer))
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Celebrate], 1.f, 1.2f);

                        if (data.pinDistance < 0.5f)
                        {
                            Social::awardXP(XPValues[XPID::Special] / 2, XPStringID::NiceChip);
                            Social::getMonthlyChallenge().updateChallenge(ChallengeID::Zero, 0);
                        }
                    }
                    else if (data.travelDistance < 9.f
                        && !isFastCPU(m_sharedData, m_currentPlayer))
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
                    }

                    //check if we're still 2 under for achievement
                    if (m_sharedData.localConnectionData.connectionID == m_currentPlayer.client
                        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].isCPU)
                    {
                        if (m_holeData[m_currentHole].par - m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]
                            < 2)
                        {
                            m_achievementTracker.twoShotsSpare = false;
                        }
                    }

                    const auto currScore = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
                    if (m_holeData[m_currentHole].par - currScore > 1)
                    {
                        Social::awardXP(5, XPStringID::GIR);
                    }
                    if (m_holeData[m_currentHole].par > 3
                        && currScore == 1)
                    {
                        Social::awardXP(2, XPStringID::FIR);
                    }
                }
                break;
            case TerrainID::Fairway:
                if (data.travelDistance < 16.f
                    && !isFastCPU(m_sharedData, m_currentPlayer))
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
                }

                if (m_holeData[m_currentHole].par > 3
                    && m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] == 1)
                {
                    Social::awardXP(2, XPStringID::FIR);
                }
                
                break;
            }

            //creates a delay before switching back to player cam
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(1.5f);
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt;

                    if (currTime < 0)
                    {
                        setActiveCamera(CameraID::Player);
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);


                        //hide the flight cam
                        cro::Command cmd;
                        cmd.targetFlags = CommandID::UI::MiniGreen;
                        cmd.action = [&](cro::Entity en, float)
                            {
                                //if (m_currentPlayer.terrain != TerrainID::Green)
                                {
                                    en.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
                                    en.getComponent<cro::Callback>().active = true;
                                }
                            };
                        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                    }
                };

            if (data.terrain == TerrainID::Fairway)
            {
                Social::awardXP(2, XPStringID::OnTheFairway);
            }

            if (oob)
            {
                //red channel triggers noise effect
                m_miniGreenEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Cyan);
            }
            else if (m_sharedData.scoreType == ScoreType::NearestThePin
                && data.terrain != TerrainID::Hole)
            {
                //display the distance travelled
                m_NTPDistance = std::sqrt(data.pinDistance);
                showMessageBoard(MessageBoardID::NTPDistance);
            }

#ifdef PATH_TRACING
            endBallDebug();
#endif
#ifdef CAMERA_TRACK
            recordCam = false;
#endif
        }
        break;
        case GolfEvent::DroneHit:
        {
            //this may have been done by someone we're playing with
            //in which case the best we can do is not falsely award it
            if (m_sharedData.groupMode == ClientGrouping::None)
            {
                //make sure we were the current player on the local client
                if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID)
                {
                    Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOneMillion]);
                    Social::awardXP(XPValues[XPID::Special] * 5, XPStringID::DroneHit);
                    Achievements::incrementStat(StatStrings[StatID::DroneHits]);
                }
            }

            m_gameScene.destroyEntity(m_drone);
            m_drone = {};

            m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];
            m_cameras[CameraID::Drone].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Noise];

            if (m_currentCamera == CameraID::Sky
                || m_currentCamera == CameraID::Drone) //although you shouldn't be able to switch to this with the ball moving
            {
                m_courseEnt.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::Noise));
            }

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 10.f, 32.f, 0.2f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString("FEED LOST");
            entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                static float state = 0.f;

                if (m_currentCamera == CameraID::Sky
                    || m_currentCamera == CameraID::Drone)
                {
                    static float accum = 0.f;
                    accum += dt;
                    if (accum > 0.5f)
                    {
                        accum -= 0.5f;
                        state = state == 1 ? 0.f : 1.f;
                    }
                }
                else
                {
                    state = 0.f;
                }

                constexpr glm::vec2 pos(10.f, 32.f);
                auto scale = m_viewScale / glm::vec2(m_courseEnt.getComponent<cro::Transform>().getScale());
                e.getComponent<cro::Transform>().setScale(scale * state);
                e.getComponent<cro::Transform>().setPosition(glm::vec3(pos * scale, 0.2f));

                if (m_drone.isValid())
                {
                    //camera was restored
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };
            m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            Timeline::addEvent(Timeline::Event::Drone, m_strokeTimer);
        }
        break;
        case GolfEvent::HoleInOne:
        {
            gamepadNotify(GamepadNotify::HoleInOne);
        }
        break;
        }
    }
    break;
    case cro::Message::ConsoleMessage:
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        switch (data.type)
        {
        default: break;
        case cro::Message::ConsoleEvent::Closed:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        case cro::Message::ConsoleEvent::Opened:
            cro::App::getWindow().setMouseCaptured(false);
            break;
        }
    }
        break;
    case cro::Message::StateMessage:
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            if (data.id == StateID::Pause
                || data.id == StateID::Tutorial)
            {
                cro::App::getWindow().setMouseCaptured(true);
            }
            else if (data.id == StateID::Options)
            {
                //update the beacon if settings changed
                cro::Command cmd;
                cmd.targetFlags = CommandID::Beacon;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = m_sharedData.showBeacon;
                    e.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //other items share the beacon colouring such as the putt assist
                cmd.targetFlags = CommandID::BeaconColour;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
                    m_spectateGhost.getComponent<cro::Model>().setMaterialProperty(0, "u_rimColour", getBeaconColour(m_sharedData.beaconColour));
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and distance values
                cmd.targetFlags = CommandID::UI::ClubName;
                cmd.action = [&](cro::Entity e, float)
                {
                    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                    {
                        e.getComponent<cro::Text>().setString(Clubs[getClub()].getName(m_sharedData.imperialMeasurements, m_distanceToHole));
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::PuttPower;
                cmd.action = [&](cro::Entity e, float)
                {
                    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                    {
                        auto club = getClub();
                        if (club == ClubID::Putter)
                        {
                            auto str = Clubs[ClubID::Putter].getName(m_sharedData.imperialMeasurements, m_distanceToHole);
                            e.getComponent<cro::Text>().setString(str.substr(str.find_last_of(' ') + 1));
                        }
                        else
                        {
                            e.getComponent<cro::Text>().setString(" ");
                        }
                    }
                    else
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::PinDistance;
                cmd.action =
                    [&](cro::Entity e, float)
                {
                    formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, m_sharedData.decimateDistance);

                    auto bounds = cro::Text::getLocalBounds(e);
                    bounds.width = std::floor(bounds.width / 2.f);
                    e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and putting grid
                cmd.targetFlags = CommandID::SlopeIndicator;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = true;
                };
                m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                m_emoteWheel.refreshLabels();
                m_ballTrail.setUseBeaconColour(m_sharedData.trailBeaconColour);

                //and the power bar
                cmd.targetFlags = CommandID::UI::Root;
                cmd.action = [&](cro::Entity e, float)
                    {
                        auto [state, _] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                        if (state == 1)
                        {
                            //we've visible
                            const float scale = m_sharedData.useLargePowerBar ? 2.f : 1.f;
                            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                        }
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::PuttPower;
                cmd.action = [&](cro::Entity e, float)
                    {
                        const auto& [posSmall, posLarge] = e.getComponent<cro::Callback>().getUserData<std::pair<glm::vec2, glm::vec2>>();

                        if (m_sharedData.useLargePowerBar)
                        {
                            e.getComponent<cro::Transform>().setPosition(posLarge);
                            e.getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
                        }
                        else
                        {
                            e.getComponent<cro::Transform>().setPosition(posSmall);
                            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        }
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::UI::BarEnt;
                cmd.action = [&](cro::Entity e, float)
                    {
                        e.getComponent<cro::Sprite>() = m_sharedData.decimatePowerBar
                            ? m_sprites[SpriteID::PowerBar10] : m_sprites[SpriteID::PowerBar];
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                m_minimapIndicatorEnt.getComponent<cro::Drawable2D>().getVertexData() = getStrokeIndicatorVerts(m_sharedData.decimatePowerBar);

                //updates the position of the entities based on bar size
                auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
                cam.resizeCallback(cam);
            }
        }
    }
        break;
    case cro::Message::SystemMessage:
    {
        const auto& data = msg.getData<cro::Message::SystemEvent>();
        if (data.type == cro::Message::SystemEvent::ScreenshotTaken)
        {
            floatingMessage("Screenshot Saved");
        }
    }
        break;
    case MessageID::AchievementMessage:
    {
        const auto& data = msg.getData<AchievementEvent>();
        std::array<std::uint8_t, 2u> packet =
        {
            m_sharedData.localConnectionData.connectionID,
            data.id
        };
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::AchievementGet, packet, net::NetFlag::Reliable);
    }
    break;

    case MessageID::AIMessage:
    {
        const auto& data = msg.getData<AIEvent>();
        if (data.type == AIEvent::Predict)
        {
            predictBall(data.power);
        }
        else
        {
            Activity a;
            a.type = data.type;
            a.client = m_sharedData.localConnectionData.connectionID;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
        break;
    case MessageID::CollisionMessage:
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.terrain == TerrainID::Scrub)
        {
            if (cro::Util::Random::value(0, 2) == 0)
            {
                auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg2->type = GolfEvent::BirdHit;
                msg2->position = data.position;

                float rot = glm::eulerAngles(m_cameras[m_currentCamera].getComponent<cro::Transform>().getWorldRotation()).y;
                msg2->travelDistance = rot;
            }
        }

        if (data.type == CollisionEvent::NearMiss)
        {
            m_achievementTracker.nearMissChallenge = true;
            Social::awardXP(2, XPStringID::NearMiss);
        }
        //else if (data.type == CollisionEvent::End
        //    && data.terrain == TerrainID::Hole)
        //{
        //    //display ring animation
        //    cro::Command cmd;
        //    cmd.targetFlags = CommandID::HoleRing;
        //    cmd.action = [](cro::Entity e2, float)
        //        {
        //            e2.getComponent<cro::Callback>().active = true;
        //        };
        //    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        //}
    }
        break;
    }

    m_cpuGolfer.handleMessage(msg);
    m_textChat.handleMessage(msg);

    m_gameScene.forwardMessage(msg);
    m_skyScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
    m_trophyScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
    m_strokeTimer += dt;
    m_textChat.update(dt);

    //hack to speed up transition when holding the action button
    static constexpr float MaxMultiplier = 2.f;
    static constexpr float MinMultiplier = 1.f;
    static float multiplier = MinMultiplier;
    if (m_currentCamera == CameraID::Transition)
    {
        if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Action])
            || m_buttonStates.buttonA)
        {
            multiplier = std::min(MaxMultiplier, multiplier + (dt * 2.f));
        }
        else
        {
            multiplier = std::max(MinMultiplier, multiplier * 0.99f);
        }
    }
    else
    {
        multiplier = std::max(MinMultiplier, multiplier * 0.99f);
    }

    dt *= multiplier;

#ifdef CRO_DEBUG_
    glm::vec3 move(0.f);
    if (cro::Keyboard::isKeyPressed(SDLK_UP))
    {
        move.z -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_DOWN))
    {
        move.z += 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_LEFT))
    {
        move.x -= 1.f;
    }
    if (cro::Keyboard::isKeyPressed(SDLK_RIGHT))
    {
        move.x += 1.f;
    }
    
    if (glm::length2(move) > 1)
    {
        move = glm::normalize(move);
    }
    m_waterEnt.getComponent<cro::Transform>().move(move * 10.f * dt);
#endif

    auto holeDir = m_holeData[m_currentHole].pin - m_currentPlayer.position;
    if (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit)
    {
        holeDir = m_holeData[m_currentHole].target - m_currentPlayer.position;
    }

    m_ballTrail.update();

    //this gets used a lot so we'll save on some calls to length()
    m_distanceToHole = glm::length(holeDir);

    m_depthMap.update(depthUpdateCount);

    if (m_sharedData.clientConnection.connected)
    {
        //these may have accumulated during loading
        for (const auto& evt : m_sharedData.clientConnection.eventBuffer)
        {
            handleNetEvent(evt);
        }
        m_sharedData.clientConnection.eventBuffer.clear();

        net::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                m_networkDebugContext.bitrateCounter += evt.packet.getSize() * 8;
                m_networkDebugContext.total += evt.packet.getSize();

                /*auto id = evt.packet.getID();
                if (id < PacketID::Poke)
                {
                    m_networkDebugContext.packetIDCounts[id]++;
                    if (m_networkDebugContext.packetIDCounts[id] >
                        m_networkDebugContext.packetIDCounts[m_networkDebugContext.highestID])
                    {
                        m_networkDebugContext.highestID = id;
                    }
                }*/
            }

            //handle events
            handleNetEvent(evt);
        }

        m_networkDebugContext.bitrateTimer += dt;
        if (m_networkDebugContext.bitrateTimer > 1.f)
        {
            m_networkDebugContext.bitrateTimer -= 1.f;
            m_networkDebugContext.bitrate = m_networkDebugContext.bitrateCounter;
            m_networkDebugContext.bitrateCounter = 0;

            m_networkDebugContext.lastHighestID = m_networkDebugContext.highestID;
            m_networkDebugContext.highestID = 0;
            std::fill(m_networkDebugContext.packetIDCounts.begin(), m_networkDebugContext.packetIDCounts.end(), 0);
        }

        if (m_wantsGameState)
        {
            if (m_readyClock.elapsed() > ReadyPingFreq)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable);
                m_readyClock.restart();
            }
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
        m_sharedData.errorMessage = "Lost connection to host.";
        requestStackPush(StateID::Error);
    }

    //update the fade distance if needed
    if (m_resolutionUpdate.targetFade != m_resolutionUpdate.resolutionData.nearFadeDistance)
    {
        float diff = m_resolutionUpdate.targetFade - m_resolutionUpdate.resolutionData.nearFadeDistance;
        if (diff > 0.0001f)
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance += (diff * (dt * 1.2f));
        }
        else if (diff < 0.0001f)
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance += (diff * (dt * 8.f));
        }
        else
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance = m_resolutionUpdate.targetFade;
        }

        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);
    }

    //update time uniforms
    static float elapsed = dt;
    elapsed += dt;
    
    m_windUpdate.currentWindSpeed += (m_windUpdate.windVector.y - m_windUpdate.currentWindSpeed) * dt;
    m_windUpdate.currentWindVector += (m_windUpdate.windVector - m_windUpdate.currentWindVector) * dt;

    WindData data;
    data.direction[0] = m_windUpdate.currentWindVector.x;
    data.direction[1] = m_windUpdate.currentWindSpeed;
    data.direction[2] = m_windUpdate.currentWindVector.z;
    data.elapsedTime = elapsed;
    m_windBuffer.setData(data);

    glm::vec3 windVector(m_windUpdate.currentWindVector.x,
                        m_windUpdate.currentWindSpeed,
                        m_windUpdate.currentWindVector.z);
    m_gameScene.getSystem<CloudSystem>()->setWindVector(windVector);

    const auto& windEnts = m_skyScene.getSystem<cro::CallbackSystem>()->getEntities();
    for (auto e : windEnts)
    {
        e.getComponent<cro::Callback>().setUserData<float>(m_windUpdate.currentWindSpeed);
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::ParticleEmitter;
    cmd.action = [&](cro::Entity e, float)
    {
        static constexpr float Strength = 2.45f;
        e.getComponent<cro::ParticleEmitter>().settings.forces[0] =
        {
            m_windUpdate.currentWindVector.x * m_windUpdate.currentWindSpeed * Strength,
            0.f,
            m_windUpdate.currentWindVector.z * m_windUpdate.currentWindSpeed * Strength
        };
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //track followed ball if we're idle
    if (m_groupIdle)
    {
        const auto c = m_groupPlayerPositions[m_idleCameraIndex].client;
        const auto p = m_groupPlayerPositions[m_idleCameraIndex].player;
        
        if (c != 255 //player may have quit
            && m_avatars[c][p].ballModel.isValid())
        {
            const auto ballPos = m_avatars[c][p].ballModel.getComponent<cro::Transform>().getWorldPosition();

            const bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
                && !m_sharedData.connectionData[c].playerData[p].targetHit);
            const auto ballTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
            const float ballDist = glm::length(ballPos - ballTarget);

            cmd.targetFlags = CommandID::UI::PinDistance;
            cmd.action = [&, ballDist, isMultiTarget](cro::Entity e, float)
                {
                    formatDistanceString(ballDist, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, m_sharedData.decimateDistance, isMultiTarget);

                    auto bounds = cro::Text::getLocalBounds(e);
                    bounds.width = std::floor(bounds.width / 2.f);
                    e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        //}
    }



    //don't update the CPU or gamepad if there are any menus open
    if (getStateCount() == 1
        && !m_textChat.isVisible())
    {
        m_cpuGolfer.update(dt, windVector, m_distanceToHole);
        m_inputParser.update(dt);

        if (float movement = m_inputParser.getCamMotion(); movement != 0)
        {
            updateCameraHeight(movement * dt);
        }

        //if we're CPU or remote player check screen pos of ball and
        //move the cam
        /*if (m_currentPlayer.client != m_sharedData.localConnectionData.connectionID
            || m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
        {
            if (m_inputParser.isAiming())
            {
                auto pos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(m_currentPlayer.position, m_gameSceneTexture.getSize());
                if ((pos.y / m_gameSceneTexture.getSize().y) < 0.08f)
                {
                    updateCameraHeight(-dt);
                }
            }
        }*/

        float rotation = m_inputParser.getCamRotation() * dt;
        if (getClub() != ClubID::Putter
            && rotation != 0)
        {
            auto& tx = m_cameras[CameraID::Player].getComponent<cro::Transform>();

            auto axis = glm::inverse(tx.getRotation()) * cro::Transform::Y_AXIS;
            tx.rotate(axis, rotation);

            auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
            auto lookDir = targetInfo.currentLookAt - tx.getWorldPosition();
            lookDir = glm::rotate(lookDir, rotation, axis);
            targetInfo.currentLookAt = tx.getWorldPosition() + lookDir;
            targetInfo.targetLookAt = targetInfo.currentLookAt;

            m_camRotation += rotation;
        }

        updateSkipMessage(dt);
    }

    
    m_emoteWheel.update(dt);
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    updateSkybox(dt);

    //m_terrainChunker.update();//this needs to be done after the camera system is updated


#ifndef CRO_DEBUG_
    if (m_roundEnded)
#endif
    {
        m_trophyScene.simulate(dt);
    }

    //tell the flag to raise or lower
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        cmd.targetFlags = CommandID::Flag;
        cmd.action = [&](cro::Entity e, float)
        {
            if (!e.getComponent<cro::Callback>().active)
            {
                auto camDist = glm::length2(m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition() - m_holeData[m_currentHole].pin);
                auto ballDist = FlagRaiseDistance * 2.f;
                if (m_activeAvatar)
                {
                    ballDist = glm::length2((m_activeAvatar->ballModel.getComponent<cro::Transform>().getWorldPosition() - m_holeData[m_currentHole].pin) * 3.f);
                }

                auto& data = e.getComponent<cro::Callback>().getUserData<FlagCallbackData>();
                if (data.targetHeight < FlagCallbackData::MaxHeight
                    && (camDist < FlagRaiseDistance || ballDist < FlagRaiseDistance))
                {
                    data.targetHeight = FlagCallbackData::MaxHeight;
                    e.getComponent<cro::Callback>().active = true;
                }
                else if (data.targetHeight > 0
                    && (camDist > FlagRaiseDistance && ballDist > FlagRaiseDistance))
                {
                    data.targetHeight = 0.f;
                    e.getComponent<cro::Callback>().active = true;
                }
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }

#ifndef CAMERA_TRACK
    //play avatar sound if player idles
    if (m_sharedData.gameMode != GameMode::Tutorial
        && !m_roundEnded
        && !m_groupIdle)
    {
        if (m_idleTimer.elapsed() > m_idleTime)
        {
            m_idleTimer.restart();
            m_idleTime = cro::seconds(std::max(20.f, m_idleTime.asSeconds() / 2.f));


            //horrible hack to make the coughing less frequent
            static std::int32_t coughCount = 0;
            if ((coughCount++ % 8) == 0)
            {
                auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
                msg->type = SceneEvent::PlayerIdle;

                gamepadNotify(GamepadNotify::NewPlayer);
            }

            auto* skel = &m_activeAvatar->model.getComponent<cro::Skeleton>();
            auto animID = m_activeAvatar->animationIDs[AnimationID::Impatient];
            auto idleID = m_activeAvatar->animationIDs[AnimationID::Idle];
            skel->play(animID, 1.f, 0.8f);

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, skel, animID, idleID](cro::Entity e, float)
            {
                auto [currAnim, nextAnim] = skel->getActiveAnimations();
                if (currAnim == animID
                    || nextAnim == animID)
                {
                    if (skel->getState() == cro::Skeleton::Stopped)
                    {
                        skel->play(idleID, 1.f, 0.8f);
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                }
                else
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
            };
        }

        //switch to idle cam if more than half time
        if (m_idleTimer.elapsed() > (m_idleTime * 0.65f)
            && m_currentCamera == CameraID::Player
            && m_inputParser.getActive()) //hmm this stops this happening on remote clients
        {
            setActiveCamera(CameraID::Idle);
            m_inputParser.setSuspended(true);

            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
            cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            Activity a;
            a.client = m_sharedData.clientConnection.connectionID;
            a.type = Activity::PlayerIdleStart;
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
    }
#else
    if (recordCam)
    {
        const auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
        bool hadUpdate = false;
        if (m_gameScene.getActiveCamera().hasComponent<CameraFollower>())
        {
            hadUpdate = m_gameScene.getActiveCamera().getComponent<CameraFollower>().hadUpdate;
            m_gameScene.getActiveCamera().getComponent<CameraFollower>().hadUpdate = false;
        }
        m_cameraDebugPoints[m_currentCamera].emplace_back(tx.getRotation(), tx.getPosition(), hadUpdate);
    }

#endif
    return true;
}

void GolfState::render()
{
    m_benchmark.update();

    //TODO we probably only need to do this once after the scene is built
    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_windBuffer.bind(2);

    //render reflections first
    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);

    auto& skyCam = m_skyCameras[SkyCam::Main].getComponent<cro::Camera>();
    skyCam.setActivePass(cro::Camera::Pass::Reflection);
    skyCam.viewport = { 0.f,0.f,1.f,1.f };
    m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Main]);

    cam.reflectionBuffer.clear(cro::Colour::Red);
    //don't want to test against skybox depth values.
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    //glCheck(glEnable(GL_PROGRAM_POINT_SIZE));
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.viewport = oldVP;

    skyCam.setActivePass(cro::Camera::Pass::Final);
    skyCam.viewport = oldVP;

    //then render scene
    if (m_holeData[m_currentHole].puttFromTee)
    {
        glUseProgram(m_gridShaders[1].shaderID);
        glUniform1f(m_gridShaders[1].transparency, m_sharedData.gridTransparency);
    }
    else
    {
        glUseProgram(m_gridShaders[0].shaderID);
        glUniform1f(m_gridShaders[0].transparency, m_sharedData.gridTransparency * (1.f - m_terrainBuilder.getSlopeAlpha()));
    }    
    m_renderTarget.clear(cro::Colour::Black);
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
#ifndef __APPLE__
    glCheck(glEnable(GL_LINE_SMOOTH));
#endif
    m_gameScene.render();
#ifdef CAMERA_TRACK
    //if (recordCam)
    //{
    //    const auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
    //    m_cameraDebugPoints[m_currentCamera].emplace_back(tx.getRotation(), tx.getPosition());
    //}
#endif
#ifndef __APPLE__
    glCheck(glDisable(GL_LINE_SMOOTH));
#endif
#ifdef CRO_DEBUG_
    //m_collisionMesh.renderDebug(cam.getActivePass().viewProjectionMatrix, m_gameSceneTexture.getSize());
#endif
    m_renderTarget.display();

    cro::Entity nightCam;

    //update mini green if ball is there
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        glUseProgram(m_gridShaders[1].shaderID);
        glUniform1f(m_gridShaders[1].transparency, 0.f);

        auto oldCam = m_gameScene.setActiveCamera(m_greenCam);
        m_overheadBuffer.clear();
        m_gameScene.render();
        m_overheadBuffer.display();
        m_gameScene.setActiveCamera(oldCam);
        nightCam = m_greenCam;
    }
    else if (m_flightCam.getComponent<cro::Camera>().active)
    {
        auto resolutionData = m_resolutionUpdate.resolutionData;
        resolutionData.nearFadeDistance = 0.15f;
        m_resolutionBuffer.setData(resolutionData);

        m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Flight]);

        //update the flight view
        auto oldCam = m_gameScene.setActiveCamera(m_flightCam);
        m_overheadBuffer.clear(/*cro::Colour(0.5, 0.5, 1.f)*/);
        m_skyScene.render();
        glClear(GL_DEPTH_BUFFER_BIT);
        m_gameScene.render();
        m_overheadBuffer.display();
        m_gameScene.setActiveCamera(oldCam);

        m_skyScene.setActiveCamera(m_skyCameras[SkyCam::Main]);
        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);

        nightCam = m_flightCam;
    }
 

    //TODO remove conditional branch?
    if (m_sharedData.nightTime)
    {
        auto& lightVolSystem = *m_gameScene.getSystem<cro::LightVolumeSystem>();
        lightVolSystem.setSourceBuffer(m_gameSceneMRTexture.getTexture(MRTIndex::Normal), cro::LightVolumeSystem::BufferID::Normal);
        lightVolSystem.setSourceBuffer(m_gameSceneMRTexture.getTexture(MRTIndex::Position), cro::LightVolumeSystem::BufferID::Position);
        lightVolSystem.updateTarget(m_gameScene.getActiveCamera(), m_lightMaps[LightMapID::Scene]);

        m_lightBlurTextures[LightMapID::Scene].clear();
        m_lightBlurQuads[LightMapID::Scene].draw();
        m_lightBlurTextures[LightMapID::Scene].display();

        if (nightCam.isValid())
        {
            lightVolSystem.setSourceBuffer(m_overheadBuffer.getTexture(MRTIndex::Normal), cro::LightVolumeSystem::BufferID::Normal);
            lightVolSystem.setSourceBuffer(m_overheadBuffer.getTexture(MRTIndex::Position), cro::LightVolumeSystem::BufferID::Position);
            lightVolSystem.updateTarget(nightCam, m_lightMaps[LightMapID::Overhead]);

            m_lightBlurTextures[LightMapID::Overhead].clear();
            m_lightBlurQuads[LightMapID::Overhead].draw();
            m_lightBlurTextures[LightMapID::Overhead].display();
        }
    }

    if (m_photoMode && m_useDOF)
    {
        m_focusTexture.clear();
        m_focusQuad.draw();
        m_focusTexture.display();
    }

#ifndef CRO_DEBUG_
    if (m_roundEnded /* && !m_sharedData.tutorial */)
#endif
    {
        m_trophySceneTexture.clear(cro::Colour::Transparent);
        m_trophyScene.render();
        m_trophySceneTexture.display();
    }

    //m_uiScene.setActiveCamera(uiCam);
    m_uiScene.render();
}

//private
void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem<InterpolationType::Linear>>(mb);
    m_gameScene.addSystem<CloudSystem>(mb, glm::vec3(MapSizeFloat.x / 2.f, 0.f, -MapSizeFloat.y / 2.f));
    m_gameScene.addSystem<AvatarRotationSystem>(mb);
    m_gameScene.addSystem<ClientCollisionSystem>(mb, m_holeData, m_collisionMesh);
    m_gameScene.addSystem<SpectatorSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<PropFollowSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<VatAnimationSystem>(mb);
    m_gameScene.addSystem<BallAnimationSystem>(mb);
    m_gameScene.addSystem<LightAnimationSystem>(mb);
    m_gameScene.addSystem<WeatherAnimationSystem>(mb);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem3D>(mb, 10.f); //water rings sprite :D
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<CameraFollowSystem>(mb);
    m_gameScene.addSystem<FpsCameraSystem>(mb, m_collisionMesh, m_sharedData)->setHumanCount(m_humanCount);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<ChunkVisSystem>(mb, MapSize, &m_terrainBuilder);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb)->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    if (m_sharedData.nightTime)
    {
        m_gameScene.addSystem<cro::LightVolumeSystem>(mb, cro::LightVolume::WorldSpace);
    }

    //m_gameScene.setSystemActive<InterpolationSystem<InterpolationType::Linear>>(false);
    m_gameScene.setSystemActive<CameraFollowSystem>(false);
    m_gameScene.setSystemActive<ChunkVisSystem>(m_sharedData.treeQuality == SharedStateData::High);
    m_gameScene.setSystemActive<WeatherAnimationSystem>(m_sharedData.weatherType == WeatherType::Rain || m_sharedData.weatherType == WeatherType::Showers);
#ifdef CRO_DEBUG_
    m_gameScene.setSystemActive<cro::ParticleSystem>(false);
    m_gameScene.setSystemActive<FpsCameraSystem>(false);
    //m_gameScene.setSystemActive<cro::SkeletalAnimator>(false); //can't do this because we rely on player animation events
#endif

    m_gameScene.addDirector<GolfParticleDirector>(m_resources.textures, m_sharedData);
    m_gameScene.addDirector<GolfSoundDirector>(m_resources.audio, m_sharedData);


    if (m_sharedData.gameMode == GameMode::Tutorial)
    {
        m_gameScene.addDirector<TutorialDirector>(m_sharedData, m_inputParser);
    }

    //this is used EXCLUSIVELY for updating wind entities
    //(which is a pain now I come to have another use for it)
    m_skyScene.addSystem<cro::CallbackSystem>(mb);
    m_skyScene.addSystem<cro::SkeletalAnimator>(mb);
    m_skyScene.addSystem<cro::CameraSystem>(mb);
    m_skyScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<NotificationSystem>(mb);
    m_uiScene.addSystem<FloatingTextSystem>(mb);
    m_uiScene.addSystem<MiniBallSystem>(mb, m_minimapZoom);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    m_trophyScene.addSystem<TrophyDisplaySystem>(mb);
    m_trophyScene.addSystem<cro::SpriteSystem3D>(mb, 300.f);
    m_trophyScene.addSystem<cro::SpriteAnimator>(mb);
    m_trophyScene.addSystem<cro::CallbackSystem>(mb);
    m_trophyScene.addSystem<cro::ParticleSystem>(mb);
    m_trophyScene.addSystem<cro::CameraSystem>(mb);
    m_trophyScene.addSystem<cro::ModelRenderer>(mb);
    m_trophyScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void GolfState::buildScene()
{
    Club::setClubLevel(m_sharedData.gameMode == GameMode::Tutorial ? 0 : m_sharedData.clubSet);

    m_achievementTracker.noGimmeUsed = (m_sharedData.gimmeRadius != 0);
    m_achievementTracker.noHolesOverPar = (m_sharedData.scoreType == ScoreType::Stroke);

    if (m_holeData.empty())
    {
        //use dummy data to get scene standing
        //but make sure to push error state too
        auto& holeDummy = m_holeData.emplace_back();
        holeDummy.modelEntity = m_gameScene.createEntity();
        holeDummy.modelEntity.addComponent<cro::Transform>();
        holeDummy.modelEntity.addComponent<cro::Callback>();
        createFallbackModel(holeDummy.modelEntity, m_resources);

        m_sharedData.errorMessage = "No Hole Data Loaded.";
        requestStackPush(StateID::Error);
    }

    //quality holing
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/cup.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 1.1f, 1.f, 1.1f });
    md.createModel(entity);

    if (m_sharedData.nightTime)
    {
        auto holeMat = m_resources.materials.get(m_materialIDs[MaterialID::BallNight]);
        entity.getComponent<cro::Model>().setMaterial(0, holeMat);
    }

    auto holeEntity = entity; //each of these entities are added to the entity with CommandID::Hole - below

    md.loadFromFile("assets/golf/models/flag.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Flag;
    entity.addComponent<float>() = 0.f;
    md.createModel(entity);
    if (md.hasSkeleton())
    {
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Flag]));
        entity.getComponent<cro::Skeleton>().play(0);
    }
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FlagCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto pos = e.getComponent<cro::Transform>().getPosition();
        auto& data = e.getComponent<cro::Callback>().getUserData<FlagCallbackData>();

        const float  speed = dt * 0.5f;

        if (data.currentHeight < data.targetHeight)
        {
            data.currentHeight = std::min(FlagCallbackData::MaxHeight, data.currentHeight + speed);
            pos.y = cro::Util::Easing::easeOutExpo(data.currentHeight / FlagCallbackData::MaxHeight);
        }
        else
        {
            data.currentHeight = std::max(0.f, data.currentHeight - speed);
            pos.y = cro::Util::Easing::easeInExpo(data.currentHeight / FlagCallbackData::MaxHeight);
        }
        
        e.getComponent<cro::Transform>().setPosition(pos);

        if (data.currentHeight == data.targetHeight)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    auto flagEntity = entity;

    md.loadFromFile("assets/golf/models/beacon.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Beacon | CommandID::BeaconColour;
    entity.addComponent<cro::Callback>().active = m_sharedData.showBeacon;
    entity.getComponent<cro::Callback>().function = BeaconCallback(m_gameScene);
    entity.getComponent<cro::Callback>().setUserData<float>(0.f); //used for debug - remove me
    md.createModel(entity);

    auto beaconMat = m_resources.materials.get(m_materialIDs[MaterialID::Beacon]);
    applyMaterialData(md, beaconMat);

    entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    entity.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::FlightCam));
    auto beaconEntity = entity;

    //registerWindow([&, beaconEntity]()
    //    {
    //        if (ImGui::Begin("Beacon"))
    //        {
    //            std::string active = beaconEntity.getComponent<cro::Callback>().active ? "Active" : "Not Active";
    //            float amount = beaconEntity.getComponent<cro::Callback>().getUserData<float>();
    //            ImGui::Text("%s, Amount: %3.1f", active.c_str(), amount);
    //        }
    //        ImGui::End();
    //    
    //    });

#ifdef CRO_DEBUG_
    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", 0.f);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    //CPUTarget = entity;

    //entity = m_gameScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //md.createModel(entity);
    //entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", 0.35f);
    //entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    //PredictedTarget = entity;
#endif

    //arrow pointing to hole
    md.loadFromFile("assets/golf/models/hole_arrow.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Beacon | CommandID::BeaconColour;
    entity.addComponent<cro::Callback>().active = m_sharedData.showBeacon;
    entity.getComponent<cro::Callback>().function =
        [flagEntity](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);

            const auto& data = flagEntity.getComponent<cro::Callback>().getUserData<FlagCallbackData>();
            float amount = cro::Util::Easing::easeOutCubic(data.currentHeight / FlagCallbackData::MaxHeight);
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(amount, amount, amount));
        };
    md.createModel(entity);

    applyMaterialData(md, beaconMat);

    entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    entity.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    auto arrowEntity = entity;

    //displays the stroke direction
    auto pos = m_holeData[m_currentHole].tee;
    pos.y += 0.01f;
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getYaw());
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeIndicator;

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    material.blendMode = cro::Material::BlendMode::Additive;
    material.enableDepthTest = false;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

    //glm::vec3 c(1.f, 0.f, 1.f);
    glm::vec3 c(1.f, 0.97f, 0.88f);
    std::vector<float> verts =
    {
        0.1f, Ball::Radius, 0.005f, c.r * IndicatorLightness, c.g * IndicatorLightness, c.b * IndicatorLightness, 1.f,
        5.f, Ball::Radius, 0.f,    c.r * IndicatorDarkness,  c.g * IndicatorDarkness,  c.b * IndicatorDarkness, 1.f,
        0.1f, Ball::Radius, -0.005f,c.r * IndicatorLightness, c.g * IndicatorLightness, c.b * IndicatorLightness, 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1,2
    };
    meshData->boundingBox = { glm::vec3(0.1f, 0.f, 0.005f), glm::vec3(5.f, Ball::Radius, -0.005f) };
    meshData->boundingSphere = meshData->boundingBox;

    auto vertStride = (meshData->vertexSize / sizeof(float));
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    entity.getComponent<cro::Model>().setHidden(true);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection | RenderFlags::FlightCam | RenderFlags::CubeMap));


    //a 'fan' which shows max rotation
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    material.blendMode = cro::Material::BlendMode::Additive;
    material.enableDepthTest = false;
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_FAN));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeArc;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection | RenderFlags::FlightCam | RenderFlags::CubeMap));
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        float scale = m_currentPlayer.terrain != TerrainID::Green ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getDirection());
    };

    const std::int32_t pointCount = 6;
    const float arc = MaxRotation * 2.f;
    const float step = arc / pointCount;
    const float radius = 2.5f;

    std::vector<glm::vec2> points;
    for(auto i = 0; i <= pointCount; ++i)
    {
        auto t = -MaxRotation + (i * step);
        auto& p = points.emplace_back(std::cos(t), std::sin(t));
        p *= radius;
    }

    c = { TextGoldColour.getVec4() };
    c *= IndicatorLightness / 10.f;
    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, Ball::Radius, 0.f, c.r, c.g, c.b, 1.f
    };
    indices = { 0 };

    for (auto i = 0u; i < points.size(); ++i)
    {
        verts.push_back(points[i].x);
        verts.push_back(Ball::Radius);
        verts.push_back(-points[i].y);
        verts.push_back(c.r);
        verts.push_back(c.g);
        verts.push_back(c.b);
        verts.push_back(1.f);

        indices.push_back(i + 1);
    }

    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));


    //ring effect when holing par or under
    material = m_resources.materials.get(m_materialIDs[MaterialID::BallTrail]);
    material.enableDepthTest = true;
    material.doubleSided = true;
    material.setProperty("u_colourRotation", m_sharedData.beaconColour);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::HoleRing | CommandID::BeaconColour;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    entity.addComponent<cro::Transform>().setScale(glm::vec3(0.f));
    
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        const float Speed = dt;
        static constexpr float MaxTime = 1.f;
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(MaxTime, progress + Speed);

        float scale = cro::Util::Easing::easeOutQuint(progress / MaxTime);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        float colour = 1.f - cro::Util::Easing::easeOutQuad(progress / MaxTime);
        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", glm::vec4(colour));

        if (progress == MaxTime)
        {
            progress = 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            e.getComponent<cro::Callback>().active = false;
        }
    };
    
    verts.clear();
    indices.clear();
    //vertex colour of the beacon model. I don't remember why I chose this specifically,
    //but it needs to match so that the hue rotation in the shader outputs the same result
    c = glm::vec3(1.f,0.f,0.781f);

    static constexpr float RingRadius = 1.f;
    auto j = 0u;
    for (auto i = 0.f; i < cro::Util::Const::TAU; i += (cro::Util::Const::TAU / 16.f))
    {
        auto x = std::cos(i) * RingRadius;
        auto z = -std::sin(i) * RingRadius;

        verts.push_back(x);
        verts.push_back(Ball::Radius);
        verts.push_back(z);
        verts.push_back(c.r);
        verts.push_back(c.g);
        verts.push_back(c.b);
        verts.push_back(1.f);
        indices.push_back(j++);

        verts.push_back(x);
        verts.push_back(Ball::Radius + 0.15f);
        verts.push_back(z);
        verts.push_back(0.02f);
        verts.push_back(0.02f);
        verts.push_back(0.02f);
        verts.push_back(1.f);
        indices.push_back(j++);
    }
    indices.push_back(indices[0]);
    indices.push_back(indices[1]);

    meshData = &entity.getComponent<cro::Model>().getMeshData();

    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    meshData->boundingBox = { glm::vec3(-7.f, 0.2f, -0.7f), glm::vec3(7.f, 0.f, 7.f) };
    meshData->boundingSphere = meshData->boundingBox;
    auto ringEntity = entity;

    //draw the flag pole as a single line which can be
    //seen from a distance - hole and model are also attached to this
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]);
    material.setProperty("u_colour", cro::Colour::White);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Hole;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].pin);
    entity.getComponent<cro::Transform>().addChild(holeEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(flagEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(beaconEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(arrowEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(ringEntity.getComponent<cro::Transform>());

    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, 2.f, 0.f,      LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 1.66f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 1.66f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,
        0.f, 1.33f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,

        0.f, 1.33f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 1.f, 0.f,      LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 1.f, 0.f,      0.05f, 0.043f, 0.05f, 1.f,
        0.f, 0.66f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,

        0.f, 0.66f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 0.33f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,

        0.f, 0.33f, 0.f,    0.05f, 0.043f, 0.05f, 1.f,
        0.f, 0.f, 0.f,      0.05f, 0.043f, 0.05f, 1.f,
    };
    indices =
    {
        0,1,2,3,4,5,6,7,8,9,10,11,12
    };
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));



    //water plane. Updated by various camera callbacks
    meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(WaterRadius, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].pin);
    waterEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, -30.f });
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Water]));
    waterEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::Refraction | RenderFlags::FlightCam));
    waterEnt.addComponent<cro::Callback>().active = true;
    waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(m_holeData[m_currentHole].pin);
    waterEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto target = e.getComponent<cro::Callback>().getUserData<glm::vec3>();
        target.y = WaterLevel;

        auto& tx = e.getComponent<cro::Transform>();
        auto diff = target - tx.getPosition();
        tx.move(diff * 5.f * dt);
    };
    m_waterEnt = waterEnt;
    m_gameScene.setWaterLevel(WaterLevel);
    m_skyScene.setWaterLevel(WaterLevel);

    //we never use the reflection buffer fot the skybox cam, but
    //it needs for one to be created for the culling to consider
    //the scene's objects for reflection map pass...
    m_skyScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.create(2, 2);

    //tee marker
    material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    bool spooky = cro::SysTime::now().months() == 10 && cro::SysTime::now().days() > 22;
    if (spooky)
    {
        md.loadFromFile("assets/golf/models/tee_balls02.cmt");
    }
    else
    {
        md.loadFromFile("assets/golf/models/tee_balls.cmt");
    }
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Tee;
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    
    auto targetDir = m_holeData[m_currentHole].target - m_holeData[m_currentHole].tee;
    m_camRotation = std::atan2(-targetDir.z, targetDir.x);
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);

    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        const auto& target = e.getComponent<cro::Callback>().getUserData<float>();
        float scale = e.getComponent<cro::Transform>().getScale().x;
        if (target < scale)
        {
            scale = std::max(target, scale - dt);
        }
        else
        {
            scale = std::min(target, scale + dt);
        }

        if (scale == target)
        {
            e.getComponent<cro::Callback>().active = false;
        }
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };


    auto teeEnt = entity;
    if (spooky && m_lightVolumeDefinition.isLoaded()
        && m_sharedData.nightTime)
    {
        static constexpr float LightRadius = 3.5f;
        static constexpr std::array<glm::vec3, 2u> Positions =
        {
            glm::vec3(0.f, 0.15f, -2.f),
            glm::vec3(0.f, 0.15f, 2.f)
        };

        for (auto i = 0u; i < Positions.size(); ++i)
        {
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(Positions[i]);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(LightRadius));
            m_lightVolumeDefinition.createModel(entity);
            entity.getComponent<cro::Model>().setHidden(true);
            entity.addComponent<cro::LightVolume>().radius = LightRadius;
            entity.getComponent<cro::LightVolume>().colour = cro::Colour(1.f, 0.55f, 0.1f);
            entity.addComponent<LightAnimation>(0.7f, 1.f);// .setPattern(LightAnimation::FlickerA);
            teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }

        setFog(0.5f);
    }

    

    //golf bags
    material.doubleSided = true;
    md.loadFromFile("assets/golf/models/golfbag02.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -0.6f, 0.f, 3.1f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.2f);
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    if (m_sharedData.localConnectionData.playerCount > 2
        || m_sharedData.connectionData[2].playerCount > 0)
    {
        md.loadFromFile("assets/golf/models/golfbag01.cmt");
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.2f, 0.f, -2.8f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 3.2f);
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Ball]));
        entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
        teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    //ball washer
    if (md.loadFromFile("assets/golf/models/washer.cmt"))
    {
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.3f, 0.f, 3.5f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -2.2f);
        md.createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
        teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        auto m = m_resources.materials.get(m_materialIDs[MaterialID::BallWasher]);
        applyMaterialData(md, m);
        entity.getComponent<cro::Model>().setMaterial(0, m);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                if ((m_currentHole % 3) == 2
                    && !m_holeData[m_currentHole].puttFromTee)
                {
                    auto& t = e.getComponent<cro::Callback>().getUserData<float>();
                    t -= dt;
                    if (t < 0)
                    {
                        t += cro::Util::Random::value(60, 180);
                        e.getComponent<cro::Skeleton>().play(0);
                    }

                    e.getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                }
                else
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                }
            };
    }


    //carts
    md.loadFromFile("assets/golf/models/cart.cmt");
    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    applyMaterialData(md, texturedMat);
    std::array cartPositions =
    {
        glm::vec3(-0.4f, 0.f, -5.9f),
        glm::vec3(2.6f, 0.f, -6.9f),
        glm::vec3(2.2f, 0.f, 6.9f),
        glm::vec3(-1.2f, 0.f, 5.2f)
    };

    //add a cart for each connected client :3
    for (auto i = 0u; i < /*m_sharedData.connectionData.size()*/4u; ++i)
    {
        if (m_sharedData.connectionData[i].playerCount > 0)
        {
            auto r = i + cro::Util::Random::value(0, 8);
            float rotation = (cro::Util::Const::PI / 4.f) * r;

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cartPositions[i]);
            entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            entity.addComponent<cro::CommandTarget>().ID = CommandID::Cart;
            md.createModel(entity);
            entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
            entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
            teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }


    //spectator ghost
    GhostCallbackData ghostData;
    md.loadFromFile("assets/golf/models/group_spectate.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    if (entity.hasComponent<cro::Skeleton>())
    {
        std::fill(ghostData.animationIDs.begin(), ghostData.animationIDs.end(), 0);
        auto& skel = entity.getComponent<cro::Skeleton>();

        ghostData.animationIDs[GhostCallbackData::Idle] = skel.getAnimationIndex("Idle");
        ghostData.animationIDs[GhostCallbackData::Looking] = skel.getAnimationIndex("Looking");
        ghostData.animationIDs[GhostCallbackData::Clap] = skel.getAnimationIndex("Clapping");
        ghostData.animation = ghostData.animationIDs[GhostCallbackData::Idle];
        skel.play(ghostData.animationIDs[GhostCallbackData::Idle]);

        entity.addComponent<cro::CommandTarget>().ID = CommandID::Ghost;
    }
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Callback>().setUserData<GhostCallbackData>(ghostData);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            const float Speed = dt * 3.f;
            auto& data = e.getComponent<cro::Callback>().getUserData<GhostCallbackData>();
            if (data.direction == GhostCallbackData::In)
            {
                //grow
                data.scale = std::min(1.f, data.scale + Speed);
                if (data.scale == 1)
                {
                    data.direction = GhostCallbackData::Hold;
                }
            }
            else if (data.direction == GhostCallbackData::Hold)
            {
                //time the animation and switch if necessary
                if (data.animation == data.animationIDs[GhostCallbackData::Looking])
                {
                    if (e.getComponent<cro::Skeleton>().getState() == cro::Skeleton::State::Stopped)
                    {
                        data.animation = data.animationIDs[GhostCallbackData::Idle];
                        e.getComponent<cro::Skeleton>().play(data.animation, 1.f, 2.f);
                    }
                }
                else
                {
                    data.animationTime -= dt;
                    if (data.animationTime < 0)
                    {
                        data.animation = data.animation == data.animationIDs[GhostCallbackData::Clap] 
                            ? data.animationIDs[GhostCallbackData::Idle] : data.animationIDs[GhostCallbackData::Looking];
                        e.getComponent<cro::Skeleton>().play(data.animation, 1.f, 1.5f);
                        data.animationTime += GhostCallbackData::MinAnimationTime + cro::Util::Random::value(-2, 5);
                    }
                }
            }
            else
            {
                //shrink
                data.scale = std::max(0.f, data.scale - Speed);
                if (data.scale == 0)
                {
                    data.direction = GhostCallbackData::In;
                    e.getComponent<cro::Callback>().active = false;

                    //reset to idle in case we were mid-animation
                    data.animation = data.animationIDs[GhostCallbackData::Idle];
                    e.getComponent<cro::Skeleton>().play(data.animation, 1.f, 2.f);
                }
            }
            const float modelScale = cro::Util::Easing::easeOutBack(data.scale);
            e.getComponent<cro::Transform>().setScale(glm::vec3(1.f, modelScale, modelScale));
        };
    m_spectateGhost = entity;

    createCameras();


    //drone model to follow camera
    createDrone();
   
    m_currentPlayer.position = m_holeData[m_currentHole].tee; //prevents the initial camera movement

    buildUI(); //put this here because we don't want to do this if the map data didn't load
    
    //a save game may have modified the current hole
    std::uint16_t data = ((m_currentHole & 0xff) << 8) | 4;
    setCurrentHole(data, m_resumedFromSave); //need to do this here to make sure everything is loaded for rendering

    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -130.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -75.f * cro::Util::Const::degToRad);

 
    if (auto month = cro::SysTime::now().months(); 
             (month == 6 || m_sharedData.weatherType == WeatherType::Showers) && !m_sharedData.nightTime)
    {
        if (cro::Util::Random::value(0, 8) == 0
            || m_sharedData.weatherType == WeatherType::Showers)
        {
            buildBow();
        }
    }

    switch (m_sharedData.weatherType)
    {
    default:
    case WeatherType::Clear:
        if (auto month = cro::SysTime::now().months(); month == 12)
        {
            if (cro::Util::Random::value(0, 20) == 0)
            {
                createWeather(WeatherType::Snow);
                setFog(m_sharedData.nightTime ? 0.45f : 0.3f);

                m_courseTitle += std::uint32_t(0x2744);
                m_courseTitle += std::uint32_t(0xFE0F);
            }
        }
        break;
    case WeatherType::Rain:
        setFog(m_sharedData.nightTime ? 0.48f : 0.4f);
        createWeather(WeatherType::Rain);
        break;
    case WeatherType::Showers:
        setFog(0.3f);
        createWeather(WeatherType::Rain);
        break;
    case WeatherType::Mist:
        setFog(m_sharedData.nightTime ? 0.45f : 0.35f);
        break;
    }
}

void GolfState::createDrone()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/drone.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec3(2.f));
        entity.getComponent<cro::Transform>().setPosition({ 160.f, 1.f, -100.f }); //lazy man's half map size
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        //material.doubleSided = true;
        applyMaterialData(md, material, 1);
        material.blendMode = cro::Material::BlendMode::Alpha;
        entity.getComponent<cro::Model>().setMaterial(1, material);

        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

        entity.addComponent<cro::AudioEmitter>();

        cro::AudioScape as;
        if (as.loadFromFile("assets/golf/sound/drone.xas", m_resources.audio)
            && as.hasEmitter("drone"))
        {
            entity.getComponent<cro::AudioEmitter>() = as.getEmitter("drone");
            entity.getComponent<cro::AudioEmitter>().play();
        }

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<DroneCallbackData>();
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto oldPos = e.getComponent<cro::Transform>().getPosition();
            auto playerPos = m_currentPlayer.position;

            //rotate towards active player
            glm::vec2 dir = glm::vec2(oldPos.x - playerPos.x, oldPos.z - playerPos.z);
            float rotation = std::atan2(dir.y, dir.x) + (cro::Util::Const::PI / 2.f);


            auto& [currRotation, acceleration, target, _] = e.getComponent<cro::Callback>().getUserData<DroneCallbackData>();

            //move towards skycam target
            static constexpr float MoveSpeed = 6.f;
            static constexpr float MinRadius = MoveSpeed * MoveSpeed;
            //static constexpr float AccelerationRadius = 7.f;// 40.f;

            auto movement = target.getComponent<cro::Transform>().getPosition() - oldPos;
            if (auto len2 = glm::length2(movement); len2 > MinRadius)
            {
                //const float len = std::sqrt(len2);
                //movement /= len;
                //movement *= MoveSpeed;

                ////go slower over short distances
                //const float multiplier = 0.6f + (0.4f * std::min(1.f, len / AccelerationRadius));

                //acceleration = std::min(1.f, acceleration + ((dt / 2.f) * multiplier));
                //movement *= cro::Util::Easing::easeInSine(acceleration);

                currRotation += cro::Util::Maths::shortestRotation(currRotation, rotation) * dt;
            }
            else
            {
                acceleration = 0.f;
                currRotation = std::fmod(currRotation + (dt * 0.5f), cro::Util::Const::TAU);
            }
            //e.getComponent<cro::Transform>().move(movement * dt);
            
            
            //---------------
            static glm::vec3 vel(0.f);
            auto pos = cro::Util::Maths::smoothDamp(e.getComponent<cro::Transform>().getPosition(), target.getComponent<cro::Transform>().getPosition(), vel, 3.f, dt, 24.f);
            e.getComponent<cro::Transform>().setPosition(pos);
            //--------------

            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, currRotation);

            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            m_cameras[CameraID::Drone].getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());

            //update emitter based on velocity
            auto velocity = oldPos - e.getComponent<cro::Transform>().getPosition();
            e.getComponent<cro::AudioEmitter>().setVelocity(velocity * 60.f);

            //update the pitch based on height above hole
            static constexpr float MaxHeight = 10.f;
            float height = oldPos.y - m_holeData[m_currentHole].pin.y;
            height = std::min(1.f, height / MaxHeight);

            float pitch = 0.5f + (0.5f * height);
            e.getComponent<cro::AudioEmitter>().setPitch(pitch);
        };

        m_drone = entity;

        //make sure this is actually valid...
        auto targetEnt = m_gameScene.createEntity();
        targetEnt.addComponent<cro::Transform>().setPosition({ 160.f, 30.f, -100.f });
        targetEnt.addComponent<cro::Callback>().active = true; //also used to make the drone orbit the flag (see showCountdown())
        targetEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        targetEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (!m_drone.isValid())
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            else if(m_sharedData.hosting) //we'll broadcast our pos occasionally so other clients can sync drones
            {
                auto wind = m_windUpdate.currentWindSpeed * (m_windUpdate.currentWindVector * 0.3f);
                wind += m_windUpdate.currentWindVector * 0.7f;

                auto resetPos = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().resetPosition;

                //clamp to radius
                const float Radius = m_holeData[m_currentHole].puttFromTee ? 25.f : 50.f;
                if (glm::length2(resetPos - e.getComponent<cro::Transform>().getPosition()) < (Radius * Radius))
                {
                    e.getComponent<cro::Transform>().move(wind * dt);
                }
                else
                {
                    e.getComponent<cro::Transform>().setPosition(resetPos);
                }

                static constexpr float NetTime = 1.f / 10.f;
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime += dt;
                if (currTime > NetTime)
                {
                    auto data = cro::Util::Net::compressVec3(e.getComponent<cro::Transform>().getPosition(), 480);

                    currTime -= NetTime;
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::DronePosition, data, net::NetFlag::Unreliable);
                }
            }
        };

        m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().target = targetEnt;
        m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Composite];
    }
    m_cameras[CameraID::Drone].getComponent<TargetInfo>().postProcess = &m_postProcesses[PostID::Composite];
}

void GolfState::spawnBall(const ActorInfo& info)
{
    auto ballID = m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballID;
    auto ballUserColour = m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballColour;

    //render the ball as a point so no perspective is applied to the scale
    cro::Colour miniBallColour;
    bool rollAnimation = true;
    auto material = m_resources.materials.get(m_ballResources.materialID);

    auto ball = std::find_if(m_sharedData.ballInfo.begin(), m_sharedData.ballInfo.end(),
        [ballID](const SharedStateData::BallInfo& ballPair)
        {
            return ballPair.uid == ballID;
        });

    //pick a ball at random if model is missing
    if (ball == m_sharedData.ballInfo.end())
    {
        ball = m_sharedData.ballInfo.begin() + cro::Util::Random::value(0u, m_sharedData.ballInfo.size() - 1);
        ballID = ball->uid;
    }
    //the above now makes this superfluous - but hey let's try avoiding butterflies
    if (ball != m_sharedData.ballInfo.end())
    {
        miniBallColour = ball->tint.getVec4() * ballUserColour.getVec4();

        material.setProperty("u_colour", miniBallColour);
        rollAnimation = ball->rollAnimation;
    }
    else
    {
        miniBallColour = m_sharedData.ballInfo.cbegin()->tint.getVec4() * ballUserColour.getVec4();

        //this should at least line up with the fallback model
        material.setProperty("u_colour", miniBallColour);
    }

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    //entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f }); //pushes the ent above the ground a bit to stop Z fighting
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>(
        InterpolationPoint(info.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(info.rotation), info.timestamp)).id = info.serverID;
    entity.addComponent<ClientCollider>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.ballMeshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    
    
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto scale = e.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale + (dt * 1.5f));
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        if (scale == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(false);
        }
    };
    m_avatars[info.clientID][info.playerID].ballModel = entity;



    //ball shadow
    auto ballEnt = entity;
    //material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]);
    material.setProperty("u_colour", cro::Colour::White);
    //material.blendMode = cro::Material::BlendMode::Multiply; //causes shadow to actually get darker as alpha reaches zero.. duh

    bool showTrail = !(m_sharedData.connectionData[info.clientID].playerData[info.playerID].isCPU && m_sharedData.fastCPU);
    showTrail = showTrail && (m_serverGroup == info.groupID);

    //point shadow seen from distance
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition(info.position);
    entity.addComponent<cro::Callback>().active = true;


    if (m_sharedData.nightTime)
    {
        //still do the ball trail
        entity.getComponent<cro::Callback>().function =
            [&, ballEnt, info, showTrail](cro::Entity e, float)
            {
                if (ballEnt.destroyed())
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
                else
                {
                    auto ballPos = ballEnt.getComponent<cro::Transform>().getPosition();

                    //only do this when active player.
                    if (ballEnt.getComponent<ClientCollider>().state != std::uint8_t(Ball::State::Idle)
                        || ballPos == m_holeData[m_currentHole].tee)
                    {
                        if (showTrail)
                        {
                            if (m_sharedData.showBallTrail && (info.playerID == m_currentPlayer.player && info.clientID == m_currentPlayer.client)
                                && ballEnt.getComponent<ClientCollider>().state == static_cast<std::uint8_t>(Ball::State::Flight))
                            {
                                m_ballTrail.addPoint(ballPos);
                            }
                        }
                    }
                }
            };
    }
    else
    {
        entity.getComponent<cro::Callback>().function =
            [&, ballEnt, info, showTrail](cro::Entity e, float)
            {
                if (ballEnt.destroyed())
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
                else
                {
                    //only do this when active player.
                    if (ballEnt.getComponent<ClientCollider>().state != std::uint8_t(Ball::State::Idle)
                        || glm::length2(ballEnt.getComponent<cro::Transform>().getPosition() - m_holeData[m_currentHole].tee) < 0.042f*0.042f)
                    {
                        auto ballPos = ballEnt.getComponent<cro::Transform>().getPosition();
                        auto ballHeight = ballPos.y;

                        auto c = cro::Colour::White;
                        if (ballPos.y > WaterLevel)
                        {
                            //rays have limited length so might miss from high up (making shadow disappear)
                            auto rayPoint = ballPos;
                            rayPoint.y = 10.f;
                            auto height = m_collisionMesh.getTerrain(rayPoint).height;
                            c.setAlpha(smoothstep(0.2f, 0.8f, (ballPos.y - height) / 0.25f));

                            ballPos.y = 0.00001f + (height - ballHeight);
                        }
                        e.getComponent<cro::Transform>().setPosition({ 0.f, ballPos.y, 0.f });
                        e.getComponent<cro::Model>().setHidden((m_currentPlayer.terrain == TerrainID::Green) || ballEnt.getComponent<cro::Model>().isHidden());
                        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);

                        if (showTrail)
                        {
                            if (m_sharedData.showBallTrail && (info.playerID == m_currentPlayer.player && info.clientID == m_currentPlayer.client)
                                && ballEnt.getComponent<ClientCollider>().state == static_cast<std::uint8_t>(Ball::State::Flight))
                            {
                                m_ballTrail.addPoint(ballEnt.getComponent<cro::Transform>().getPosition(), e.getIndex());
                            }
                        }
                    }
                }
            };
        entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    }

    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    if (!m_sharedData.nightTime)
    {
        //large shadow seen close up
        auto shadowEnt = entity;
        entity = m_gameScene.createEntity();
        shadowEnt.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, 0.003f, 0.f });
        m_modelDefs[ModelID::BallShadow]->createModel(entity);
        entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
        //entity.getComponent<cro::Transform>().setScale(glm::vec3(1.3f));
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, ballEnt](cro::Entity e, float)
            {
                if (ballEnt.destroyed())
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
                e.getComponent<cro::Model>().setHidden(ballEnt.getComponent<cro::Model>().isHidden());
                e.getComponent<cro::Transform>().setScale(ballEnt.getComponent<cro::Transform>().getScale()/* * 0.95f*/);
            };
    }

    //adding a ball model means we see something a bit more reasonable when close up
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    if (rollAnimation)
    {
        entity.getComponent<cro::Transform>().setPosition({ 0.f, Ball::Radius, 0.f });
        entity.getComponent<cro::Transform>().setOrigin({ 0.f, Ball::Radius, 0.f });
        entity.addComponent<BallAnimation>().parent = ballEnt;
    }
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt](cro::Entity e, float dt)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };

    const auto loadDefaultBall = [&]()
    {
        auto& defaultBall = m_ballModels.begin()->second;
        if (!defaultBall->isLoaded())
        {
            defaultBall->loadFromFile(m_sharedData.ballInfo[0].modelPath);
        }

        //a bit dangerous assuming we're not empty, but we
        //shouldn't have made it this far if there are no ball files
        //as they are vetted by the menu state.
        LogW << "Ball with ID " << (int)ballID << " not found" << std::endl;
        defaultBall->createModel(entity);
        applyMaterialData(*m_ballModels.begin()->second, material);
    };

    const auto matID = m_sharedData.nightTime ? MaterialID::BallNight : MaterialID::Ball;

    material = m_resources.materials.get(m_materialIDs[matID]);
    material.setProperty("u_ballColour", ballUserColour);
    if (m_ballModels.count(ballID) != 0
        && ball != m_sharedData.ballInfo.end())
    {
        if (!m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->loadFromFile(ball->modelPath);
        }

        if (m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->createModel(entity);
            applyMaterialData(*m_ballModels[ballID], material);
#ifdef USE_GNS
            if (ball->workshopID
                && info.clientID == m_sharedData.localConnectionData.connectionID)
            {
                std::vector<std::uint64_t> v;
                v.push_back(ball->workshopID);
                Social::beginStats(v);
            }
#endif
        }
        else
        {
            loadDefaultBall();
        }
    }
    else
    {
        loadDefaultBall();
    }
    //clamp scale of balls in case someone got funny with a large model
    const float scale = std::min(1.f, MaxBallRadius / entity.getComponent<cro::Model>().getBoundingSphere().radius);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

    entity.getComponent<cro::Model>().setMaterial(0, material);
    if (entity.getComponent<cro::Model>().getMeshData().submeshCount > 1)
    {
        //this assumes the model loaded successfully, otherwise
        //there wouldn't be two submeshes.
        auto mat = m_resources.materials.get(m_materialIDs[m_sharedData.nightTime ? MaterialID::BallNight : MaterialID::Trophy]);
        applyMaterialData(*m_ballModels[ballID], mat);
        entity.getComponent<cro::Model>().setMaterial(1, mat);
    }
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::CubeMap));
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    if (m_sharedData.nightTime)
    {
        if (m_lightVolumeDefinition.isLoaded())
        {
            static constexpr float LightRadius = 2.f;
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setScale(glm::vec3(LightRadius));
            entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.1f, 0.f });
            m_lightVolumeDefinition.createModel(entity);

            entity.addComponent<cro::LightVolume>().radius = LightRadius;
            entity.getComponent<cro::LightVolume>().colour = miniBallColour.getVec4() / 2.f;
            entity.getComponent<cro::Model>().setHidden(true);

            //hack to stop the point light lens flares drawing on balls
            entity.addComponent<LightAnimation>().pattern.clear();

            ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }



    //name label for the ball's owner
    glm::vec2 texSize(LabelTextureSize);
    texSize.y -= (LabelIconSize.y * 4.f);

    const auto playerID = info.playerID;
    const auto clientID = info.clientID;
    const auto depthOffset = ((clientID * ConstVal::MaxPlayers) + playerID) + 1; //must be at least 1 (if you change this, note that it breaks anything which refers to this uid)
    const cro::FloatRect textureRect(0.f, playerID * (texSize.y / ConstVal::MaxPlayers), texSize.x, texSize.y / ConstVal::MaxPlayers);
    const cro::FloatRect uvRect(0.f, textureRect.bottom / static_cast<float>(LabelTextureSize.y),
                            1.f, textureRect.height / static_cast<float>(LabelTextureSize.y));

    constexpr glm::vec2 AvatarSize(16.f);
    const glm::vec2 AvatarOffset((textureRect.width - AvatarSize.x) / 2.f, textureRect.height + 2.f);
    cro::FloatRect avatarUV(0.f, texSize.y / static_cast<float>(LabelTextureSize.y),
                    LabelIconSize.x / static_cast<float>(LabelTextureSize.x), 
                    LabelIconSize.y / static_cast<float>(LabelTextureSize.y));

    float xOffset = (playerID % 2) * avatarUV.width;
    float yOffset = (playerID / 2) * avatarUV.height;
    avatarUV.left += xOffset;
    avatarUV.bottom += yOffset;

    static constexpr cro::Colour BaseColour(1.f, 1.f, 1.f, 0.f);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ texSize.x / 2.f, 0.f, -0.1f - (0.01f * depthOffset) });
    entity.addComponent<cro::Drawable2D>().setTexture(&m_sharedData.nameTextures[info.clientID].getTexture());
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    //entity.getComponent<cro::Drawable2D>().setCullingEnabled(false);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PuttingLabel;
    entity.getComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, textureRect.height), glm::vec2(0.f, uvRect.bottom + uvRect.height), BaseColour),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, uvRect.bottom), BaseColour),
            cro::Vertex2D(glm::vec2(textureRect.width, textureRect.height), glm::vec2(uvRect.width, uvRect.bottom + uvRect.height), BaseColour),

            cro::Vertex2D(glm::vec2(textureRect.width, textureRect.height), glm::vec2(uvRect.width, uvRect.bottom + uvRect.height), BaseColour),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, uvRect.bottom), BaseColour),
            cro::Vertex2D(glm::vec2(textureRect.width, 0.f), glm::vec2(uvRect.width, uvRect.bottom), BaseColour),

            cro::Vertex2D(AvatarOffset + glm::vec2(0.f, AvatarSize.y), glm::vec2(avatarUV.left, avatarUV.bottom + avatarUV.height), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(0.f), glm::vec2(avatarUV.left, avatarUV.bottom), BaseColour),
            cro::Vertex2D(AvatarOffset + AvatarSize, glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom + avatarUV.height), BaseColour),

            cro::Vertex2D(AvatarOffset + AvatarSize, glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom + avatarUV.height), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(0.f), glm::vec2(avatarUV.left, avatarUV.bottom), BaseColour),
            cro::Vertex2D(AvatarOffset + glm::vec2(AvatarSize.x, 0.f), glm::vec2(avatarUV.left + avatarUV.width, avatarUV.bottom), BaseColour),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, playerID, clientID](cro::Entity e, float dt)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }

        auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
        auto& colour = e.getComponent<cro::Callback>().getUserData<float>();

        auto position = ballEnt.getComponent<cro::Transform>().getPosition();
        position.y += Ball::Radius * 3.f;

        auto labelPos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(position, m_renderTarget.getSize());
        const float halfWidth = m_renderTarget.getSize().x / 2.f;

        e.getComponent<cro::Transform>().setPosition(labelPos);

        
        if (m_groupIdle
            && terrain != TerrainID::Hole)
        {
            if (playerID == m_groupPlayerPositions[m_idleCameraIndex].player
                && clientID == m_groupPlayerPositions[m_idleCameraIndex].client)
            {
                colour = 1.f;
            }
            else
            {
                colour = std::max(0.f, colour - dt);
            }

        }
        else if (terrain == TerrainID::Green)
        {
            if (m_currentPlayer.player == playerID
                && m_sharedData.clientConnection.connectionID == clientID)
            {
                //set target fade to zero
                colour = std::max(0.f, colour - dt);
            }
            else
            {
                //calc target fade based on distance to the camera
                const auto& camTx = m_cameras[CameraID::Player].getComponent<cro::Transform>();
                auto camPos = camTx.getPosition();
                auto ballVec = position - camPos;
                auto len2 = glm::length2(ballVec);
                static constexpr float MinLength = 64.f; //8m^2
                float alpha = smoothstep(0.05f, 0.5f, 1.f - std::min(1.f, std::max(0.f, len2 / MinLength)));

                //fade slightly near the centre of the screen
                //to prevent blocking the view
                float halfPos = labelPos.x - halfWidth;
                float amount = std::min(1.f, std::max(0.f, std::abs(halfPos) / halfWidth));
                amount = /*0.1f +*/ (smoothstep(0.14f, 0.26f, amount) * /*0.85f*/0.95f); //remember tex size is probably a lot wider than the window
                alpha *= amount;

                //and hide if near the tee
                alpha *= std::min(1.f, glm::length2(m_holeData[m_currentHole].tee - position));

                float currentAlpha = colour;
                colour = std::max(0.f, std::min(1.f, currentAlpha + (dt * cro::Util::Maths::sgn(alpha - currentAlpha))));
            }
        }
        else
        {
            colour = std::max(0.f, colour - dt);
        }

        for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
        {
            v.colour.setAlpha(colour);
        }


        float scale = m_sharedData.pixelScale ? 1.f : m_viewScale.x;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //miniball for player
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.01f * depthOffset });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-2.f, 2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(-2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(2.f), miniBallColour),
        cro::Vertex2D(glm::vec2(2.f, -2.f), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniBall;
    entity.addComponent<MiniBall>().playerID = depthOffset;
    entity.getComponent<MiniBall>().parent = ballEnt;
    entity.getComponent<MiniBall>().minimap = m_minimapEnt;

    m_minimapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //and indicator icon when putting
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-1.f, 4.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(0.f,0.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(1.f, 4.5f), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, depthOffset](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
            return;
        }

        //if (m_miniGreenEnt.getComponent<cro::Sprite>().getTexture() &&
        //    m_miniGreenEnt.getComponent<cro::Sprite>().getTexture()->getGLHandle() ==
        //    m_flightTexture.getTexture().getGLHandle())
        //{
        //    //hide
        //    e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
        //}
        //else
        {
            if (m_currentPlayer.terrain == TerrainID::Green)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                auto pos = ballEnt.getComponent<cro::Transform>().getWorldPosition();
                auto iconPos = m_greenCam.getComponent<cro::Camera>().coordsToPixel(pos, m_overheadBuffer.getSize());

                const glm::vec2 Centre = glm::vec2(m_overheadBuffer.getSize() / 2u);

                iconPos -= Centre;
                iconPos *= std::min(1.f, Centre.x / glm::length(iconPos));
                iconPos += Centre;

                auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
                float scale = terrain == TerrainID::Green ? m_viewScale.x / m_miniGreenEnt.getComponent<cro::Transform>().getScale().x : 0.f;

                e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                e.getComponent<cro::Transform>().setPosition(glm::vec3(iconPos, static_cast<float>(depthOffset) / 100.f));

                const auto activePlayer = ((m_currentPlayer.client * ConstVal::MaxPlayers) + m_currentPlayer.player) + 1;
                if (m_inputParser.getActive()
                    && activePlayer == depthOffset)
                {
                    m_miniGreenIndicatorEnt.getComponent<cro::Transform>().setPosition(glm::vec3(iconPos, 0.05f));
                }
            }
            else
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
        }
    };

    m_miniGreenEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballTint = miniBallColour;

#ifdef CRO_DEBUG_
    ballEntity = ballEnt;
#endif
}

void GolfState::spawnBullsEye(const BullsEye& b)
{
    if (b.spawn)
    {
        //make sure to select the correct shader
        auto* shader = m_holeData[m_currentHole].puttFromTee ? &m_resources.shaders.get(ShaderID::CourseGrid) : &m_resources.shaders.get(ShaderID::Course);
        m_targetShader.shaderID = shader->getGLHandle();
        m_targetShader.vpUniform = shader->getUniformID("u_targetViewProjectionMatrix");

        auto targetScale = b.diametre;

        auto position = b.position;
        position.y = m_collisionMesh.getTerrain(b.position).height;

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Target]);

        //create new model
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::BullsEye;
        m_modelDefs[ModelID::BullsEye]->createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<BullsEyeData>();
        entity.getComponent<cro::Callback>().function =
            [&, targetScale](cro::Entity e, float dt)
        {
            auto& [direction, progress] = e.getComponent<cro::Callback>().getUserData<BullsEyeData>();
            if (direction == AnimDirection::Grow)
            {
                progress = std::min(1.f, progress + dt);

                float scale = cro::Util::Easing::easeOutBack(progress) * targetScale;
                e.getComponent<cro::Transform>().setScale(glm::vec3(scale, targetScale, scale));
                m_targetShader.size = (scale / 2.f);
                m_targetShader.update();

                if (progress == 1)
                {
                    direction = AnimDirection::Hold;
                }
            }
            else if (direction == AnimDirection::Hold)
            {
                //idle while rotating
            }
            else
            {
                progress = std::max(0.f, progress - dt);

                float scale = cro::Util::Easing::easeOutBack(progress) * targetScale;
                e.getComponent<cro::Transform>().setScale(glm::vec3(scale, targetScale, scale));
                m_targetShader.size = (scale / 2.f);
                m_targetShader.update();
                
                if (progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    
                    //setting to 2 just hides
                    if (direction == AnimDirection::Destroy)
                    {
                        m_gameScene.destroyEntity(e);
                    }
                }
            }

            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.2f);
        };
        m_targetShader.position = position;
        m_targetShader.update();
    }
    else
    {
        //remove existing
        cro::Command cmd;
        cmd.targetFlags = CommandID::BullsEye;
        cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = AnimDirection::Destroy;
            e.getComponent<cro::Callback>().active = true;
        };
        m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }
}

void GolfState::handleNetEvent(const net::NetEvent& evt)
{
    switch (evt.type)
    {
    case net::NetEvent::PacketReceived:
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::GroupHoled:
            if (m_groupIdle && evt.packet.as<std::int32_t>() != m_serverGroup)
            {
                //triggers audio/animation
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::SpectateApplaud;
            }
            //break;
            [[fallthrough]];
        case PacketID::GroupTurnEnded:
            if (m_groupIdle 
                && evt.packet.as<std::int32_t>() == m_gameScene.getSystem<CameraFollowSystem>()->getTargetGroup())
            {
                //creates a delay before switching back to player cam
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(1.5f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                    {
                        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                        currTime -= dt;

                        if (currTime < 0)
                        {
                            setActiveCamera(CameraID::Player);
                            e.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(e);
                        }
                    };
            }
            break;
        case PacketID::AvatarRotation:
            remoteRotation(evt.packet.as<std::uint32_t>());
            break;
        case PacketID::GroupPosition:
        {
            auto data = evt.packet.as<GroupPosition>();
            if (data.groupID >= 0 &&
                data.groupID < static_cast<std::int32_t>(ConstVal::MaxClients))
            {
                m_groupPlayerPositions[data.groupID] = data.playerData;

                if (m_groupIdle)
                {
                    //check the ball and see if we can spectate it
                    if(!m_sharedData.connectionData[data.playerData.client].playerData[data.playerData.player].holeComplete[m_currentHole]
                        && m_currentCamera == CameraID::Player)
                    {
                        spectateGroup(data.groupID);
                    }
                }
            }
        }
            break;
        case PacketID::SetIdle:
        {
            setIdleGroup(evt.packet.as<std::uint8_t>());
        }
            break;
        case PacketID::SpectateGroup:
            if (m_groupIdle)
            {
                m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(evt.packet.as<std::uint8_t>());
            }
            break;
        case PacketID::GroupID:
            m_serverGroup = evt.packet.as<std::uint8_t>();
            m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(m_serverGroup);
            m_idleCameraIndex = m_serverGroup;
            break;
        case PacketID::HoleComplete:
        {
            //hmm maybe we should be sending the hole number too
            //in case of severe lag and the hole changes in the interim...
            const auto data = evt.packet.as<std::uint16_t>();
            const auto client = (data >> 8);
            const auto player = (data & 0xff);
            m_sharedData.connectionData[client].playerData[player].holeComplete[m_currentHole] = true;
        }
            break;
        case PacketID::TargetHit:
        {
            //we do this so often we should have a single point for it...
            const auto data = evt.packet.as<std::uint16_t>();
            const auto client = (data >> 8);
            const auto player = (data & 0xff);
            m_sharedData.connectionData[client].playerData[player].targetHit = true;
        }
            break;
        case PacketID::ClubChanged:
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            if ((data & 0x00ff) != m_sharedData.localConnectionData.connectionID)
            {
                auto club = (data & 0xff00) >> 8;
                togglePuttingView(club == ClubID::Putter);
            }
        }
            break;
        case PacketID::CAT:
            catAuth();
            break;
        case PacketID::DronePosition:
        if (!m_sharedData.hosting)
        {
            auto data = evt.packet.as<std::array<std::int16_t, 3u>>();
            auto pos = cro::Util::Net::decompressVec3(data, 480);

            if (m_drone.isValid())
            {
                m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().target.getComponent<cro::Transform>().setPosition(pos);
            }
        }
            break;
        case PacketID::Poke:
            showNotification("You have been poked!");
            gamepadNotify(GamepadNotify::NewPlayer);
            {
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::Poke;
            }
            break;
        case PacketID::Elimination:
        {
            auto d = evt.packet.as<std::uint16_t>();
            std::uint8_t p = d & 0xff;
            std::uint8_t c = (d >> 8) & 0xff;

            c = std::clamp(c, std::uint8_t(0), std::uint8_t(ConstVal::MaxClients - 1));
            p = std::clamp(p, std::uint8_t(0), std::uint8_t(ConstVal::MaxPlayers - 1));

            if (c == m_sharedData.clientConnection.connectionID)
            {
                if (!m_sharedData.connectionData[c].playerData[p].isCPU)
                {
                    showMessageBoard(MessageBoardID::Eliminated);
                    survivorXP = 0; //prevents awarding this on other player's elimination after we're eliminated.
                }

                //for everyone on this client who isn't the eliminated player
                for (auto i = 0u; i < m_sharedData.connectionData[c].playerCount; ++i)
                {
                    if (i != p &&
                        !m_sharedData.connectionData[c].playerData[i].isCPU)
                    {
                        //we might be inactive between turns...
                        if (m_allowAchievements
                            && survivorXP != 0)
                        {
                            auto active = Achievements::getActive();
                            Achievements::setActive(true);
                            Social::awardXP(survivorXP, XPStringID::Survivor);
                            Achievements::setActive(active);

                            survivorXP *= 2;
                        }
                    }
                }
            }
            else
            {
                //we might be inactive between turns...
                if (m_allowAchievements
                    && survivorXP != 0)
                {
                    auto active = Achievements::getActive();
                    Achievements::setActive(true);
                    Social::awardXP(survivorXP, XPStringID::Survivor);
                    Achievements::setActive(active);

                    survivorXP *= 2;
                }
            }
            
            cro::String s = m_sharedData.connectionData[c].playerData[p].name;
            s += " was eliminated.";
            showNotification(s);

            postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::PlayerEliminated;
        }
            break;
        case PacketID::LifeGained:
        case PacketID::LifeLost:
        {
            auto d = evt.packet.as<std::uint16_t>();
            std::uint8_t p = d & 0xff;
            std::uint8_t c = (d >> 8) & 0xff;

            c = std::clamp(c, std::uint8_t(0), std::uint8_t(ConstVal::MaxClients - 1));
            p = std::clamp(p, std::uint8_t(0), std::uint8_t(ConstVal::MaxPlayers - 1));

            cro::String s = m_sharedData.connectionData[c].playerData[p].name;
            if (evt.packet.getID() == PacketID::LifeGained)
            {
                s += " gained a life!";
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::PlayerLifeGained;
            }
            else
            {
                s += " lost a life";
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::PlayerLifeLost;

                if (c == m_sharedData.localConnectionData.connectionID)
                {
                    if (m_currentPlayer.terrain != TerrainID::Hole)
                    {
                        //penalty won't have come from the server yet so
                        //update the score that the message board displays correctly
                        m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]++;
                        showMessageBoard(MessageBoardID::EliminatedStroke); //hmm we might want a separate case for this so that it doesn't trigger the appause audio
                    }
                }
            }

            showNotification(s);
        }
            break;
        case PacketID::WeatherChange:
            handleWeatherChange(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::WarnTime:
        {
            float warnTime = static_cast<float>(evt.packet.as<std::uint8_t>());

            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::AFKWarn;
            cmd.action = [warnTime](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().setUserData<float>(warnTime + 0.1f);
                    e.getComponent<cro::Callback>().active = true;
                    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        break;
        case PacketID::MaxClubs:
        {
            std::uint8_t clubSet = evt.packet.as<std::uint8_t>();
            if (clubSet < m_sharedData.clubSet)
            {
                m_sharedData.clubSet = clubSet;
                Club::setClubLevel(clubSet);

                if (m_sharedData.scoreType != ScoreType::ClubShuffle)
                {
                    //hmm this should be read from whichever player is setting the limit
                    switch (clubSet)
                    {
                    default: break;
                    case 0:
                        m_sharedData.inputBinding.clubset = ClubID::DefaultSet;
                        break;
                    case 1:
                        m_sharedData.inputBinding.clubset = ClubID::DefaultSet | (1 << ClubID::FiveWood) | (1 << ClubID::FourIron) | (1 << ClubID::SixIron);
                        break;
                    case 2:
                        m_sharedData.inputBinding.clubset = ClubID::FullSet;
                        break;
                    }
                }
            }
        }
        break;
        case PacketID::ChatMessage:
            if (m_textChat.handlePacket(evt.packet))
            {
                postMessage<SceneEvent>(MessageID::SceneMessage)->type = SceneEvent::ChatMessage;
            }
            break;
        case PacketID::FlagHit:
        {
            auto data = evt.packet.as<BullHit>();
            data.player = std::clamp(data.player, std::uint8_t(0), ConstVal::MaxPlayers);
            if (!m_sharedData.localConnectionData.playerData[data.player].isCPU)
            {
                auto active = Achievements::getActive();
                Achievements::setActive(m_allowAchievements);
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Eleven, 0);
                Achievements::incrementStat(StatStrings[StatID::FlagHits]);
                Achievements::setActive(active);
            }

            Timeline::addEvent(Timeline::Event::BeefStick, m_strokeTimer);
        }
        break;
        case PacketID::BullHit:
            handleBullHit(evt.packet.as<BullHit>());
            break;
        case PacketID::BullsEye:
        {
            spawnBullsEye(evt.packet.as<BullsEye>());
        }
        break;
        case PacketID::FastCPU:
            m_sharedData.fastCPU = evt.packet.as<std::uint8_t>() != 0;
            m_cpuGolfer.setFastCPU(m_sharedData.fastCPU);
            break;
        case PacketID::PlayerXP:
        {
            auto value = evt.packet.as<std::uint16_t>();
            std::uint8_t client = value & 0xff;
            std::uint8_t level = value >> 8;
            m_sharedData.connectionData[client].level = level;
        }
        break;
        case PacketID::BallPrediction:
        {
            auto pos = evt.packet.as<glm::vec3>();
            m_cpuGolfer.setPredictionResult(pos, m_collisionMesh.getTerrain(pos).terrain);
#ifdef CRO_DEBUG_
            //PredictedTarget.getComponent<cro::Transform>().setPosition(evt.packet.as<glm::vec3>());
#endif
        }
        break;
        case PacketID::LevelUp:
            showLevelUp(evt.packet.as<std::uint64_t>());
            break;
        case PacketID::SetPar:
        {
            std::uint16_t holeInfo = evt.packet.as<std::uint16_t>();
            std::uint8_t hole = (holeInfo & 0xff00) >> 8;
            m_holeData[hole].par = (holeInfo & 0x00ff);

            if (hole == m_currentHole)
            {
                updateScoreboard();
            }
        }
        break;
        case PacketID::Emote:
            showEmote(evt.packet.as<std::uint32_t>());
            break;
        case PacketID::MaxStrokes:
            handleMaxStrokes(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::PingTime:
        {
            auto data = evt.packet.as<std::uint32_t>();
            auto pingTime = data & 0xffff;
            auto client = (data & 0xffff0000) >> 16;

            m_sharedData.connectionData[client].pingTime = pingTime;
        }
        break;
        case PacketID::Activity:
        {
            const auto sendCommand = [&](std::int32_t spriteID, std::int32_t direction)
                {
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::UI::ThinkBubble;
                    cmd.action = [&,direction, spriteID](cro::Entity e, float)
                        {
                            auto& [dir, _, sprID] = e.getComponent<cro::Callback>().getUserData<CogitationData>();
                            
                            //if this is another sprite trying to 'out' then ignore it
                            if (direction == 1 && sprID != spriteID)
                            {
                                return;
                            }
                            
                            
                            dir = direction;
                            e.getComponent<cro::Callback>().active = true;

                            if (sprID != spriteID)
                            {
                                e.getComponent<cro::Sprite>() = m_sprites[spriteID];
                                e.getComponent<cro::SpriteAnimation>().play(0);
                                sprID = spriteID;
                            }
                        };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                };

            auto data = evt.packet.as<Activity>();
            std::int32_t spriteID = SpriteID::Thinking;
            switch (data.type)
            {
            default: break;
            case Activity::CPUThinkStart:
            case Activity::CPUThinkEnd:
                sendCommand(SpriteID::Thinking, data.type % 2);
                return;
            case Activity::PlayerChatStart:
            case Activity::PlayerChatEnd:
                spriteID = SpriteID::Typing;
                break;
            case Activity::PlayerThinkStart:
            case Activity::PlayerThinkEnd:
                spriteID = SpriteID::Thinking;
                break;
            case Activity::PlayerIdleStart:
            case Activity::PlayerIdleEnd:
                spriteID = SpriteID::Sleeping;
                break;
            case Activity::FreecamStart:
            case Activity::FreecamEnd:
                spriteID = SpriteID::Freecam;
                break;
            }

            if (data.client == m_currentPlayer.client
                && data.client != m_sharedData.clientConnection.connectionID)
            {
                sendCommand(spriteID, data.type % 2);
            }
        }
        break;
        case PacketID::ReadyQuitStatus:
            m_readyQuitFlags = evt.packet.as<std::uint8_t>();
            m_gameScene.getDirector<GolfSoundDirector>()->playSound(GolfSoundDirector::AudioID::Stone, m_currentPlayer.position);
            break;
        case PacketID::AchievementGet:
            notifyAchievement(evt.packet.as<std::array<std::uint8_t, 2u>>());
            break;
        case PacketID::ServerAchievement:
        {
            auto [client, player, achID] = evt.packet.as<std::array<std::uint8_t, 3u>>();
            if (client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[player].isCPU
                && achID < AchievementID::Count)
            {
                Achievements::awardAchievement(AchievementStrings[achID]);
            }
        }
        break;
        case PacketID::Gimme:
        {
            auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::Gimme;

            auto data = evt.packet.as<std::uint16_t>();
            auto client = (data >> 8);
            auto player = (data & 0x0f);

            showNotification(m_sharedData.connectionData[client].playerData[player].name + " took a Gimme");

            m_sharedData.connectionData[client].playerData[player].holeComplete[m_currentHole] = true;
            
            if (m_sharedData.scoreType != ScoreType::Elimination //this is messed up otherwise
                || m_sharedData.connectionData[client].playerData[player].holeScores[m_currentHole] < m_holeData[m_currentHole].par-1)
            {
                //inflate this so that the message board is correct - the update will come
                //in to assert this is correct afterwards
                m_sharedData.connectionData[client].playerData[player].holeScores[m_currentHole]++;
                showMessageBoard(MessageBoardID::Gimme);
            }

            if (client == m_sharedData.localConnectionData.connectionID)
            {
                if (m_sharedData.gimmeRadius == 1)
                {
                    Achievements::incrementStat(StatStrings[StatID::LeatherGimmies]);
                }
                else
                {
                    Achievements::incrementStat(StatStrings[StatID::PutterGimmies]);
                }

                if (!m_sharedData.connectionData[client].playerData[player].isCPU)
                {
                    m_achievementTracker.noGimmeUsed = false;
                    m_achievementTracker.gimmes++;

                    if (m_achievementTracker.gimmes == 18)
                    {
                        Achievements::awardAchievement(AchievementStrings[AchievementID::GimmeGimmeGimme]);
                    }

                    if (m_achievementTracker.nearMissChallenge)
                    {
                        Social::getMonthlyChallenge().updateChallenge(ChallengeID::Seven, 0);
                        m_achievementTracker.nearMissChallenge = false;
                    }

                    updateLeagueHole();
                }
            }
        }
        break;
        case PacketID::BallLanded:
        {
            //this *potentially* could mess with the player
            //camera - however if this is true then it should
            //never be any camera view except the correct one
            if (m_puttViewState.isPuttView)
            {
                m_puttViewState.isEnabled = true; //this *should* be disabled but we need to enable it here to override state
                togglePuttingView(false);

                if (m_activeAvatar)
                {
                    auto anim = m_activeAvatar->animationIDs[AnimationID::Celebrate];
                    auto& skel = m_activeAvatar->model.getComponent<cro::Skeleton>();

                    if (skel.getState() == cro::Skeleton::Stopped
                        || (skel.getActiveAnimations().first != anim && skel.getActiveAnimations().second != anim))
                    {
                        skel.play(anim, 1.f, 0.4f);
                    }
                }
            }


            auto update = evt.packet.as<BallUpdate>();
            switch (update.terrain)
            {
            default: break;
            case TerrainID::Bunker:
                if (m_sharedData.scoreType != ScoreType::NearestThePin) //just because the messages overlap
                {
                    showMessageBoard(MessageBoardID::Bunker);
                }
                break;
            case TerrainID::Scrub:
                showMessageBoard(MessageBoardID::Scrub);
                m_achievementTracker.hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Water:
                showMessageBoard(MessageBoardID::Water);
                m_achievementTracker.hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Hole:
                if (m_sharedData.scoreType == ScoreType::NearestThePin)
                {
                    showMessageBoard(MessageBoardID::NTPForfeit);
                    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeComplete[m_currentHole] = true;
                    //so the scoreboard can show a forfeit
                    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] = MaxNTPStrokes + 1;
                }
                else
                {
                    if (m_sharedData.gameMode == GameMode::Tutorial)
                    {
                        Achievements::setActive(true);
                        Achievements::awardAchievement(AchievementStrings[AchievementID::CluedUp]);
                        Achievements::setActive(m_allowAchievements);
                    }

                    bool special = false;

                    //check if this is our own score
                    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                    {
                        if (m_currentHole == m_holeData.size() - 1)
                        {
                            //just completed the course
                            if (m_currentHole == 17) //full round
                            {
                                auto old = Achievements::getActive();
                                Achievements::setActive(m_allowAchievements);
                                Achievements::incrementStat(StatStrings[StatID::HolesPlayed]);
                                Achievements::awardAchievement(AchievementStrings[AchievementID::JoinTheClub]);
                                Achievements::setActive(old);
                            }

                            if (m_sharedData.scoreType != ScoreType::Skins)
                            {
                                Social::awardXP(XPValues[XPID::CompleteCourse] / (18 / m_holeData.size()), XPStringID::CourseComplete);
                            }
                        }

                        //check putt distance / if this was in fact a putt
                        if (getClub() == ClubID::Putter)
                        {
                            if (glm::length(update.position - m_currentPlayer.position) > LongPuttDistance)
                            {
                                Achievements::incrementStat(StatStrings[StatID::LongPutts]);
                                Achievements::awardAchievement(AchievementStrings[AchievementID::PuttStar]);
                                Social::awardXP((XPValues[XPID::Special] / 4) * 3, XPStringID::LongPutt);
                                Social::getMonthlyChallenge().updateChallenge(ChallengeID::One, 0);
                                special = true;
                            }

                            if (!m_achievementTracker.wasGreen)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::FringeBenefit]);
                            }
                        }
                        else
                        {
                            if (getClub() > ClubID::NineIron)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::TopChip]);
                                Achievements::incrementStat(StatStrings[StatID::ChipIns]);
                                Social::awardXP(XPValues[XPID::Special], XPStringID::TopChip);
                            }

                            if (m_achievementTracker.hadBackspin)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::SpinClass]);
                                Social::awardXP((XPValues[XPID::Special] * 5) / 2, XPStringID::BackSpinSkill);
                            }
                            else if (m_achievementTracker.hadTopspin)
                            {
                                Social::awardXP(XPValues[XPID::Special] * 2, XPStringID::TopSpinSkill);
                            }

                            if (m_achievementTracker.hadPunch)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::PunchIt]);
                            }

                            if (m_achievementTracker.hadFlop)
                            {
                                Achievements::awardAchievement(AchievementStrings[AchievementID::TopOfTheFlops]);
                            }
                        }

                        //really we want to do this in any mode if holed without a gimme
                        //however there's no real way to distinguish atm
                        if (m_sharedData.gimmeRadius == 0)
                        {
                            Social::awardXP(25, XPStringID::NoGimme);
                        }
                    }

                    updateLeagueHole(); //requires current hole

                    //really we want to find a way to differentiate and show this in legit cases.
                    showMessageBoard(MessageBoardID::HoleScore, special);

                    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeComplete[m_currentHole] = true;
                }
                break;
            }

            auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::BallLanded;
            msg->terrain = update.position.y <= WaterLevel ? TerrainID::Water : update.terrain;
            msg->club = getClub();
            msg->travelDistance = glm::length2(update.position - m_currentPlayer.position);
            msg->pinDistance = glm::length2(update.position - m_holeData[m_currentHole].pin);

            //update achievements if this is local player
            if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
            {
                m_sharedData.timeStats[m_currentPlayer.player].holeTimes[m_currentHole] += m_turnTimer.elapsed().asSeconds();

                if (msg->terrain == TerrainID::Bunker)
                {
                    Achievements::incrementStat(StatStrings[StatID::SandTrapCount]);
                }
                else if (msg->terrain == TerrainID::Water)
                {
                    Achievements::incrementStat(StatStrings[StatID::WaterTrapCount]);
                }

                //this won't register if player 0 is CPU...
                if (m_currentPlayer.player == 0
                    && !m_sharedData.localConnectionData.playerData[0].isCPU)
                {
                    m_playTime += m_playTimer.elapsed();
                }

                //track other achievements awarded at the end of the round
                if (!m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
                {
                    if ((msg->terrain != TerrainID::Green
                        && msg->terrain != TerrainID::Fairway)
                        && m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole] == 1) //first stroke
                    {
                        m_achievementTracker.alwaysOnTheCourse = false;
                    }
                }

                //update profile stats for distance
                if (msg->club == ClubID::Putter
                    && msg->terrain == TerrainID::Hole)
                {
                    m_personalBests[m_currentPlayer.player][m_currentHole].longestPutt =
                        std::max(std::sqrt(msg->travelDistance), m_personalBests[m_currentPlayer.player][m_currentHole].longestPutt);

                    m_personalBests[m_currentPlayer.player][m_currentHole].wasPuttAssist = m_sharedData.showPuttingPower ? 1 : 0;
                }
                else if (msg->club < ClubID::FourIron)
                {
                    m_personalBests[m_currentPlayer.player][m_currentHole].longestDrive =
                        std::max(std::sqrt(msg->travelDistance), m_personalBests[m_currentPlayer.player][m_currentHole].longestDrive);
                }
            }

            if (m_groupIdle)
            {
                setActiveCamera(CameraID::Player);
            }
        }
        break;
        case PacketID::ClientDisconnected:
            removeClient(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Map";
                break;
            }
            requestStackPush(StateID::Error);
            break;
        case PacketID::SetPlayer:
            if (m_photoMode)
            {
                toggleFreeCam();
            }
            else
            {
                resetIdle();
            }
            m_wantsGameState = false;
            m_newHole = false; //not necessarily a new hole, but the server has said player wants to go, regardless
            {
                auto playerData = evt.packet.as<ActivePlayer>();
                createTransition(playerData);
            }
            break;
        case PacketID::ActorSpawn:
            spawnBall(evt.packet.as<ActorInfo>());
            break;
        case PacketID::ActorUpdate:
            updateActor(evt.packet.as<ActorInfo>());
            break;
        case PacketID::ActorAnimation:
        {
            if (m_activeAvatar)
            {
                auto animID = evt.packet.as<std::uint8_t>();
                if (animID == AnimationID::Celebrate)
                {
                    m_clubModels[ClubModel::Wood].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                    m_clubModels[ClubModel::Iron].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                }
                else
                {
                    m_clubModels[ClubModel::Wood].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                    m_clubModels[ClubModel::Iron].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                }

                /*if (animID == AnimationID::Swing)
                {
                    if(m_inputParser.getPower() * Clubs[getClub()].target < 20.f)
                    {
                        animID = AnimationID::Chip;
                    }
                }*/

                //TODO scale club model to zero if not idle or swing

                auto anim = m_activeAvatar->animationIDs[animID];
                auto& skel = m_activeAvatar->model.getComponent<cro::Skeleton>();

                if (skel.getState() == cro::Skeleton::Stopped
                    || (skel.getActiveAnimations().first != anim && skel.getActiveAnimations().second != anim))
                {
                    skel.play(anim, /*isCPU ? 2.f :*/ 1.f, 0.4f);
                }
            }
        }
        break;
        case PacketID::WindDirection:
            updateWindDisplay(cro::Util::Net::decompressVec3(evt.packet.as<std::array<std::int16_t, 3u>>()));
            break;
        case PacketID::SetHole:
            if (m_photoMode)
            {
                toggleFreeCam();
            }
            else
            {
                resetIdle();
            }
            setCurrentHole(evt.packet.as<std::uint16_t>());
            m_sharedData.holesPlayed++;
            break;
        case PacketID::ScoreUpdate:
        {
            auto su = evt.packet.as<ScoreUpdate>();
            auto& player = m_sharedData.connectionData[su.client].playerData[su.player];

            if (su.hole < player.holeScores.size())
            {
                player.score = su.score;
                player.matchScore = su.matchScore;
                player.skinScore = su.skinsScore;
                player.holeScores[su.hole] = su.stroke;
                player.distanceScores[su.hole] = su.distanceScore;

                if (su.client == m_sharedData.localConnectionData.connectionID)
                {
                    if (getClub() == ClubID::Putter)
                    {
                        Achievements::incrementStat(StatStrings[StatID::PuttDistance], su.strokeDistance);
                    }
                    else
                    {
                        Achievements::incrementStat(StatStrings[StatID::StrokeDistance], su.strokeDistance);
                    }

                    m_personalBests[su.player][su.hole].hole = su.hole;
                    m_personalBests[su.player][su.hole].course = m_courseIndex;
                    m_personalBests[su.player][su.hole].score = su.stroke;
                }
                updateScoreboard(false);
            }
        }
        break;
        case PacketID::HoleWon:
            updateHoleScore(evt.packet.as<std::uint16_t>());
            break;
        case PacketID::GameEnd:
            showCountdown(evt.packet.as<std::uint8_t>());
            Progress::clear(m_sharedData.leagueRoundID);

            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::ScoreTitle;
                cmd.action = [&](cro::Entity e, float)
                    {
                        auto str = m_courseTitle + " - " + ScoreTypes[m_sharedData.scoreType];
                        e.getComponent<cro::Text>().setString(str);
                        centreText(e);
                    };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
            break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Lobby)
            {
                requestStackClear();
                requestStackPush(StateID::Menu);
            }
            break;
        case PacketID::EntityRemoved:
        {
            auto idx = evt.packet.as<std::uint32_t>();
            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [&, idx](cro::Entity e, float)
                {
                    if (e.getComponent<InterpolationComponent<InterpolationType::Linear>>().id == idx)
                    {
                        //this just does some effects
                        auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
                        msg->type = GolfEvent::PlayerRemoved;
                        msg->position = e.getComponent<cro::Transform>().getWorldPosition();

                        m_gameScene.destroyEntity(e);
                        LOG("Packet removed ball entity", cro::Logger::Type::Warning);
                    }
                };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        break;
        case PacketID::ConnectionRefused:
            m_sharedData.errorMessage = "Kicked By Host";
            requestStackPush(StateID::Error);
            break;
        }
        break;
    case net::NetEvent::ClientDisconnect:
        m_sharedData.errorMessage = "Disconnected From Server";
        requestStackPush(StateID::Error);
        break;
    default: break;
    }
}

void GolfState::handleBullHit(const BullHit& bh)
{
    if (bh.client == m_sharedData.localConnectionData.connectionID)
    {
        if (!m_sharedData.localConnectionData.playerData[bh.player].isCPU)
        {
            if (!m_achievementTracker.bullseyeChallenge
                && m_sharedData.scoreType != ScoreType::MultiTarget)
            {
                Social::getMonthlyChallenge().updateChallenge(ChallengeID::Two, 0);
            }
            m_achievementTracker.bullseyeChallenge = true;

            if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
            {
                auto xp = static_cast<std::int32_t>(80.f * bh.accuracy);
                if (xp)
                {
                    Social::awardXP(xp, XPStringID::BullsEyeHit);
                }
                else
                {
                    floatingMessage("Target Hit!");
                }

                auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::TargetHit;
                msg->position = bh.position;
            }
        }
        else if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
        {
            auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::TargetHit;
            msg->position = bh.position;

            floatingMessage("Target Hit!");
        }
    }
    else if (!m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit)
    {
        auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
        msg->type = GolfEvent::TargetHit;
        msg->position = bh.position;

        floatingMessage("Target Hit!");
    }
    
    //hide the target
    cro::Command cmd;
    cmd.targetFlags = CommandID::BullsEye;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = 
                Social::getMonth() == 2 ? AnimDirection::Destroy : AnimDirection::Shrink;
            e.getComponent<cro::Callback>().active = true;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_sharedData.connectionData[bh.client].playerData[bh.player].targetHit = true;

    Timeline::addEvent(Timeline::Event::TargetHit, m_strokeTimer);
}

void GolfState::handleMaxStrokes(std::uint8_t reason)
{
    //we may have never reached the green, so we don't
    //want a forfeit contributing to the achievement...
    m_achievementTracker.twoShotsSpare = false;

    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeComplete[m_currentHole] = true;
    
    switch (reason)
    {
    default:
        switch (m_sharedData.scoreType)
        {
        default:
            showNotification("Stroke Limit Reached.");
            break;
        case ScoreType::MultiTarget:
            if (reason == MaxStrokeID::Forfeit)
            {
                showNotification("Hole Forfeit: No Target Hit.");
            }
            else
            {
                showNotification("Stroke Limit Reached.");
            }
            break;
        case ScoreType::LongestDrive: //this shouldn't ever reach here in this game mode
        case ScoreType::NearestThePin:
            //do nothing, this is integral behavior
            return;
        }

        {
            auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
            msg->type = Social::SocialEvent::PlayerAchievement;
            msg->level = 1; //sad sound
        }
        break;
    case MaxStrokeID::HostPunishment:
        showNotification("Host Penalised Player's Turn");
        break;
    case MaxStrokeID::IdleTimeout:
        showNotification("AFK Timeout");
        break;
    }
}

void GolfState::removeClient(std::uint8_t clientID)
{
    for (auto& p : m_groupPlayerPositions)
    {
        if (p.client == clientID)
        {
            p.client = 255;
        }
    }

    cro::String str = m_sharedData.connectionData[clientID].playerData[0].name;
    for (auto i = 1u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        str += ", " + m_sharedData.connectionData[clientID].playerData[i].name;
    }
    str += " left the game";

    showNotification(str);

    for (auto i = m_netStrengthIcons.size() - 1; i >= (m_netStrengthIcons.size() - m_sharedData.connectionData[clientID].playerCount); i--)
    {
        m_uiScene.destroyEntity(m_netStrengthIcons[i]);
    }
    m_netStrengthIcons.resize(m_netStrengthIcons.size() - m_sharedData.connectionData[clientID].playerCount);

    m_sharedData.connectionData[clientID].playerCount = 0;
    m_sharedData.connectionData[clientID].pingTime = 1000; //just so existing net indicators don't show green

    updateScoreboard();

    //raise a message in case other states (eg player management) needs to know
    auto* msg = postMessage<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClientDisconnect;
    msg->client = clientID;
}

void GolfState::setCurrentHole(std::uint16_t holeInfo, bool forceTransition)
{
    if (m_photoMode)
    {
        //hard reset the free cam if we were idle in group
        m_gameScene.setActiveCamera(m_defaultCam);
        m_gameScene.setActiveListener(m_defaultCam);

        m_defaultCam.getComponent<cro::Camera>().active = true;
        m_freeCam.getComponent<cro::Camera>().active = false;

        //restore fade distance - TODO what if this is putt from tee?
        m_resolutionUpdate.targetFade = CourseFadeDistance;

        //unhide UI
        setUIHidden(false);
        m_gameScene.setSystemActive<FpsCameraSystem>(false);
        m_waterEnt.getComponent<cro::Callback>().active = true;
        enableDOF(false);
        m_freecamMenuEnt.getComponent<cro::Callback>().active = false;

        for (auto i = 0; i < 4; ++i)
        {
            cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createWeapon(0, 1, 2));
        }

        Activity a;
        a.client = m_sharedData.clientConnection.connectionID;
        a.type = Activity::FreecamEnd;
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
    
    m_spectateGhost.getComponent<cro::Callback>().getUserData<GhostCallbackData>().direction = GhostCallbackData::Out;

    m_gameScene.getSystem<CameraFollowSystem>()->setTargetGroup(m_serverGroup);
    setActiveCamera(CameraID::Player);
    m_gameScene.getSystem<cro::CameraSystem>()->process(0.f);

    m_groupIdle = false;

    if (m_sharedData.scoreType == ScoreType::ClubShuffle)
    {
        m_sharedData.inputBinding.clubset = ClubID::getRandomSet();
    }

    std::uint8_t hole = (holeInfo & 0xff00) >> 8;
    m_holeData[hole].par = (holeInfo & 0x00ff);

    //mark all holes complete - this fudges any missing
    //scores on the scoreboard... we shouldn't really have to do this :(
    if (hole > m_currentHole)
    {
        for (auto& client : m_sharedData.connectionData)
        {
            for (auto& player : client.playerData)
            {
                player.holeComplete[m_currentHole] = true;
            }
        }
    }


    if (hole != m_currentHole
        && m_sharedData.logBenchmarks)
    {
        dumpBenchmark();
    }


    //if this is the final hole repeated then we're in skins sudden death
    if (m_sharedData.gameMode != GameMode::Tutorial
        && m_sharedData.scoreType == ScoreType::Skins)
    {
        //TODO this will show if we're playing a custom course with
        //only one hole in skins mode (for some reason)
        if (hole == m_currentHole
            && hole == m_holeData.size() - 1)
        {
            showNotification("Sudden Death Round!");
            showNotification("First to hole wins!");
            m_suddenDeath = true;
        }
    }


    //update all the total hole times - TODO don't do this if loading from save game
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        m_sharedData.timeStats[i].totalTime += m_sharedData.timeStats[i].holeTimes[m_currentHole];

        if (!m_resumedFromSave)
        {
            Achievements::incrementStat(StatStrings[StatID::TimeOnTheCourse], m_sharedData.timeStats[i].holeTimes[m_currentHole]);
        }
    }

    //update the look-at target in multi-target mode
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.targetHit = false;
        }
    }

    updateScoreboard();
    m_achievementTracker.hadFoul = false;// m_resumedFromSave;// false;
    m_achievementTracker.bullseyeChallenge = false; //monthly challenge is OK
    m_achievementTracker.puttCount = m_resumedFromSave ? 2 : 0;

    //can't get these when putting else it's
    //far too easy (we're technically always on the green)
    if (m_holeData[hole].puttFromTee
        || m_resumedFromSave)
    {
        m_achievementTracker.alwaysOnTheCourse = false;
        m_achievementTracker.twoShotsSpare = false;
        m_achievementTracker.under15metres = false;
    }

    //CRO_ASSERT(hole < m_holeData.size(), "");
    if (hole >= m_holeData.size())
    {
        m_sharedData.errorMessage = "Server requested hole\nnot found";
        requestStackPush(StateID::Error);
        return;
    }

    m_terrainBuilder.update(hole, forceTransition);
    m_gameScene.getSystem<ClientCollisionSystem>()->setMap(hole);
    m_gameScene.getSystem<ClientCollisionSystem>()->setPinPosition(m_holeData[hole].pin);
    m_collisionMesh.updateCollisionMesh(m_holeData[hole].modelEntity.getComponent<cro::Model>().getMeshData());

    //set the min tree height of the culling system based on the hole model's AABB
    const float height = m_holeData[hole].modelEntity.getComponent<cro::Model>().getAABB().getSize().y + 15.f;
    m_gameScene.getSystem<ChunkVisSystem>()->setWorldHeight(height);

    //create hole model transition
    bool rescale = (hole == 0) || (m_holeData[hole - 1].modelPath != m_holeData[hole].modelPath) || forceTransition;
    auto* propModels = &m_holeData[m_currentHole].propEntities;
    auto* particles = &m_holeData[m_currentHole].particleEntities;
    auto* audio = &m_holeData[m_currentHole].audioEntities;
    auto* lights = &m_holeData[m_currentHole].lights;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().active = true;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().function =
        [&, propModels, particles, audio, lights, rescale](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + (dt / 2.f));

        if (rescale)
        {
            float scale = 1.f - progress;
            e.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
            m_waterEnt.getComponent<cro::Transform>().setScale({ scale, scale, scale });
        }

        if (progress == 1) //fully scaled to zero
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(rescale);

            for (auto i = 0u; i < propModels->size(); ++i)
            {
                //if we're not rescaling we're recycling the model so don't hide its props
                propModels->at(i).getComponent<cro::Model>().setHidden(rescale);
            }

            
            
            //index should be updated by now (as this is a callback)
            //so we're actually targetting the next hole entity
            auto entity = m_holeData[m_currentHole].modelEntity;
            entity.getComponent<cro::Model>().setHidden(false);            
            
            if (rescale)
            {
                for (auto i = 0u; i < particles->size(); ++i)
                {
                    particles->at(i).getComponent<cro::ParticleEmitter>().stop();
                }
                //should already be started otherwise...

                for (auto i = 0u; i < audio->size(); ++i)
                {
                    audio->at(i).getComponent<cro::AudioEmitter>().stop();
                }

                for (auto spectator : m_spectatorModels)
                {
                    spectator.getComponent<cro::Model>().setHidden(true);
                    spectator.getComponent<Spectator>().path = nullptr;
                    spectator.getComponent<cro::Skeleton>().stop();
                }

                entity.getComponent<cro::Transform>().setScale({ 0.f, 1.f, 0.f });

                if (m_lightVolumeDefinition.isLoaded())
                {
                    for (const auto& lightData : m_holeData[m_currentHole].lightData)
                    {
                        //create a light entity and parent it to the hole model
                        //these will already exist if we're not rescaling
                        auto lightEnt = m_gameScene.createEntity();
                        lightEnt.addComponent<cro::Transform>().setPosition(lightData.position);
                        lightEnt.getComponent<cro::Transform>().setScale(glm::vec3(lightData.radius));
                        lightEnt.addComponent<cro::LightVolume>().colour = lightData.colour;
                        lightEnt.getComponent<cro::LightVolume>().radius = lightData.radius;
                        lightEnt.getComponent<cro::LightVolume>().maxVisibilityDistance = static_cast<float>((MapSize.x / 2) * (MapSize.x / 2)); //this is sqr!!
                        //add any light animation property
                        if (!lightData.animation.empty())
                        {
                            lightEnt.addComponent<LightAnimation>().setPattern(lightData.animation);
                        }

                        m_lightVolumeDefinition.createModel(lightEnt);
                        lightEnt.getComponent<cro::Model>().setHidden(true);
                        entity.getComponent<cro::Transform>().addChild(lightEnt.getComponent<cro::Transform>());
                    }
                }
            }
            else
            {
                entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f, 1.f });
            }
            entity.getComponent<cro::Callback>().setUserData<float>(0.f);
            entity.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, rescale](cro::Entity ent, float dt)
            {
                auto& progress = ent.getComponent<cro::Callback>().getUserData<float>();
                progress = std::min(1.f, progress + (dt / 2.f));

                if (rescale)
                {
                    float scale = progress;
                    ent.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
                    m_waterEnt.getComponent<cro::Transform>().setScale({ scale, scale, scale });
                }

                if (progress == 1)
                {
                    ent.getComponent<cro::Callback>().active = false;
                    updateMiniMap();
                }
            };

            //unhide any prop models
            for (auto prop : m_holeData[m_currentHole].propEntities)
            {
                prop.getComponent<cro::Model>().setHidden(false);

                if (prop.hasComponent<cro::Skeleton>()
                    //&& !prop.getComponent<cro::Skeleton>().getAnimations().empty())
                    && prop.getComponent<cro::Skeleton>().getAnimations().size() == 1)
                {
                    //this is a bit kludgy without animation index mapping - 
                    //basically props with one anim are idle, others might be
                    //specialised like clapping, which we don't want to trigger
                    //on a new hole
                    //if (prop.getComponent<cro::Skeleton>().getAnimations().size() == 1)
                    {
                        prop.getComponent<cro::Skeleton>().play(0);
                    }
                }
            }

            for (auto particle : m_holeData[m_currentHole].particleEntities)
            {
                particle.getComponent<cro::ParticleEmitter>().start();
            }

            for (auto audio : m_holeData[m_currentHole].audioEntities)
            {
                audio.getComponent<cro::AudioEmitter>().play();
                audio.getComponent<cro::Callback>().active = true;
            }

            //check hole for any crowd paths and assign any free
            //spectator models we have
            if (rescale &&
                !m_holeData[m_currentHole].crowdCurves.empty())
            {
                auto modelsPerPath = std::min(std::size_t(6), m_spectatorModels.size() / m_holeData[m_currentHole].crowdCurves.size());
                std::size_t assignedModels = 0;
                for (const auto& curve : m_holeData[m_currentHole].crowdCurves)
                {
                    for (auto i = 0u; i < modelsPerPath && assignedModels < m_spectatorModels.size(); i++, assignedModels++)
                    {
                        auto model = m_spectatorModels[assignedModels];
                        model.getComponent<cro::Model>().setHidden(false);
                        
                        auto& spectator = model.getComponent<Spectator>();
                        spectator.path = &curve;
                        spectator.target = i % curve.getPoints().size();
                        spectator.stateTime = 0.f;
                        spectator.state = Spectator::State::Pause;
                        spectator.direction = spectator.target < (curve.getPoints().size() / 2) ? -1 : 1;
                        //spectator.walkSpeed = 1.f;// +cro::Util::Random::value(-0.1f, 0.15f);

                        model.getComponent<cro::Skeleton>().play(spectator.anims[Spectator::AnimID::Idle]);
#ifdef CRO_DEBUG_
                        model.getComponent<cro::Skeleton>().setInterpolationEnabled(false);
                        model.getComponent<cro::ShadowCaster>().active = false;
#endif
                        model.getComponent<cro::Transform>().setPosition(curve.getPoint(spectator.target));
                        m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().addChild(model.getComponent<cro::Transform>());
                    }
                }
            }

            m_gameScene.getSystem<SpectatorSystem>()->updateSpectatorGroups();


            if (entity != e)
            {
                //new hole model so remove old props
                for (auto i = 0u; i < propModels->size(); ++i)
                {
                    m_gameScene.destroyEntity(propModels->at(i));
                }
                propModels->clear();

                for (auto i = 0u; i < particles->size(); ++i)
                {
                    m_gameScene.destroyEntity(particles->at(i));
                }
                particles->clear();
                
                for (auto i = 0u; i < audio->size(); ++i)
                {
                    //m_gameScene.destroyEntity(audio->at(i));
                    audio->at(i).getComponent<cro::Callback>().function =
                        [&](cro::Entity e, float dt)
                    {
                        //fade out then remove oneself
                        auto vol = e.getComponent<cro::AudioEmitter>().getVolume();
                        vol = std::max(0.f, vol - dt);
                        e.getComponent<cro::AudioEmitter>().setVolume(vol);

                        if (vol == 0)
                        {
                            e.getComponent<cro::AudioEmitter>().stop();
                            e.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(e);
                        }
                    };
                    audio->at(i).getComponent<cro::Callback>().active = true;
                }
                audio->clear();

                for (auto i = 0u; i < lights->size(); ++i)
                {
                    m_gameScene.destroyEntity(lights->at(i));
                }
                lights->clear();
            }
        }
    };

    m_currentHole = hole;
    startFlyBy(); //requires current hole

    //restore the drone if someone hit it
    if (!m_drone.isValid())
    {
        createDrone();
    }


    //map collision data
    m_currentMap.loadFromFile(m_holeData[m_currentHole].mapPath);

    //make sure we have the correct target position
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetHeight = m_holeData[m_currentHole].puttFromTee ? CameraPuttHeight : CameraStrokeHeight;
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetOffset = m_holeData[m_currentHole].puttFromTee ? CameraPuttOffset : CameraStrokeOffset;
    m_cameras[CameraID::Player].getComponent<TargetInfo>().targetLookAt = m_holeData[m_currentHole].target;

    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point the ent destroys itself - also interps the position of the tee/flag
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_currentPlayer.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<glm::vec3>(m_currentPlayer.position);
    entity.getComponent<cro::Callback>().function =
        [&, hole](cro::Entity e, float dt)
    {
        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = m_holeData[m_currentHole].tee - currPos;

        auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();

        //if we're moving on to any other than the first hole, interp the
        //tee and hole position based on how close to the tee the camera is
        float percent = 1.f;
        if (hole > 0)
        {
            auto startPos = e.getComponent<cro::Callback>().getUserData<glm::vec3>();
            auto totalDist = glm::length(m_holeData[m_currentHole].tee - startPos);
            auto currentDist = glm::length(travel);

            percent = (totalDist - currentDist) / totalDist;
            percent = std::min(1.f, std::max(0.f, percent));

            targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

            auto pinMove = m_holeData[m_currentHole].pin - m_holeData[m_currentHole - 1].pin;
            auto pinPos = m_holeData[m_currentHole - 1].pin + (pinMove * percent);

            cro::Command cmd;
            cmd.targetFlags = CommandID::Hole;
            cmd.action = [pinPos](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(pinPos);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //updates the height shaders on the greens
            for (auto& s : m_gridShaders)
            {
                glUseProgram(s.shaderID);
                glUniform1f(s.holeHeight, pinPos.y);
            }

            auto teeMove = m_holeData[m_currentHole].tee - m_holeData[m_currentHole - 1].tee;
            auto teePos = m_holeData[m_currentHole - 1].tee + (teeMove * percent);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&, teePos](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(teePos);

                auto pinDir = m_holeData[m_currentHole].target - teePos;
                auto rotation = std::atan2(-pinDir.z, pinDir.x);
                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            auto targetDir = m_holeData[m_currentHole].target - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            //randomise the cart positions a bit
            cmd.targetFlags = CommandID::Cart;
            cmd.action = [&](cro::Entity e, float)
            {
                //move to ground level
                auto pos = e.getComponent<cro::Transform>().getWorldPosition();
                auto result = m_collisionMesh.getTerrain(pos);
                float diff = result.height - pos.y;

                e.getComponent<cro::Transform>().move({ 0.f, diff, 0.f });

                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);

                //and orientate to slope
                /*auto r = glm::rotation(cro::Transform::Y_AXIS, result.normal);
                e.getComponent<cro::Transform>().setRotation(r);
                e.getComponent<cro::Transform>().rotate(glm::inverse(r) * cro::Transform::Y_AXIS, cro::Util::Const::PI);*/
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
        else
        {
            auto targetDir = m_holeData[m_currentHole].target - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);
        }


        if (glm::length2(travel) < 0.0001f)
        {
            float height = m_holeData[m_currentHole].pin.y;
            for (auto& s : m_gridShaders)
            {
                glUseProgram(s.shaderID);
                glUniform1f(s.holeHeight, height);
            }

            targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
            targetInfo.startHeight = targetInfo.targetHeight;
            targetInfo.startOffset = targetInfo.targetOffset;

            auto targetDir = m_holeData[m_currentHole].target - m_holeData[m_currentHole].tee;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            //we're there
            setCameraPosition(m_holeData[m_currentHole].tee, targetInfo.targetHeight, targetInfo.targetOffset);

            //set tee / flag
            cro::Command cmd;
            cmd.targetFlags = CommandID::Hole;
            cmd.action = [&](cro::Entity en, float)
            {
                auto pos = m_holeData[m_currentHole].pin;
                pos.y += 0.0001f;

                en.getComponent<cro::Transform>().setPosition(pos);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
                en.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //remove the transition ent
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            auto height = targetInfo.targetHeight - targetInfo.startHeight;
            auto offset = targetInfo.targetOffset - targetInfo.startOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(),
                targetInfo.startHeight + (height * percent),
                targetInfo.startOffset + (offset * percent));
        }
    };

    m_currentPlayer.position = m_holeData[m_currentHole].tee;


    m_inputParser.setHoleDirection(m_holeData[m_currentHole].target - m_currentPlayer.position);
    m_currentPlayer.terrain = m_holeData[m_currentHole].puttFromTee ? TerrainID::Green : TerrainID::Fairway; //this will be overwritten from the server but setting this to non-green makes sure the mini cam stops updating in time
    m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin, true); //limits club selection based on hole size

    //hide the slope indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [](cro::Entity e, float)
    {
        //e.getComponent<cro::Model>().setHidden(true);
        e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    m_terrainBuilder.setSlopePosition(m_holeData[m_currentHole].pin);

    //update the UI
    cmd.targetFlags = CommandID::UI::HoleNumber;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto holeNumber = holeNumberFromIndex();

        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = "Hole: " + std::to_string(holeNumber);
        e.getComponent<cro::Callback>().active = true;

        //while we're here set the screenshot location, save recalculating the hole number
        const auto location = data.string + ", " + m_courseTitle;
        m_gameScene.getSystem<FpsCameraSystem>()->setLocation(location);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //hide the overhead view
    cmd.targetFlags = CommandID::UI::MiniGreen;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide current terrain
    cmd.targetFlags = CommandID::UI::TerrainType;
    cmd.action =
        [](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(" ");
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //tell the tee and its children to scale depending on if they should be visible
    cmd.targetFlags = CommandID::Tee;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().setUserData<float>(m_holeData[m_currentHole].puttFromTee ? 0.f : 1.f);
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //set green cam position
    auto holePos = m_holeData[m_currentHole].pin;
    m_greenCam.getComponent<cro::Transform>().setPosition({ holePos.x, holePos.y + 0.5f, holePos.z });


    //and the uh, other green cam. The spectator one
    setGreenCamPosition();


    //reset the flag
    cmd.targetFlags = CommandID::Flag;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<FlagCallbackData>().targetHeight = 0.f;
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //update status
    const std::size_t MaxTitleLen = 220;

    cro::String courseTitle = m_courseTitle.substr(0, MaxTitleLen) + " ";
    if (m_sharedData.nightTime)
    {
        //moon
        courseTitle += cro::String(std::uint32_t(0x1F319));
    }
    else
    {
        //sun
        courseTitle += cro::String(std::uint32_t(0x2600));
        courseTitle += cro::String(std::uint32_t(0xFE0F));
    }

    switch (m_sharedData.weatherType)
    {
    default:
        //don't add anything if the weather is clear
        break;
    case WeatherType::Rain:
        //rain cloud
        courseTitle += std::uint32_t(0x1F327);
        courseTitle += std::uint32_t(0xFE0F);
        break;
    case WeatherType::Showers:
        if (m_sharedData.nightTime)
        {
            //umbrella
            courseTitle += std::uint32_t(0x2614);
        }
        else
        {
            //rainbow
            courseTitle += std::uint32_t(0x1F308);
        }
        break;
    case WeatherType::Mist:
        //fog
        courseTitle += std::uint32_t(0x1F32B);
        courseTitle += std::uint32_t(0xFE0F);
        break;
    }

    const auto title = m_sharedData.gameMode == GameMode::Tutorial ? cro::String("Tutorial").toUtf8() : courseTitle.toUtf8();
    const auto holeNumber = std::to_string(m_currentHole + 1);
    const auto holeTotal = std::to_string(m_holeData.size());
    //well... this is awful.
    Social::setStatus(Social::InfoID::Course, { reinterpret_cast<const char*>(title.c_str()), holeNumber.c_str(), holeTotal.c_str() });


    //cue up next depth map
    auto next = m_currentHole + 1;
    if (next < m_holeData.size())
    {
        m_depthMap.setModel(m_holeData[next]);
    }
    else
    {
        m_depthMap.forceSwap(); //make sure we're reading the correct texture anyway
    }
    m_waterEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_depthMap", m_depthMap.getTexture());

    m_sharedData.minimapData.teePos = m_holeData[m_currentHole].tee;
    m_sharedData.minimapData.pinPos = m_holeData[m_currentHole].pin;
    //m_sharedData.minimapData.holeNumber = m_currentHole;
    
    if (m_sharedData.reverseCourse)
    {
        if (m_sharedData.holeCount == 0)
        {
            m_sharedData.minimapData.holeNumber = 17 - m_sharedData.minimapData.holeNumber;
        }
        else
        {
            m_sharedData.minimapData.holeNumber = 8 - m_sharedData.minimapData.holeNumber;
        }
    }
    if (m_sharedData.holeCount == 2)
    {
        m_sharedData.minimapData.holeNumber += 9;
    }


    m_sharedData.minimapData.courseName = m_courseTitle;
    m_sharedData.minimapData.courseName += "\n";
#ifdef USE_GNS
    m_sharedData.minimapData.courseName += Social::getLeader(m_sharedData.mapDirectory, m_sharedData.holeCount);
#endif
    std::int32_t offset = m_sharedData.reverseCourse ? 0 : 2;
    m_sharedData.minimapData.courseName += "\nHole: " + std::to_string(m_sharedData.minimapData.holeNumber + offset); //this isn't updated until the map texture is 
    m_sharedData.minimapData.courseName += "\nPar: " + std::to_string(m_holeData[m_currentHole].par);
    m_gameScene.getDirector<GolfSoundDirector>()->setCrowdPositions(m_holeData[m_currentHole].crowdPositions[m_sharedData.crowdDensity]);


    if (m_sharedData.leagueRoundID != LeagueRoundID::Club
        && (m_currentHole % 9) == 0)
    {
        m_mulliganCount = 1;
    }
}

std::uint32_t GolfState::holeNumberFromIndex() const
{
    auto holeNumber = m_currentHole + 1;
    if (m_sharedData.reverseCourse)
    {
        holeNumber = static_cast<std::uint32_t>(m_holeData.size() + 1) - holeNumber;

        if (m_sharedData.scoreType == ScoreType::ShortRound)
        {
            switch (m_sharedData.holeCount)
            {
            default:
            case 0:
                holeNumber += 6;
                break;
            case 1:
            case 2:
                holeNumber += 3;
                break;
            }
        }
    }

    if (m_sharedData.holeCount == 2)
    {
        if (m_sharedData.scoreType == ScoreType::ShortRound
            && m_courseIndex != -1)
        {
            holeNumber += 9;
        }
        else
        {
            holeNumber += static_cast<std::uint32_t>(m_holeData.size());
        }
    }

    return holeNumber;
}

void GolfState::requestNextPlayer(const ActivePlayer& player)
{
    if (m_sharedData.gameMode != GameMode::Tutorial)
    {
        m_currentPlayer = player;

        setActiveCamera(CameraID::Player);
        showMessageBoard(MessageBoardID::PlayerName);
        showScoreboard(false);
    }
    else
    {
        Club::setClubLevel(0); //always use the default set for the tutorial
        setCurrentPlayer(player);
    }

    //raise message for particles
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->position = player.position;
    msg->type = GolfEvent::RequestNewPlayer;
    msg->terrain = player.terrain;
}

void GolfState::setCurrentPlayer(const ActivePlayer& player)
{
#ifdef USE_GNS
    cro::String timelineDesc = m_sharedData.connectionData[player.client].playerData[player.player].name;
    if (!m_courseTitle.empty())
    {
        timelineDesc += " " + m_courseTitle;
    }
    timelineDesc += " - Hole " + std::to_string(holeNumberFromIndex()); //ugh we need to do all the shenanigans for converting to hole number when playing in reverse etc

    if (m_sharedData.connectionData[player.client].playerData[player.player].holeScores[m_currentHole] == 0)
    {
        timelineDesc += ", at the tee";
    }
    else
    {
        switch (player.terrain)
        {
        default: break;
        case TerrainID::Rough:
            timelineDesc += ", in the rough";
            break;
        case TerrainID::Fairway:
            timelineDesc += ", on the fairway";
            break;
        case TerrainID::Green:
            timelineDesc += ", on the green";
            break;
        case TerrainID::Bunker:
            timelineDesc += ", in the bunker";
            break;
        }
    }
    timelineDesc += " (" + ScoreTypes[m_sharedData.scoreType] + ")";
    Timeline::setTimelineDesc(timelineDesc);
#endif
    m_buttonStates = {};
    m_gameScene.getSystem<PropFollowSystem>()->setPlayerPosition(player.position);

    cro::App::getWindow().setMouseCaptured(true);
    m_achievementTracker.hadBackspin = false;
    m_achievementTracker.hadTopspin = false;
    m_achievementTracker.hadFlop = false;
    m_achievementTracker.hadPunch = false;
    m_achievementTracker.wasGreen = player.terrain == TerrainID::Green;
    m_achievementTracker.nearMissChallenge = false;
    m_turnTimer.restart();
    m_idleTimer.restart();
    m_playTimer.restart();
    m_idleTime = cro::seconds(90.f);
    m_skipState = {};
    m_ballTrail.setNext();
    m_strokeDistanceEnt.getComponent<cro::Text>().setString(" ");

    //close any remaining icons
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::ThinkBubble;
    cmd.action = [](cro::Entity e, float)
        {
            auto& [dir, _1, _2] = e.getComponent<cro::Callback>().getUserData<CogitationData>();
            dir = 1;
            e.getComponent<cro::Callback>().active = true;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);
    auto isCPU = m_sharedData.connectionData[player.client].playerData[player.player].isCPU;

    m_uiScene.getSystem<MiniBallSystem>()->setActivePlayer(player.client, player.player);
    m_gameScene.getDirector<GolfSoundDirector>()->setActivePlayer(player.client, player.player, isCPU && m_sharedData.fastCPU);
    m_avatars[player.client][player.player].ballModel.getComponent<cro::Transform>().setScale(glm::vec3(1.f));

    m_resolutionUpdate.targetFade = player.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

    updateScoreboard(false);
    showScoreboard(false);

    Club::setClubLevel(isCPU ? m_sharedData.clubLimit ? m_sharedData.clubSet : m_cpuGolfer.getClubLevel() : m_sharedData.clubSet); //do this first else setActive has the wrong estimation distance
    auto lie = m_avatars[player.client][player.player].ballModel.getComponent<ClientCollider>().lie;

    m_puttViewState.isEnabled = true;
    m_sharedData.inputBinding.playerID = localPlayer ? player.player : 0; //this also affects who can emote, so if we're currently emoting when it's not our turn always be player 0(??)
    m_inputParser.setActive(localPlayer && !m_photoMode, m_currentPlayer.terrain, isCPU, lie);
    m_inputParser.setDistanceToHole(glm::length(m_holeData[m_currentHole].pin - player.position));
    m_restoreInput = localPlayer; //if we're in photo mode should we restore input parser?
    Achievements::setActive(localPlayer && !isCPU && m_allowAchievements);

    if (player.terrain == TerrainID::Bunker)
    {
        auto clubID = lie == 0 ? ClubID::SevenIron : ClubID::FourIron;
        m_inputParser.setMaxClub(clubID);
    }
    else
    {
        m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin, glm::length2(player.position - m_holeData[m_currentHole].tee) < 1.f);
    }

    //player UI name
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name;
        data.client = m_currentPlayer.client;
        data.player = m_currentPlayer.player;
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    /*cmd.targetFlags = CommandID::UI::PlayerIcon;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);*/

    cmd.targetFlags = CommandID::UI::PinDistance;
    cmd.action =
        [&](cro::Entity e, float)
    {
        formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, m_sharedData.decimateDistance);

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    if (player.terrain != TerrainID::Green)
    {
        cmd.targetFlags = CommandID::UI::PinHeight;
        cmd.action =
            [&, player](cro::Entity e, float)
            {
                const auto holeDiff = (m_holeData[m_currentHole].pin.y - player.position.y) * 10.f;

                std::stringstream ss;
                ss << "(";
                if (holeDiff > 0)
                {
                    ss << "+";
                }
                ss.precision(2);
                ss << holeDiff << "%)";

                //up & down arrows - look a bit pants quality though
                //auto up = cro::String(std::uint32_t(0x2B06));
                //up += std::uint32_t(0xFE0F);
                // //OR std::uint32_t(0xA71B)
                //auto down = cro::String(std::uint32_t(0x2B07));
                //down += std::uint32_t(0xFE0F);
                // //OR std::uint32_t(0xA71C)

                e.getComponent<cro::Text>().setString(ss.str());
                e.getComponent<cro::Callback>().active = true;
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    }

    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action =
        [&,player](cro::Entity e, float)
    {
        auto pid = ((player.client * ConstVal::MaxPlayers) + player.player) + 1;

        if (e.getComponent<MiniBall>().playerID == pid)
        {
            //play the callback animation
            e.getComponent<MiniBall>().state = MiniBall::Animating;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //display current terrain
    cmd.targetFlags = CommandID::UI::TerrainType;
    cmd.action =
        [&,player, lie](cro::Entity e, float)
    {
        if (player.terrain == TerrainID::Bunker)
        {
            static const std::array<std::string, 2u> str = { u8"Bunker ↓", u8"Bunker ↑" };
            e.getComponent<cro::Text>().setString(cro::String::fromUtf8(str[lie].begin(), str[lie].end()));
        }
        else
        {
            e.getComponent<cro::Text>().setString(TerrainStrings[player.terrain]);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show ui if this is our client    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [&,localPlayer, isCPU](cro::Entity e, float)
    {
        //only show CPU power to beginners
        std::int32_t show = localPlayer && (!isCPU || Social::getLevel() < 10) ? 0 : 1;

        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = show;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //reset any warning
    cmd.targetFlags = CommandID::UI::AFKWarn;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            e.getComponent<cro::Callback>().active = false;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //stroke indicator is in model scene...
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [&,localPlayer, player](cro::Entity e, float)
    {
        auto position = player.position;
        position.y += 0.014f; //z-fighting
        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[player.player].isCPU));
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_inputParser.getYaw());
        e.getComponent<cro::Callback>().active = localPlayer;
        e.getComponent<cro::Model>().setDepthTestEnabled(0, /*player.terrain == TerrainID::Green*/false);

        e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);

        //fudgy way of changing the render type when putting
        if (e.getComponent<cro::CommandTarget>().ID & CommandID::StrokeIndicator)
        {
            if (player.terrain == TerrainID::Green)
            {
                e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_TRIANGLES;
            }
            else
            {
                e.getComponent<cro::Model>().getMeshData().indexData[0].primitiveType = GL_LINE_STRIP;
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //if client is ours activate input/set initial stroke direction
    auto target = m_cameras[CameraID::Player].getComponent<TargetInfo>().targetLookAt;
    m_inputParser.resetPower();
    m_inputParser.setHoleDirection(target - player.position);
    
    //we can set the CPU skill extra wide without worrying about
    //rotating the player model as the aim is hidden anyway
    auto rotation = isCPU ? m_cpuGolfer.getSkillIndex() > 2 ? cro::Util::Const::PI : MaxRotation / 3.f : MaxRotation;
    m_inputParser.setMaxRotation(m_holeData[m_currentHole].puttFromTee ? MaxPuttRotation : 
        player.terrain == TerrainID::Green ? rotation / 3.f : rotation);

    auto midTarget = findTargetPos(player.position);

    //set this separately because target might not necessarily be the pin.
    bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
        && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);
    auto clubTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
    m_inputParser.setClub(glm::length(clubTarget - player.position));


    cmd.targetFlags = CommandID::BullsEye;
    cmd.action = [&,isMultiTarget](cro::Entity e, float)
        {
            bool multiTarget = isMultiTarget || Social::getMonth() == 2; //challenge month
            e.getComponent<cro::Callback>().getUserData<BullsEyeData>().direction = multiTarget ? AnimDirection::Grow : AnimDirection::Shrink;
            e.getComponent<cro::Callback>().active = true;
        };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //check if input is CPU
    if (localPlayer
        && isCPU)
    {
        auto pft = m_holeData[m_currentHole].puttFromTee;

        const auto getTargetOffset = [&](glm::vec3 t)
            {
                if (m_sharedData.scoreType == ScoreType::NearestThePin)
                {
                    //for some reason this stops the CPU just holing out all the time...
                    pft = true;

                    auto o = glm::normalize(player.position - t) * (0.12f + (0.26f * std::min(1.f, m_distanceToHole / 5.f)));
                    auto temp = o.x;
                    o.x = -o.z;
                    o.z = temp;
                    o *= (cro::Util::Random::value(0, 1) == 0) ? -1.f : 1.f;
                    return o;
                }
                return glm::vec3(0.f);
            };

        const auto cTarg = clubTarget + getTargetOffset(clubTarget);
        const auto mTarg = midTarget + getTargetOffset(midTarget);

        //if the CPU is smart enough always go for the hole if we can
        if (m_cpuGolfer.getSkillIndex() > 3)
        {
            //fallback is used when repeatedly launching the ball into the woods...
            m_cpuGolfer.activate(cTarg, mTarg, pft);
        }

        else
        {
            //if the player can rotate enough prefer the hole as the target
            auto pin = cTarg - player.position;
            auto targetPoint = (target + getTargetOffset(target)) - player.position;

            auto p = glm::normalize(glm::vec2(pin.x, -pin.z));
            auto t = glm::normalize(glm::vec2(targetPoint.x, -targetPoint.z));

            float dot = glm::dot(p, t);
            float det = (p.x * t.y) - (p.y * t.x);
            float targetAngle = std::abs(std::atan2(det, dot));

            if (targetAngle < m_inputParser.getMaxRotation())
            {
                m_cpuGolfer.activate(cTarg, mTarg, pft);
            }
            else
            {
                //aim for whichever is closer (target or pin)
                //if (glm::length2(target - player.position) < glm::length2(m_holeData[m_currentHole].pin - player.position))
                {
                    m_cpuGolfer.activate(target + getTargetOffset(target), mTarg, pft);
                }
                /*else
                {
                    m_cpuGolfer.activate(m_holeData[m_currentHole].pin, target, m_holeData[m_currentHole].puttFromTee);
                }*/
            }
#ifdef CRO_DEBUG_
            //CPUTarget.getComponent<cro::Transform>().setPosition(m_cpuGolfer.getTarget());
#endif
        }
    }


    //this just makes sure to update the direction indicator
    //regardless of whether or not we actually switched clubs
    //it's a hack where above case tells the input parser not to update the club (because we're the same player)
    //but we've also landed on th green and therefor auto-switched to a putter
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;


    //show the new player model
    m_activeAvatar = &m_avatars[m_currentPlayer.client][m_currentPlayer.player];
    m_activeAvatar->model.getComponent<cro::Transform>().setPosition(player.position);
    if (player.terrain != TerrainID::Green)
    {
        auto playerRotation = m_camRotation - (cro::Util::Const::PI / 2.f);

        m_activeAvatar->model.getComponent<cro::Model>().setHidden(false);
        m_activeAvatar->model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, playerRotation);
        m_activeAvatar->model.getComponent<AvatarRotation>().currentRotation = playerRotation;
        m_activeAvatar->model.getComponent<AvatarRotation>().targetRotation = playerRotation;
        m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 0;
        m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().scale = 0.f;
        m_activeAvatar->model.getComponent<cro::Callback>().active = true;

        if (m_activeAvatar->hands)
        {
            if (getClub() < ClubID::FiveIron)
            {
                m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Wood]);
            }
            else
            {
                m_activeAvatar->hands->setModel(m_clubModels[ClubModel::Iron]);
            }
            m_activeAvatar->hands->getModel().getComponent<cro::Model>().setFacing(m_activeAvatar->model.getComponent<cro::Model>().getFacing());
        }

        //set just far enough the flag shows in the distance
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowFarDistance);


        //update the position of the bystander camera
        //make sure to reset any zoom
        auto& zoomData = m_cameras[CameraID::Bystander].getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>();
        zoomData.progress = 0.f;
        zoomData.fov = 1.f;
        m_cameras[CameraID::Bystander].getComponent<cro::Camera>().resizeCallback(m_cameras[CameraID::Bystander].getComponent<cro::Camera>());

        //then set new position
        auto eye = CameraBystanderOffset;
        if (m_activeAvatar->model.getComponent<cro::Model>().getFacing() == cro::Model::Facing::Back)
        {
            eye.x *= -1.f;
        }
        eye = glm::rotateY(eye, playerRotation);
        eye += player.position;

        auto result = m_collisionMesh.getTerrain(eye);
        auto terrainHeight = result.height + CameraBystanderOffset.y;
        if (terrainHeight > eye.y)
        {
            eye.y = terrainHeight;
        }

        auto target = player.position;
        target.y += 1.f;

        m_cameras[CameraID::Bystander].getComponent<cro::Transform>().setRotation(lookRotation(eye, target));
        m_cameras[CameraID::Bystander].getComponent<cro::Transform>().setPosition(eye);

        //if this is a CPU player or a remote player, show a bystander cam automatically
        if (player.client != m_sharedData.localConnectionData.connectionID
            || 
            (player.client == m_sharedData.localConnectionData.connectionID &&
            m_sharedData.localConnectionData.playerData[player.player].isCPU &&
                !m_sharedData.fastCPU))
        {
            static constexpr float MinCamDist = 25.f;
            if (cro::Util::Random::value(0,2) != 0 &&
                glm::length2(player.position - m_holeData[m_currentHole].pin) > (MinCamDist * MinCamDist))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(2.7f);
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                    currTime -= dt;
                    if (currTime < 0)
                    {
                        setActiveCamera(CameraID::Bystander);

                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                };
            }
        }
    }
    else
    {
        m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

        //and use closer shadow mapping
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(m_shadowQuality.shadowNearDistance);
    }
    setActiveCamera(CameraID::Player);

    //show or hide the slope indicator depending if we're on the green
    //or if we're on a putting map (in which case we're using the contour material)
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [&,player](cro::Entity e, float)
    {
        bool hidden = ((player.terrain != TerrainID::Green) /*&& m_distanceToHole > Clubs[ClubID::GapWedge].getBaseTarget()*/) || m_holeData[m_currentHole].puttFromTee;

        if (!hidden)
        {
            e.getComponent<cro::Model>().setHidden(hidden);
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            e.getComponent<cro::Callback>().active = true;
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //also check if we need to display mini map for green
    cmd.targetFlags = CommandID::UI::MiniGreen;
    cmd.action = [&, player](cro::Entity e, float)
    {
        bool hidden = (player.terrain != TerrainID::Green);

        if (!hidden)
        {
            e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 0;
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            m_greenCam.getComponent<cro::Camera>().active = true;
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<GreenCallbackData>().state = 1;
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    m_currentPlayer = player;

    //announce player has changed
    auto* msg2 = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg2->position = m_currentPlayer.position;
    msg2->terrain = m_currentPlayer.terrain;
    msg2->type = GolfEvent::SetNewPlayer;

    m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::NewPlayer, 0, net::NetFlag::Reliable, ConstVal::NetChannelReliable);


    //this is just so that the particle director knows if we're on a new hole
    if (glm::length2(m_currentPlayer.position - m_holeData[m_currentHole].tee) < (0.05f * 0.05f))
    {
        msg2->travelDistance = -1.f;
    }

    m_gameScene.setSystemActive<CameraFollowSystem>(false);

    const auto setCamTarget = [&](glm::vec3 pos)
    {
        if (m_drone.isValid())
        {
            auto t = m_collisionMesh.getTerrain(pos);
            if (pos.y - t.height < MinDroneHeight)
            {
                pos.y = t.height + MinDroneHeight;
            }

            auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
            data.target.getComponent<cro::Transform>().setPosition(pos);
            data.resetPosition = pos;
        }
        else
        {
            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(pos);
        }
    };

    //see where the player is and move the sky cam if possible
    //else set it to the default position
    static constexpr float MinPlayerDist = 40.f * 40.f; //TODO should this be the sky cam radius^2?
    auto dir = /*m_holeData[m_currentHole].pin*/target - player.position;
    if (auto len2 = glm::length2(dir); len2 > MinPlayerDist)
    {
        static constexpr float MaxHeightMultiplier = static_cast<float>(MapSize.x) * 0.8f; //probably should be diagonal but meh
        
        auto camHeight = SkyCamHeight * std::min(1.2f, (std::sqrt(len2) / MaxHeightMultiplier));
        dir /= 2.f;
        dir.y = camHeight;

        dir += player.position;

        /*auto centre = glm::vec3(static_cast<float>(MapSize.x) / 2.f, camHeight, -static_cast<float>(MapSize.y) / 2.f);
        dir += (centre - dir) / 2.f;*/
        auto perp = glm::vec3( dir.z, dir.y, -dir.x );
        if (cro::Util::Random::value(0, 1) == 0)
        {
            perp.x *= -1.f;
            perp.z *= -1.f;
        }
        perp *= static_cast<float>(cro::Util::Random::value(7, 9)) / 100.f;
        perp *= m_holeToModelRatio;
        dir += perp;

        //make sure we're not too close to the player else we won't switch to this cam
        auto followerRad = m_cameras[CameraID::Sky].getComponent<CameraFollower>().radius;
        if (len2 = glm::length2(player.position - dir); len2 < followerRad)
        {
            auto diff = std::sqrt(followerRad) - std::sqrt(len2);
            dir += glm::normalize(dir - player.position) * (diff * 1.01f);

            //clamp the height - if we're so close this needs to happen
            //then we don't want to switch to this cam anyway...
            dir.y = std::min(dir.y, SkyCamHeight);
        }

        setCamTarget(dir);
    }
    else
    {
       // auto pos = m_holeData[m_currentHole].puttFromTee ? glm::vec3(160.f, SkyCamHeight, -100.f) : DefaultSkycamPosition;
        
        
        //setCamTarget(DefaultSkycamPosition);

        auto p = m_holeData[m_currentHole].target;
        p.y = SkyCamHeight;
        setCamTarget(p);
    }

    setGreenCamPosition();

    //set the player controlled drone cam to look at the player
    //(although this will drift as the drone moves)
    auto orientation = lookRotation(m_cameras[CameraID::Drone].getComponent<cro::Transform>().getPosition(), player.position);
    m_cameras[CameraID::Drone].getComponent<cro::Transform>().setRotation(orientation);


    if ((cro::GameController::getControllerCount() > 1
        && (m_sharedData.localConnectionData.playerCount > 1 && !isCPU))
        || m_sharedData.connectionData[1].playerCount != 0) //doesn't account if someone leaves mid-game however
    {
        gamepadNotify(GamepadNotify::NewPlayer);
    }
    else
    {
        cro::GameController::setLEDColour(activeControllerID(player.player), m_sharedData.connectionData[player.client].playerData[player.player].ballTint);
    }

    retargetMinimap(false); //must do this after current player position is set...

    calcTerrainLevel();

    //reset the freecam based on new player position
    auto fcPos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition();
    fcPos.y += 1.f;
    auto fcRot = m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldRotation();
    m_freeCam.getComponent<FpsCamera>().resetTransition(fcPos, fcRot);
    m_freeCam.getComponent<cro::Camera>().resizeCallback(m_freeCam.getComponent<cro::Camera>());

    //if we're not on the tee and there's a mulligan available
    //set it to be allowed from the menu
    if (glm::length2(m_holeData[m_currentHole].tee - player.position) > 0.1f)
    {
        //crude way of telling if we're not at the tee
        m_sharedData.hasMulligan = m_sharedData.leagueRoundID != LeagueRoundID::Club && m_mulliganCount != 0;
    }
    else
    {
        m_sharedData.hasMulligan = false;
    }
}

void GolfState::predictBall(float powerPct)
{
    auto club = getClub();
    if (club != ClubID::Putter)
    {
        powerPct = cro::Util::Easing::easeOutSine(powerPct);
    }
    auto pitch = Clubs[club].getAngle();
    auto yaw = m_inputParser.getYaw();
    auto power = Clubs[club].getPower(m_distanceToHole, m_sharedData.imperialMeasurements) * powerPct;

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    auto lie = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<ClientCollider>().lie;

    impulse *= power;
    impulse *= Dampening[m_currentPlayer.terrain] * LieDampening[m_currentPlayer.terrain][lie];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;
    update.clubID = static_cast<std::uint8_t>(club);

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::BallPrediction, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GolfState::hitBall()
{
    m_sharedData.hasMulligan = false;

    auto club = getClub();
    auto facing = cro::Util::Maths::sgn(m_activeAvatar->model.getComponent<cro::Transform>().getScale().x);
    auto lie = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<ClientCollider>().lie;

    auto [impulse, spin, _] = m_inputParser.getStroke(club, facing, m_distanceToHole);
    impulse *= Dampening[m_currentPlayer.terrain] * LieDampening[m_currentPlayer.terrain][lie];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;
    update.spin = spin;
    update.clubID = static_cast<std::uint8_t>(club);

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_puttViewState.isEnabled = false;
    m_inputParser.setActive(false, m_currentPlayer.terrain);
    m_restoreInput = false;
    m_achievementTracker.hadBackspin = (spin.y < 0);
    m_achievementTracker.hadTopspin = (spin.y > 0);
    m_achievementTracker.hadFlop = Club::getModifierIndex() == 2;
    m_achievementTracker.hadPunch = Club::getModifierIndex() == 1;
    m_achievementTracker.wasGreen = m_currentPlayer.terrain == TerrainID::Green;

    //increase the local stroke count so that the UI is updated
    //the server will set the actual value
    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]++;

    //hide the indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = false;

        //temp ent to delay hiding slightly then pop itself
        static constexpr float FadeTime = 1.5;
        auto tempEnt = m_gameScene.createEntity();
        tempEnt.addComponent<cro::Callback>().active = true;
        tempEnt.getComponent<cro::Callback>().setUserData<float>(FadeTime);
        tempEnt.getComponent<cro::Callback>().function =
            [&, e](cro::Entity f, float dt) mutable
        {
            auto& currTime = f.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::max(0.f, currTime - dt);

            float c = currTime / FadeTime;
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour(c, c, c));

            if (currTime == 0)
            {
                e.getComponent<cro::Model>().setHidden(true);

                f.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(f);
            }
        };
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //reset any warning
    cmd.targetFlags = CommandID::UI::AFKWarn;
    cmd.action = [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            e.getComponent<cro::Callback>().active = false;
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide height to pin
    cmd.targetFlags = CommandID::UI::PinHeight;
    cmd.action =
        [](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    if (m_currentCamera == CameraID::Bystander
        && cro::Util::Random::value(0,1) == 0)
    {
        //activate the zoom
        m_cameras[CameraID::Bystander].getComponent<cro::Callback>().active = true;
    }

    //track achievements for not using more than two putts
    if (club == ClubID::Putter &&
        !m_sharedData.connectionData[m_sharedData.localConnectionData.connectionID].playerData[m_currentPlayer.player].isCPU)
    {
        m_achievementTracker.puttCount++;
        if (m_achievementTracker.puttCount > 2)
        {
            m_achievementTracker.underTwoPutts = false;
        }
    }
}

void GolfState::updateActor(const ActorInfo& update)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [&, update](cro::Entity e, float)
    {
        if (e.isValid())
        {
            auto& interp = e.getComponent<InterpolationComponent<InterpolationType::Linear>>();
            bool active = (interp.id == update.serverID);
            if (active)
            {
                //cro::Transform::QUAT_IDENTITY;
                interp.addPoint({ update.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(update.rotation), update.timestamp});

                //only follow the ball with the camera if it belongs to the active group
                if (update.groupID == m_gameScene.getSystem<CameraFollowSystem>()->getTargetGroup())
                {
                    //update spectator camera
                    cro::Command cmd2;
                    cmd2.targetFlags = CommandID::SpectatorCam;
                    cmd2.action = [&, e](cro::Entity en, float)
                        {
                            en.getComponent<CameraFollower>().target = e;
                            en.getComponent<CameraFollower>().playerPosition = m_currentPlayer.position;
                            en.getComponent<CameraFollower>().holePosition = m_holeData[m_currentHole].pin;
                        };
                    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);

                    //set this ball as the flight cam target
                    m_flightCam.getComponent<cro::Callback>().setUserData<cro::Entity>(e);

                    //if the dest point moves the ball out of a certain radius
                    //then set the drone cam to follow (if it's active)
                    if (m_currentCamera == CameraID::Sky
                        && m_drone.isValid())
                    {
                        auto p = m_cameras[CameraID::Sky].getComponent<cro::Transform>().getPosition();
                        glm::vec2 camPos(p.x, p.z);
                        p = e.getComponent<cro::Transform>().getPosition();
                        glm::vec2 ballPos(p.x, p.z);
                        glm::vec2 destPos(update.position.x, update.position.z);

                        //Later note: I found this half written code^^ and can't remember what
                        //I was planning, so here we go:
                        static constexpr float MaxRadius = 15.f * 15.f;
                        if (glm::length2(camPos - ballPos) < MaxRadius
                            && glm::length2(camPos - destPos) > MaxRadius)
                        {
                            auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
                            auto d = glm::normalize(ballPos - camPos) * 8.f;
                            data.resetPosition += glm::vec3(d.x, 0.f, d.y);
                            data.target.getComponent<cro::Transform>().setPosition(data.resetPosition);
                        }
                    }
                }

                e.getComponent<ClientCollider>().active = active;
                e.getComponent<ClientCollider>().state = update.state;
                e.getComponent<ClientCollider>().lie = update.lie;
            }

            if (update.groupID != m_serverGroup)
            {
                //this is another player so activate the callback to grow the model
                static constexpr float minDist = 0.5f;
                static constexpr float minDistSqr = minDist * minDist;

                if (glm::length2(e.getComponent<cro::Transform>().getPosition() - m_holeData[m_currentHole].tee) > minDistSqr
                    && e.getComponent<cro::Transform>().getScale().x == 0)
                {
                    e.getComponent<cro::Callback>().active = true;
                }

            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    if (update == m_currentPlayer)
    {
        //shows how much effect the wind is currently having
        cmd.targetFlags = CommandID::UI::WindEffect;
        cmd.action = [update](cro::Entity e, float) 
        {
            e.getComponent<cro::Callback>().getUserData<WindEffectData>().first = update.windEffect;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //set the green cam zoom as appropriate
        const bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
            && !m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].targetHit);
        const auto ballTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
        const float ballDist = glm::length(update.position - ballTarget);

#ifdef PATH_TRACING
        updateBallDebug(update.position);
#endif // CRO_DEBUG_

        m_greenCam.getComponent<cro::Callback>().getUserData<MiniCamData>().targetSize =
            interpolate(MiniCamData::MinSize, MiniCamData::MaxSize, smoothstep(MiniCamData::MinSize, MiniCamData::MaxSize, ballDist + 0.4f)); //pad the dist so ball is always in view

        //this is the active ball so update the UI
        cmd.targetFlags = CommandID::UI::PinDistance;
        cmd.action = [&, ballDist, isMultiTarget](cro::Entity e, float)
        {
            formatDistanceString(ballDist, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, m_sharedData.decimateDistance, isMultiTarget);

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        const float travelDistance = glm::length(m_currentPlayer.position - update.position);
        std::stringstream ss;
        ss.precision(2);
        //ss << "Travel: ";
        if (m_sharedData.imperialMeasurements)
        {
            ss << std::fixed << (travelDistance * ToYards);
            ss << "yd";
        }
        else
        {
            ss << std::fixed << travelDistance << "m";
        }
        m_strokeDistanceEnt.getComponent<cro::Callback>().getUserData<cro::String>() = ss.str();

        //set the skip state so we can tell if we're allowed to skip
        m_skipState.state = (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID) ? update.state : -1;
    }
    else
    {
        m_skipState.state = -1;

        //if (m_groupIdle)
        //{
        //    //if this update is the spectated ball, update the ui
        //    if (update.clientID == m_groupPlayerPositions[m_idleCameraIndex].client
        //        && update.playerID == m_groupPlayerPositions[m_idleCameraIndex].player)
        //    {
        //        const bool isMultiTarget = (m_sharedData.scoreType == ScoreType::MultiTarget
        //            && !m_sharedData.connectionData[update.clientID].playerData[update.playerID].targetHit);
        //        const auto ballTarget = isMultiTarget ? m_holeData[m_currentHole].target : m_holeData[m_currentHole].pin;
        //        const float ballDist = glm::length(update.position - ballTarget);

        //        cro::Command cmd;
        //        cmd.targetFlags = CommandID::UI::PinDistance;
        //        cmd.action = [&, ballDist, isMultiTarget](cro::Entity e, float)
        //            {
        //                formatDistanceString(ballDist, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements, isMultiTarget);

        //                auto bounds = cro::Text::getLocalBounds(e);
        //                bounds.width = std::floor(bounds.width / 2.f);
        //                e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        //            };
        //        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        //    }
        //}
    }


    if (m_drone.isValid() 
        && !m_roundEnded
        && glm::length2(m_drone.getComponent<cro::Transform>().getPosition() - update.position) < 4.f)
    {
        auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
        msg->position = m_drone.getComponent<cro::Transform>().getPosition();
        msg->type = GolfEvent::DroneHit;
    }
}

void GolfState::remoteRotation(std::uint32_t data)
{
    if (m_activeAvatar)
    {
        const std::int16_t rotation = (data & 0xffff);
        const float rotationFloat = cro::Util::Net::decompressFloat(rotation, 8);

        const auto client = (data & 0xff000000) >> 24;
        const auto player = (data & 0x00ff0000) >> 16;

        //LogI << "Rx: " << std::hex << data << std::endl;
        //LogI << "Client: " << client << ", player: " << player << ", rotation: " << rotationFloat << std::endl;

        if (client != m_sharedData.localConnectionData.connectionID)
        {
            //this isn't us
            if (m_currentPlayer.client == client &&
                m_currentPlayer.player == player)
            {
                m_activeAvatar->model.getComponent<AvatarRotation>().targetRotation = rotationFloat;
                m_activeAvatar->model.getComponent<AvatarRotation>().active = true;
            }
        }
    }
    else
    {
        LogI << "Avatat is nullptr" << std::endl;
    }
}

std::int32_t GolfState::getClub() const
{
    switch (m_currentPlayer.terrain)
    {
    default: 
        return m_inputParser.getClub();
    case TerrainID::Green: 
        return ClubID::Putter;
    }
}

void GolfState::gamepadNotify(std::int32_t type)
{
    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
        && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
    {
        struct CallbackData final
        {
            float flashRate = 1.f;
            float currentTime = 0.f;
            std::int32_t state = 0; //on or off
            
            std::size_t colourIndex = 0;
            std::vector<cro::Colour> colours;
        };

        auto controllerID = activeControllerID(m_currentPlayer.player);
        auto colour = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].ballTint;

        auto ent = m_gameScene.createEntity();
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().function =
            [&, controllerID, colour](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<CallbackData>();
            if (data.colourIndex < data.colours.size())
            {
                data.currentTime += dt;
                if (data.currentTime > data.flashRate)
                {
                    data.currentTime -= data.flashRate;

                    if (data.state == 0)
                    {
                        data.state = 1;

                        //switch on effect
                        cro::GameController::rumbleStart(controllerID, 0, 60000 * m_sharedData.enableRumble, static_cast<std::uint32_t>(data.flashRate / 1000.f));
                        cro::GameController::setLEDColour(controllerID, data.colours[data.colourIndex++]);
                    }
                    else
                    {
                        data.state = 0;

                        //switch off effect
                        cro::GameController::rumbleStop(controllerID);
                        cro::GameController::setLEDColour(controllerID, data.colours[data.colourIndex++]);
                    }
                }
            }
            else
            {
                cro::GameController::setLEDColour(controllerID, colour);

                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };

        CallbackData data;

        switch (type)
        {
        default: break;
        case GamepadNotify::NewPlayer:
        {
            data.flashRate = 0.15f;
            data.colours = { colour, cro::Colour::Black, colour, cro::Colour::Black };
        }
            break;
        case GamepadNotify::Hole:

            break;
        case GamepadNotify::HoleInOne:
        {
            data.flashRate = 0.1f;
            data.colours = 
            {
                colour,
                cro::Colour::Red,
                colour, 
                cro::Colour::Green,
                colour,
                cro::Colour::Blue,
                colour,
                cro::Colour::Cyan,
                colour,
                cro::Colour::Magenta,
                colour,
                cro::Colour::Yellow,
                colour,
                cro::Colour::Red,
                colour,
                cro::Colour::Green,
                colour,
                cro::Colour::Blue,
                colour,
                cro::Colour::Cyan,
                colour,
                cro::Colour::Magenta,
                colour,
                cro::Colour::Yellow
            };
        }
            break;
        }
        ent.getComponent<cro::Callback>().setUserData<CallbackData>(data);
    }
}