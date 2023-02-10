/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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
#include "SpectatorAnimCallback.hpp"
#include "MiniBallSystem.hpp"
#include "CallbackData.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

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
#include "../ErrorCheck.hpp"

#include <sstream>

namespace
{
#include "WaterShader.inl"
#include "CelShader.inl"
#include "MinimapShader.inl"
#include "WireframeShader.inl"
#include "TransitionShader.inl"
#include "BillboardShader.inl"
#include "ShadowMapping.inl"
#include "TreeShader.inl"
#include "BeaconShader.inl"
#include "PostProcess.inl"
#include "ShaderIncludes.inl"

    std::int32_t debugFlags = 0;
    cro::Entity ballEntity;
    std::size_t bitrate = 0;
    std::size_t bitrateCounter = 0;
    glm::vec4 topSky;
    glm::vec4 bottomSky;

    float godmode = 1.f;
    bool allowAchievements = false;
    const cro::Time DefaultIdleTime = cro::seconds(90.f);
    cro::Time idleTime = DefaultIdleTime;

    constexpr std::uint32_t MaxCascades = 4; //actual value is 1 less this - see ShadowQuality::update()
    constexpr float MaxShadowFarDistance = 150.f;
    constexpr float MaxShadowNearDistance = 90.f;

    struct ShadowQuality final
    {
        float shadowNearDistance = MaxShadowNearDistance;
        float shadowFarDistance = MaxShadowFarDistance;
        std::uint32_t cascadeCount = MaxCascades - 1;

        void update(bool hq)
        {
            cascadeCount = hq ? 3 : 1;
            float divisor = static_cast<float>(std::pow((MaxCascades - cascadeCount), 2)); //cascade sizes are exponential
            shadowNearDistance = 90.f / divisor;
            shadowFarDistance = 150.f / divisor;
        }
    }shadowQuality;

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    constexpr float MaxRotation = 0.3f;// 0.13f;
    constexpr float MaxPuttRotation = 0.4f;// 0.24f;
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus(), 768/*, cro::INFO_FLAG_SYSTEM_TIME | cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_skyScene          (context.appInstance.getMessageBus(), 512),
    m_uiScene           (context.appInstance.getMessageBus(), 1024),
    m_trophyScene       (context.appInstance.getMessageBus()),
    m_mouseVisible      (true),
    m_inputParser       (sd, context.appInstance.getMessageBus()),
    m_cpuGolfer         (m_inputParser, m_currentPlayer, m_collisionMesh),
    m_wantsGameState    (true),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_windBuffer        ("WindValues"),
    m_holeToModelRatio  (1.f),
    m_currentHole       (0),
    m_distanceToHole    (1.f), //don't init to 0 incase we get div0
    m_terrainChunker    (m_gameScene),
    m_terrainBuilder    (sd, m_holeData, m_terrainChunker),
    m_audioPath         ("assets/golf/sound/ambience.xas"),
    m_currentCamera     (CameraID::Player),
    m_photoMode         (false),
    m_activeAvatar      (nullptr),
    m_camRotation       (0.f),
    m_roundEnded        (false),
    m_newHole           (true),
    m_viewScale         (1.f),
    m_scoreColumnCount  (2),
    m_readyQuitFlags    (0),
    m_emoteWheel        (sd, m_currentPlayer),
    m_minimapScale      (1.f),
    m_minimapRotation   (0.f),
    m_minimapOffset     (0.f),
    m_hadFoul           (false)
{
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

    registerCommand("noclip", [&](const std::string&)
        {
            toggleFreeCam();
            if (m_photoMode)
            {
                cro::Console::print("noclip ON");
            }
            else
            {
                cro::Console::print("noclip OFF");
            }
        });

    shadowQuality.update(sd.hqShadows);


    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        buildTrophyScene();
        buildScene();
        });

    createTransition();

    sd.baseState = StateID::Golf;

    std::int32_t humanCount = 0;
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        if (!m_sharedData.localConnectionData.playerData[i].isCPU)
        {
            humanCount++;
        }
    }
    allowAchievements = humanCount == 1;

    //This is set when setting active player.
    Achievements::setActive(allowAchievements);


    std::int32_t clientCount = 0;
    for (auto& c : sd.connectionData)
    {
        if (c.playerCount != 0)
        {
            clientCount++;

            for (auto i = 0u; i < c.playerCount; ++i)
            {
                c.playerData[i].matchScore = 0;
                c.playerData[i].skinScore = 0;
            }
        }
    }
    if (clientCount == 4)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::BetterWithFriends]);
    }

    //glLineWidth(1.5f);
#ifdef CRO_DEBUG_
    ballEntity = {};

    registerDebugWindows();

#endif
    cro::App::getInstance().resetFrameTime();
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return true;
    }


    //handle this first in case the input parser is currently suspended
    if (m_photoMode)
    {
        m_gameScene.getSystem<FpsCameraSystem>()->handleEvent(evt);
    }
    else
    {
        if (!m_emoteWheel.handleEvent(evt))
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
        if (m_mouseVisible
            && getStateCount() == 1)
        {
            m_mouseVisible = false; //TODO do we really need to track this?
            cro::App::getWindow().setMouseCaptured(true);
        }
    };

    const auto resetIdle = [&]()
    {
        m_idleTimer.restart();
        idleTime = DefaultIdleTime / 3.f;

        if (m_currentCamera == CameraID::Idle)
        {
            setActiveCamera(CameraID::Player);
            m_inputParser.setSuspended(false);

            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float)
            {
                auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
                e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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

    if (evt.type == SDL_KEYUP)
    {
        //hideMouse(); //TODO this should only react to current keybindings
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_KP_DIVIDE:
            if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
            {
                if (m_inputParser.getActive())
                {
                    if (m_currentCamera == CameraID::Player)
                    {
                        setActiveCamera(CameraID::Bystander);
                    }
                    else if (m_currentCamera == CameraID::Bystander)
                    {
                        setActiveCamera(CameraID::Player);
                    }
                }
            }
            break;
        case SDLK_TAB:
            showScoreboard(false);
            break;
        case SDLK_SPACE: //TODO this should read the keymap... but it's not const
            closeMessage();
            break;
#ifdef CRO_DEBUG_
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextHole), net::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextPlayer), net::NetFlag::Reliable);
            break;
        case SDLK_F4:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::GotoGreen), net::NetFlag::Reliable);
            break;
        case SDLK_F6:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::EndGame), net::NetFlag::Reliable);
            break;
        case SDLK_F7:
            //showCountdown(30);
            //showMessageBoard(MessageBoardID::Scrub);
            //requestStackPush(StateID::Tutorial);
            showNotification("buns");
            break;
        case SDLK_F8:
        {
            m_sharedData.hqShadows = !m_sharedData.hqShadows;
            auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
            msg->type = SystemEvent::ShadowQualityChanged;
        }
            break;
        case SDLK_F10:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::ChangeWind), net::NetFlag::Reliable);
            break;
        case SDLK_KP_0:
            setActiveCamera(CameraID::Idle);
        {
            /*static bool hidden = false;
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(!hidden);
            hidden = !hidden;*/
        }
            break;
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
        case SDLK_KP_7:
        {
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::BirdHit;
            msg2->position = m_currentPlayer.position;
            float rot = glm::eulerAngles(m_cameras[m_currentCamera].getComponent<cro::Transform>().getWorldRotation()).y;
            msg2->travelDistance = rot;
        }
            break;
        case SDLK_KP_MULTIPLY:

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
        case SDLK_INSERT:
            toggleFreeCam();
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        resetIdle();
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            if (!evt.key.repeat)
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
        case SDLK_p:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        resetIdle();

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonBack:
            showScoreboard(true);
            break;
        case cro::GameController::ButtonB:
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
            toggleQuitReady();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        hideMouse();
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonBack:
            showScoreboard(false);
            break;
        case cro::GameController::ButtonStart:
        case cro::GameController::ButtonGuide:
            requestStackPush(StateID::Pause);
            break;
        case cro::GameController::ButtonA:
            if (evt.cbutton.which == cro::GameController::deviceID(activeControllerID(m_currentPlayer.player)))
            {
                closeMessage();
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
        if (!m_photoMode
            && (evt.motion.state & SDL_BUTTON_RMASK) == 0)
        {
            cro::App::getWindow().setMouseCaptured(false);
            m_mouseVisible = true;
        }
    }
    else if (evt.type == SDL_JOYAXISMOTION)
    {
        if (std::abs(evt.caxis.value) > 10000)
        {
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
            floatingMessage(std::to_string(data.level) + " XP");
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

            if (m_currentPlayer.client == m_sharedData.localConnectionData.connectionID
                && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU)
            {
                auto strLow = static_cast<std::uint16_t>(50000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;
                auto strHigh = static_cast<std::uint16_t>(35000.f * m_inputParser.getPower()) * m_sharedData.enableRumble;

                cro::GameController::rumbleStart(activeControllerID(m_sharedData.inputBinding.playerID), strLow, strHigh, 200);
            }

            //check if we hooked/sliced
            if (auto club = getClub(); club != ClubID::Putter)
            {
                auto hook = m_inputParser.getHook() * m_activeAvatar->model.getComponent<cro::Transform>().getScale().x;
                if (hook < -0.2f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::HookedBall;
                    floatingMessage("Hook");
                }
                else if (hook > 0.2f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::SlicedBall;
                    floatingMessage("Slice");
                }

                auto power = m_inputParser.getPower();
                hook *= 20.f;
                hook = std::round(hook);
                hook /= 20.f;

                if (power > 0.59f //hmm not sure why power should factor into this?
                    && std::abs(hook) < 0.05f)
                {
                    auto* msg3 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                    msg3->type = GolfEvent::NiceShot;

                    //award more XP for aiming straight
                    float dirAmount = /*cro::Util::Easing::easeOutExpo*/((m_inputParser.getMaxRotation() - std::abs(m_inputParser.getRotation())) / m_inputParser.getMaxRotation());

                    auto xp = std::clamp(static_cast<std::int32_t>(6.f * dirAmount), 0, 6);
                    if (xp)
                    {
                        Social::awardXP(xp);
                    }
                }

                //hide the ball briefly to hack around the motion lag
                //(the callback automatically scales back)
                m_activeAvatar->ballModel.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                m_activeAvatar->ballModel.getComponent<cro::Callback>().active = true;
                m_activeAvatar->ballModel.getComponent<cro::Model>().setHidden(true);

                //if particles are enabled, start them
                if (m_sharedData.showBallTrail &&
                    club < ClubID::GapWedge)
                {
                    auto& emitter = m_avatars[m_currentPlayer.client][m_currentPlayer.player].ballModel.getComponent<cro::ParticleEmitter>();
                    emitter.settings.colour = getBeaconColour(m_sharedData.beaconColour);
                    emitter.start();
                }
            }

            m_gameScene.setSystemActive<CameraFollowSystem>(true);
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
        case SceneEvent::TransitionComplete:
        {
            //show miniball
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::MiniBall;
            cmd.action = [&](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                e.getComponent<cro::Transform>().setPosition(toMinimapCoords(m_holeData[m_currentHole].tee));
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete, m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
            break;
        case SceneEvent::RequestSwitchCamera:
            setActiveCamera(data.data);
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
        case GolfEvent::HitBall:
        {
            hitBall();
#ifdef PATH_TRACING
            beginBallDebug();
#endif
        }
        break;
        case GolfEvent::ClubChanged:
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float)
            {
                float scale = std::max(0.25f, Clubs[getClub()].getPower(m_distanceToHole) / Clubs[ClubID::Driver].getPower(m_distanceToHole));
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            //update the player with correct club
            if (m_activeAvatar
                && m_activeAvatar->hands)
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
        }
        break;
        case GolfEvent::BallLanded:
        {
            switch (data.terrain)
            {
            default: break;
            case TerrainID::Bunker:
            case TerrainID::Scrub:
            case TerrainID::Water:
                if (data.travelDistance > 100.f)
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.2f);
                }
                break;
            case TerrainID::Green:
                if (getClub() != ClubID::Putter)
                {
                    if (data.pinDistance < 1.f
                        || data.travelDistance > 10000.f)
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Celebrate], 1.f, 1.2f);
                    }
                    else if (data.travelDistance < 9.f)
                    {
                        m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
                    }
                }
                break;
            case TerrainID::Fairway:
                if (data.travelDistance < 16.f)
                {
                    m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[AnimationID::Disappoint], 1.f, 0.4f);
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
                }
            };

            if (data.terrain == TerrainID::Fairway)
            {
                Social::awardXP(1);
            }
#ifdef PATH_TRACING
            endBallDebug();
#endif
        }
        break;
        case GolfEvent::DroneHit:
        {
            Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOneMillion]);
            Social::awardXP(XPValues[XPID::Special]);

            m_gameScene.destroyEntity(m_drone);
            m_drone = {};

            m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = &m_resources.shaders.get(ShaderID::Noise);

            if (m_currentCamera == CameraID::Sky)
            {
                m_courseEnt.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(ShaderID::Noise));
            }

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 10.f, 32.f, 0.2f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString("FEED LOST");
            entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                static float state = 0.f;

                if (m_currentCamera == CameraID::Sky)
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

                cmd.targetFlags = CommandID::UI::PinDistance;
                cmd.action =
                    [&](cro::Entity e, float)
                {
                    formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements);

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
            }
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
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::CPUThink, std::uint8_t(data.type), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
    }
        break;
    }

    m_cpuGolfer.handleMessage(msg);

    m_gameScene.forwardMessage(msg);
    m_skyScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
    m_trophyScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
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

    //this gets used a lot so we'll save on some calls to length()
    m_distanceToHole = glm::length(m_holeData[m_currentHole].pin - m_currentPlayer.position);

    m_depthMap.update(1);

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
#ifdef CRO_DEBUG_
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                bitrateCounter += evt.packet.getSize() * 8;
            }
#endif
            //handle events
            handleNetEvent(evt);
        }

#ifdef CRO_DEBUG_
        static float bitrateTimer = 0.f;
        bitrateTimer += dt;
        if (bitrateTimer > 1.f)
        {
            bitrateTimer -= 1.f;
            bitrate = bitrateCounter;
            bitrateCounter = 0;
        }
#endif

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
        if (std::fabs(diff) > 0.001f)
        {
            m_resolutionUpdate.resolutionData.nearFadeDistance += (diff * dt);
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

    //don't update the CPU or gamepad if there are any menus open
    if (getStateCount() == 1)
    {
        m_cpuGolfer.update(dt, windVector, m_distanceToHole);
        m_inputParser.update(dt, m_currentPlayer.terrain);

        if (float movement = m_inputParser.getCamMotion(); movement != 0)
        {
            updateCameraHeight(movement * dt);
        }
    }


    m_emoteWheel.update(dt);
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    //m_terrainChunker.update();//this needs to be done after the camera system is updated

    //do this last to ensure game scene camera is up to date
    const auto& srcCam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto& dstCam = m_skyScene.getActiveCamera().getComponent<cro::Camera>();

    auto baseFov = m_sharedData.fov * cro::Util::Const::degToRad;
    auto ratio = srcCam.getFOV() / baseFov;
    float diff = 1.f - ratio;
    diff -= (diff / 32.f);

    dstCam.viewport = srcCam.viewport;
    dstCam.setPerspective(baseFov * (1.f - diff), srcCam.getAspectRatio(), 0.5f, 14.f);

    m_skyScene.getActiveCamera().getComponent<cro::Transform>().setRotation(m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldRotation());
    auto pos = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    pos.x = 0.f;
    pos.y /= 64.f;
    pos.z = 0.f;
    m_skyScene.getActiveCamera().getComponent<cro::Transform>().setPosition(pos);
    //and make sure the skybox is up to date too, so there's
    //no lag between camera orientation.
    m_skyScene.simulate(dt);


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

    //play avatar sound if player idles
    if (!m_sharedData.tutorial
        && !m_roundEnded)
    {
        if (m_idleTimer.elapsed() > idleTime)
        {
            m_idleTimer.restart();
            idleTime = cro::seconds(std::max(10.f, idleTime.asSeconds() / 2.f));

            auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
            msg->type = SceneEvent::PlayerIdle;

            gamepadNotify(GamepadNotify::NewPlayer);

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
        if (m_idleTimer.elapsed() > (idleTime * 0.65f)
            && m_currentCamera == CameraID::Player
            && m_inputParser.getActive()) //hmm this stops this happening on remote clients
        {
            setActiveCamera(CameraID::Idle);
            m_inputParser.setSuspended(true);

            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;
            cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }
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
    cam.renderFlags = RenderFlags::Reflection;

    auto& skyCam = m_skyScene.getActiveCamera().getComponent<cro::Camera>();
    skyCam.setActivePass(cro::Camera::Pass::Reflection);
    skyCam.renderFlags = RenderFlags::Reflection;
    skyCam.viewport = { 0.f,0.f,1.f,1.f };

    cam.reflectionBuffer.clear(cro::Colour::Red);
    //don't want to test against skybox depth values.
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    //glCheck(glEnable(GL_PROGRAM_POINT_SIZE));
    m_gameScene.render();
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.renderFlags = RenderFlags::All;
    cam.viewport = oldVP;

    skyCam.setActivePass(cro::Camera::Pass::Final);
    skyCam.renderFlags = RenderFlags::All;
    skyCam.viewport = oldVP;

    //then render scene
    glUseProgram(m_gridShader.shaderID);
    glUniform1f(m_gridShader.transparency, m_sharedData.gridTransparency);
    m_gameSceneTexture.clear();
    m_skyScene.render();
    glClear(GL_DEPTH_BUFFER_BIT);
    glCheck(glEnable(GL_LINE_SMOOTH));
    m_gameScene.render();
    glCheck(glDisable(GL_LINE_SMOOTH));
#ifdef CRO_DEBUG_
    //m_collisionMesh.renderDebug(cam.getActivePass().viewProjectionMatrix, m_gameSceneTexture.getSize());
#endif
    m_gameSceneTexture.display();

    //update mini green if ball is there
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        glUseProgram(m_gridShader.shaderID);
        glUniform1f(m_gridShader.transparency, 0.f);

        auto oldCam = m_gameScene.setActiveCamera(m_greenCam);
        m_greenBuffer.clear();
        m_gameScene.render();
        m_greenBuffer.display();
        m_gameScene.setActiveCamera(oldCam);
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
void GolfState::loadAssets()
{
    m_reflectionMap.loadFromFile("assets/golf/images/skybox/billiards/trophy.ccm");

    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    //load materials
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    //cel shaded material
    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n" + wobble);
    auto* shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelSkinned, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define DITHERED\n#define SKINNED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Ball, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Ball);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ball] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define REFLECTIONS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Trophy] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::Trophy]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);
    noiseTex.setSmooth(true);
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]).setProperty("u_noiseTexture", noiseTex);

    //custom shadow map so shadows move with wind too...
    m_resources.shaders.loadFromString(ShaderID::ShadowMap, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMap);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::ShadowMap] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]).setProperty("u_noiseTexture", noiseTex);

    m_resources.shaders.loadFromString(ShaderID::BillboardShadow, BillboardVertexShader, ShadowFragment, "#define DITHERED\n#define SHADOW_MAPPING\n#define ALPHA_CLIP\n");
    shader = &m_resources.shaders.get(ShaderID::BillboardShadow);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Leaderboard, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define SUBRECT\n");
    shader = &m_resources.shaders.get(ShaderID::Leaderboard);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Leaderboard] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define DITHERED\n#define SKINNED\n#define NOCHEX\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinned] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Player, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define SKINNED\n#define NOCHEX\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Player);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Player] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Hair, CelVertexShader, CelFragmentShader, "#define USER_COLOUR\n#define NOCHEX\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Hair);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Hair] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TERRAIN\n#define COMP_SHADE\n#define COLOUR_LEVELS 5.0\n#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CourseGrid, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n#define CONTOUR\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CourseGrid);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    auto* gridShader = shader; //used below when testing for putt holes.
    m_gridShader.shaderID = shader->getGLHandle();
    m_gridShader.transparency = shader->getUniformID("u_transparency");
    m_gridShader.minHeight = shader->getUniformID("u_minHeight");
    m_gridShader.maxHeight = shader->getUniformID("u_maxHeight");

    m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);


    //shaders used by terrain
    m_resources.shaders.loadFromString(ShaderID::CelTexturedInstanced, CelVertexShader, CelFragmentShader, "#define WIND_WARP\n#define TEXTURED\n#define DITHERED\n#define NOCHEX\n#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::CelTexturedInstanced);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::ShadowMapInstanced, ShadowVertex, ShadowFragment, "#define DITHERED\n#define WIND_WARP\n#define ALPHA_CLIP\n#define INSTANCING\n");
    shader = &m_resources.shaders.get(ShaderID::ShadowMapInstanced);
    m_windBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::Crowd, CelVertexShader, CelFragmentShader, "#define DITHERED\n#define INSTANCING\n#define VATS\n#define NOCHEX\n#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::Crowd);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CrowdShadow, ShadowVertex, ShadowFragment, "#define DITHERED\n#define INSTANCING\n#define VATS\n");
    m_resolutionBuffer.addShader(m_resources.shaders.get(ShaderID::CrowdShadow));

    if (m_sharedData.treeQuality == SharedStateData::High)
    {
        m_resources.shaders.loadFromString(ShaderID::TreesetBranch, BranchVertex, BranchFragment, "#define ALPHA_CLIP\n#define INSTANCING\n" + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetBranch);
        m_scaleBuffer.addShader(*shader);
        m_resolutionBuffer.addShader(*shader);
        m_windBuffer.addShader(*shader);

        m_resources.shaders.loadFromString(ShaderID::TreesetLeaf, BushVertex, /*BushGeom,*/ BushFragment, "#define POINTS\n#define INSTANCING\n#define HQ\n" + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetLeaf);
        m_scaleBuffer.addShader(*shader);
        m_resolutionBuffer.addShader(*shader);
        m_windBuffer.addShader(*shader);


        m_resources.shaders.loadFromString(ShaderID::TreesetShadow, ShadowVertex, ShadowFragment, "#define INSTANCING\n#define TREE_WARP\n#define ALPHA_CLIP\n" + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetShadow);
        m_windBuffer.addShader(*shader);
        //m_resolutionBuffer.addShader(*shader);

        std::string alphaClip;
        if (m_sharedData.hqShadows)
        {
            alphaClip = "#define ALPHA_CLIP\n";
        }
        m_resources.shaders.loadFromString(ShaderID::TreesetLeafShadow, ShadowVertex, /*ShadowGeom,*/ ShadowFragment, "#define POINTS\n#define INSTANCING\n#define LEAF_SIZE\n" + alphaClip + wobble);
        shader = &m_resources.shaders.get(ShaderID::TreesetLeafShadow);
        m_windBuffer.addShader(*shader);
        //m_resolutionBuffer.addShader(*shader);
    }


    //scanline transition
    m_resources.shaders.loadFromString(ShaderID::Transition, MinimapVertex, ScanlineTransition);

    //noise effect
    m_resources.shaders.loadFromString(ShaderID::Noise, MinimapVertex, NoiseFragment);
    shader = &m_resources.shaders.get(ShaderID::Noise);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);


    //wireframe
    m_resources.shaders.loadFromString(ShaderID::Wireframe, WireframeVertex, WireframeFragment);
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Wireframe));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;

    m_resources.shaders.loadFromString(ShaderID::WireframeCulled, WireframeVertex, WireframeFragment, "#define CULLED\n");
    m_materialIDs[MaterialID::WireFrameCulled] = m_resources.materials.add(m_resources.shaders.get(ShaderID::WireframeCulled));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]).blendMode = cro::Material::BlendMode::Alpha;
    //shader = &m_resources.shaders.get(ShaderID::WireframeCulled);
    //m_resolutionBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::PuttAssist, WireframeVertex, WireframeFragment, "#define HUE\n");
    m_materialIDs[MaterialID::PuttAssist] = m_resources.materials.add(m_resources.shaders.get(ShaderID::PuttAssist));
    m_resources.materials.get(m_materialIDs[MaterialID::PuttAssist]).blendMode = cro::Material::BlendMode::Additive;

    //minimap - green overhead
    m_resources.shaders.loadFromString(ShaderID::Minimap, MinimapVertex, MinimapFragment);

    //minimap - course view
    m_resources.shaders.loadFromString(ShaderID::MinimapView, MinimapViewVertex, MinimapViewFragment);

    //water
    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment);
    shader = &m_resources.shaders.get(ShaderID::Water);
    m_scaleBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(*shader);
    //m_resources.materials.get(m_materialIDs[MaterialID::Water]).setProperty("u_noiseTexture", noiseTex);
    //forces rendering last to reduce overdraw - overdraws stroke indicator though(??)
    //also suffers the black banding effect where alpha  < 1
    //m_resources.materials.get(m_materialIDs[MaterialID::Water]).blendMode = cro::Material::BlendMode::Alpha; 
    

    m_resources.shaders.loadFromString(ShaderID::Horizon, HorizonVert, HorizonFrag);
    shader = &m_resources.shaders.get(ShaderID::Horizon);
    m_materialIDs[MaterialID::Horizon] = m_resources.materials.add(*shader);

    //mmmm... bacon
    m_resources.shaders.loadFromString(ShaderID::Beacon, BeaconVertex, BeaconFragment, "#define TEXTURED\n");
    m_materialIDs[MaterialID::Beacon] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Beacon));


    //model definitions
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }
    m_modelDefs[ModelID::BallShadow]->loadFromFile("assets/golf/models/ball_shadow.cmt");
    m_modelDefs[ModelID::PlayerShadow]->loadFromFile("assets/golf/models/player_shadow.cmt");

    //ball models - the menu should never have let us get this far if it found no ball files
    for (const auto& [colour, uid, path, _] : m_sharedData.ballModels)
    {
        std::unique_ptr<cro::ModelDefinition> def = std::make_unique<cro::ModelDefinition>(m_resources);
        m_ballModels.insert(std::make_pair(uid, std::move(def)));
    }

    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar_wide");
    m_sprites[SpriteID::PowerBarInner] = spriteSheet.getSprite("power_bar_inner_wide");
    m_sprites[SpriteID::HookBar] = spriteSheet.getSprite("hook_bar");
    m_sprites[SpriteID::MiniFlag] = spriteSheet.getSprite("putt_flag");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");
    m_sprites[SpriteID::WindSpeed] = spriteSheet.getSprite("wind_speed");
    m_sprites[SpriteID::Thinking] = spriteSheet.getSprite("thinking");
    m_sprites[SpriteID::MessageBoard] = spriteSheet.getSprite("message_board");
    m_sprites[SpriteID::Bunker] = spriteSheet.getSprite("bunker");
    m_sprites[SpriteID::Foul] = spriteSheet.getSprite("foul");
    auto flagSprite = spriteSheet.getSprite("flag03");
    m_flagQuad.setTexture(*flagSprite.getTexture());
    m_flagQuad.setTextureRect(flagSprite.getTextureRect());

    spriteSheet.loadFromFile("assets/golf/sprites/emotes.spt", m_resources.textures);
    m_sprites[SpriteID::EmoteHappy] = spriteSheet.getSprite("happy_small");
    m_sprites[SpriteID::EmoteGrumpy] = spriteSheet.getSprite("grumpy_small");
    m_sprites[SpriteID::EmoteLaugh] = spriteSheet.getSprite("laughing_small");
    m_sprites[SpriteID::EmoteSad] = spriteSheet.getSprite("sad_small");
    m_sprites[SpriteID::EmotePulse] = spriteSheet.getSprite("pulse");


    //load audio from avatar info
    for (const auto& avatar : m_sharedData.avatarInfo)
    {
        m_gameScene.getDirector<GolfSoundDirector>()->addAudioScape(avatar.audioscape, m_resources.audio);
    }

    //TODO we don't actually need to load *every* sprite sheet, just look up the index first
    //and load it as necessary...
    //however: while it may load unnecessary audioscapes, it does ensure they are loaded in the correct order :S

    //copy into active player slots
    const auto indexFromSkinID = [&](std::uint32_t skinID)->std::size_t
    {
        auto result = std::find_if(m_sharedData.avatarInfo.begin(), m_sharedData.avatarInfo.end(),
            [skinID](const SharedStateData::AvatarInfo& ai) 
            {
                return skinID == ai.uid;
            });

        if (result != m_sharedData.avatarInfo.end())
        {
            return std::distance(m_sharedData.avatarInfo.begin(), result);
        }
        return 0;
    };

    const auto indexFromHairID = [&](std::uint32_t hairID)
    {
        if (auto hair = std::find_if(m_sharedData.hairInfo.begin(), m_sharedData.hairInfo.end(),
            [hairID](const SharedStateData::HairInfo& h) {return h.uid == hairID;});
            hair != m_sharedData.hairInfo.end())
        {
            return static_cast<std::int32_t>(std::distance(m_sharedData.hairInfo.begin(), hair));
        }

        return 0;
    };

    //player avatars
    cro::ModelDefinition md(m_resources);
    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            auto skinID = m_sharedData.connectionData[i].playerData[j].skinID;
            auto avatarIndex = indexFromSkinID(skinID);

            m_gameScene.getDirector<GolfSoundDirector>()->setPlayerIndex(i, j, static_cast<std::int32_t>(avatarIndex));
            m_avatars[i][j].flipped = m_sharedData.connectionData[i].playerData[j].flipped;

            //player avatar model
            //TODO we might want error checking here, but the model files
            //should have been validated by the menu state.
            if (md.loadFromFile(m_sharedData.avatarInfo[avatarIndex].modelPath))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                entity.addComponent<cro::Callback>().setUserData<PlayerCallbackData>();
                entity.getComponent<cro::Callback>().function =
                    [&](cro::Entity e, float dt)
                {
                    auto& [direction, scale] = e.getComponent<cro::Callback>().getUserData<PlayerCallbackData>();
                    const auto xScale = e.getComponent<cro::Transform>().getScale().x; //might be flipped

                    if (direction == 0)
                    {
                        scale = std::min(1.f, scale + (dt * 3.f));

                        if (scale == 1)
                        {
                            direction = 1;
                            e.getComponent<cro::Callback>().active = false;
                        }
                    }
                    else
                    {
                        scale = std::max(0.f, scale - (dt * 3.f));

                        if (scale == 0)
                        {
                            direction = 0;
                            e.getComponent<cro::Callback>().active = false;
                            e.getComponent<cro::Model>().setHidden(true);

                            //seems counter-intuitive to search the avatar here
                            //but we have a, uh.. 'handy' handle (to the hands)
                            if (m_activeAvatar->hands)
                            {
                                //we have to free this up alse the model might
                                //become attached to two avatars...
                                m_activeAvatar->hands->setModel({});
                            }
                        }
                    }
                    auto yScale = cro::Util::Easing::easeOutBack(scale);
                    e.getComponent<cro::Transform>().setScale(glm::vec3(xScale, yScale, yScale));
                };
                md.createModel(entity);

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Player]);
                material.setProperty("u_diffuseMap", m_sharedData.avatarTextures[i][j]);
                entity.getComponent<cro::Model>().setMaterial(0, material);

                if (m_avatars[i][j].flipped)
                {
                    entity.getComponent<cro::Transform>().setScale({ -1.f, 0.f, 0.f });
                    entity.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                }
                else
                {
                    entity.getComponent<cro::Transform>().setScale({ 1.f, 0.f, 0.f });
                }

                //this should assert in debug, however oor IDs will just be ignored
                //in release so this is the safest way to handle missing animations
                std::fill(m_avatars[i][j].animationIDs.begin(), m_avatars[i][j].animationIDs.end(), AnimationID::Invalid);
                if (entity.hasComponent<cro::Skeleton>())
                {
                    auto& skel = entity.getComponent<cro::Skeleton>();
                    
                    //find attachment points for club model
                    auto id = skel.getAttachmentIndex("hands");
                    if (id > -1)
                    {
                        m_avatars[i][j].hands = &skel.getAttachments()[id];
                    }                    
                    
                    const auto& anims = skel.getAnimations();
                    for (auto k = 0u; k < std::min(anims.size(), static_cast<std::size_t>(AnimationID::Count)); ++k)
                    {
                        if (anims[k].name == "idle")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Idle] = k;
                            skel.play(k);
                        }
                        else if (anims[k].name == "drive")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Swing] = k;
                        }
                        else if (anims[k].name == "chip")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Chip] = k;
                        }
                        else if (anims[k].name == "putt")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Putt] = k;
                        }
                        else if (anims[k].name == "celebrate")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Celebrate] = k;
                        }
                        else if (anims[k].name == "disappointment")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Disappoint] = k;
                        }
                        else if (anims[k].name == "impatient")
                        {
                            m_avatars[i][j].animationIDs[AnimationID::Impatient] = k;
                        }
                    }

                    //and attachment for hair/hats
                    id = skel.getAttachmentIndex("head");
                    if (id > -1)
                    {
                        //look to see if we have a hair model to attach
                        auto hairID = indexFromHairID(m_sharedData.connectionData[i].playerData[j].hairID);
                        if (hairID != 0
                            && md.loadFromFile(m_sharedData.hairInfo[hairID].modelPath))
                        {
                            auto hairEnt = m_gameScene.createEntity();
                            hairEnt.addComponent<cro::Transform>();
                            md.createModel(hairEnt);

                            //set material and colour
                            material = m_resources.materials.get(m_materialIDs[MaterialID::Hair]);
                            material.setProperty("u_hairColour", cro::Colour(pc::Palette[m_sharedData.connectionData[i].playerData[j].avatarFlags[pc::ColourKey::Hair]].light));
                            //material.setProperty("u_darkColour", cro::Colour(pc::Palette[m_sharedData.connectionData[i].playerData[j].avatarFlags[pc::ColourKey::Hair]].dark));
                            hairEnt.getComponent<cro::Model>().setMaterial(0, material);

                            skel.getAttachments()[id].setModel(hairEnt);

                            if (m_avatars[i][j].flipped)
                            {
                                hairEnt.getComponent<cro::Model>().setFacing(cro::Model::Facing::Back);
                            }

                            hairEnt.addComponent<cro::Callback>().active = true;
                            hairEnt.getComponent<cro::Callback>().function =
                                [entity](cro::Entity e, float)
                            {
                                e.getComponent<cro::Model>().setHidden(entity.getComponent<cro::Model>().isHidden());
                            };
                        }
                    }
                }

                entity.getComponent<cro::Model>().setHidden(true);
                m_avatars[i][j].model = entity;
            }
        }
    }


    //club models
    m_clubModels[ClubModel::Wood] = m_gameScene.createEntity();
    m_clubModels[ClubModel::Wood].addComponent<cro::Transform>();
    if (md.loadFromFile("assets/golf/models/club_wood.cmt"))
    {
        md.createModel(m_clubModels[ClubModel::Wood]);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material, 0);
        m_clubModels[ClubModel::Wood].getComponent<cro::Model>().setMaterial(0, material);
        m_clubModels[ClubModel::Wood].getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniGreen);
    }
    else
    {
        createFallbackModel(m_clubModels[ClubModel::Wood], m_resources);
    }
    m_clubModels[ClubModel::Wood].addComponent<cro::Callback>().active = true;
    m_clubModels[ClubModel::Wood].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_activeAvatar)
        {
            bool hidden = !(!m_activeAvatar->model.getComponent<cro::Model>().isHidden() &&
                m_activeAvatar->hands->getModel() == e);

            e.getComponent<cro::Model>().setHidden(hidden);
        }
    };


    m_clubModels[ClubModel::Iron] = m_gameScene.createEntity();
    m_clubModels[ClubModel::Iron].addComponent<cro::Transform>();
    if (md.loadFromFile("assets/golf/models/club_iron.cmt"))
    {
        md.createModel(m_clubModels[ClubModel::Iron]);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material, 0);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setMaterial(0, material);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniGreen);
    }
    else
    {
        createFallbackModel(m_clubModels[ClubModel::Iron], m_resources);
        m_clubModels[ClubModel::Iron].getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Cyan);
    }
    m_clubModels[ClubModel::Iron].addComponent<cro::Callback>().active = true;
    m_clubModels[ClubModel::Iron].getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_activeAvatar)
        {
            bool hidden = !(!m_activeAvatar->model.getComponent<cro::Model>().isHidden() &&
                m_activeAvatar->hands->getModel() == e);

            e.getComponent<cro::Model>().setHidden(hidden);
        }
    };


    //ball resources - ball is rendered as a single point
    //at a distance, and as a model when closer
    //glCheck(glPointSize(BallPointSize)); - this is set in resize callback based on the buffer resolution/pixel scale
    m_ballResources.materialID = m_materialIDs[MaterialID::WireFrameCulled];
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,   1.f, 1.f, 1.f, 1.f
    };
    std::vector<std::uint32_t> indices =
    {
        0
    };

    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    meshData = &m_resources.meshes.getMesh(m_ballResources.shadowMeshID);
    verts =
    {
        0.f, 0.f, 0.f,    0.f, 0.f, 0.f, 0.25f,
    };
    meshData->vertexCount = 1;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    submesh = &meshData->indexData[0];
    submesh->indexCount = 1;
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    //used when parsing holes
    auto addCrowd = [&](HoleData& holeData, glm::vec3 position, float rotation)
    {
        constexpr auto MapOrigin = glm::vec3(MapSize.x / 2.f, 0.f, -static_cast<float>(MapSize.y) / 2.f);

        //used by terrain builder to create instanced geom
        glm::vec3 offsetPos(-8.f, 0.f, 0.f);
        const glm::mat4 rotMat = glm::rotate(glm::mat4(1.f), rotation * cro::Util::Const::degToRad, cro::Transform::Y_AXIS);

        for (auto i = 0; i < 16; ++i)
        {
            auto offset = glm::vec3(rotMat * glm::vec4(offsetPos, 1.f));

            auto tx = glm::translate(glm::mat4(1.f), position - MapOrigin);
            tx = glm::translate(tx, offset);

            auto holeDir = holeData.pin - (glm::vec3(tx[3]) + MapOrigin);
            if (float len = glm::length2(holeDir); len < 1600.f)
            {
                rotation = std::atan2(-holeDir.z, holeDir.x) + (90.f * cro::Util::Const::degToRad);
                tx = glm::rotate(tx, rotation, glm::vec3(0.f, 1.f, 0.f));
            }
            else
            {
                tx = glm::rotate(tx, cro::Util::Random::value(-0.25f, 0.25f) + (rotation * cro::Util::Const::degToRad), glm::vec3(0.f, 1.f, 0.f));
            }

            float scale = static_cast<float>(cro::Util::Random::value(95, 110)) / 100.f;
            tx = glm::scale(tx, glm::vec3(scale));

            holeData.crowdPositions.push_back(tx);

            offsetPos.x += 0.3f + (static_cast<float>(cro::Util::Random::value(2, 5)) / 10.f);
            offsetPos.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;
        }        
    };


    //load the map data
    bool error = false;
    bool hasSpectators = false;
    auto mapDir = m_sharedData.mapDirectory.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    bool isUser = false;
    if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + mapPath))
    {
        mapPath = cro::App::getPreferencePath() + ConstVal::UserMapPath + mapDir + "/course.data";
        isUser = true;

        if (!cro::FileSystem::fileExists(mapPath))
        {
            LOG("Course file doesn't exist", cro::Logger::Type::Error);
            error = true;
        }
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath, !isUser))
    {
        error = true;
    }

    if (auto* title = courseFile.findProperty("title"); title)
    {
        m_courseTitle = title->getValue<cro::String>();
    }

    std::string skyboxPath;

    ThemeSettings theme;
    std::vector<std::string> holeStrings;
    const auto& props = courseFile.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "hole"
            && holeStrings.size() < MaxHoles)
        {
            holeStrings.push_back(prop.getValue<std::string>());
        }
        else if (name == "skybox")
        {
            skyboxPath = prop.getValue<std::string>();
        }
        else if (name == "shrubbery")
        {
            cro::ConfigFile shrubbery;
            if (shrubbery.loadFromFile(prop.getValue<std::string>()))
            {
                const auto& shrubProps = shrubbery.getProperties();
                for (const auto& sp : shrubProps)
                {
                    const auto& shrubName = sp.getName();
                    if (shrubName == "model")
                    {
                        theme.billboardModel = sp.getValue<std::string>();
                    }
                    else if (shrubName == "sprite")
                    {
                        theme.billboardSprites = sp.getValue<std::string>();
                    }
                    else if (shrubName == "treeset")
                    {
                        Treeset ts;
                        if (theme.treesets.size() < ThemeSettings::MaxTreeSets
                            && ts.loadFromFile(sp.getValue<std::string>()))
                        {
                            theme.treesets.push_back(ts);
                        }
                    }
                    else if (shrubName == "grass")
                    {
                        theme.grassColour = sp.getValue<cro::Colour>();
                    }
                    else if (shrubName == "grass_tint") 
                    {
                        theme.grassTint = sp.getValue<cro::Colour>();
                    }
                }
            }
        }
        else if (name == "audio")
        {
            auto audioPath = prop.getValue<std::string>();
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + audioPath))
            {
                m_audioPath = audioPath;
            }
        }
        else if (name == "instance_model")
        {
            theme.instancePath = prop.getValue<std::string>();
        }
    }
    
    //use old sprites if user so wishes
    if (m_sharedData.treeQuality == SharedStateData::Classic)
    {
        std::string classicModel;
        std::string classicSprites;

        //assume we have the correct file extension, because it's an invalid path anyway if not.
        classicModel = theme.billboardModel.substr(0,theme.billboardModel.find_last_of('.')) + "_low.cmt";
        classicSprites = theme.billboardSprites.substr(0,theme.billboardSprites.find_last_of('.')) + "_low.spt";

        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicModel))
        {
            classicModel.clear();
        }
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + classicSprites))
        {
            classicSprites.clear();
        }

        if (!classicModel.empty() && !classicSprites.empty())
        {
            theme.billboardModel = classicModel;
            theme.billboardSprites = classicSprites;
        }
    }

    theme.cloudPath = loadSkybox(skyboxPath, m_skyScene, m_resources, m_materialIDs[MaterialID::Horizon], m_materialIDs[MaterialID::CelTexturedSkinned]);

#ifdef CRO_DEBUG_
    auto& colours = m_skyScene.getSkyboxColours();
    topSky = colours.top.getVec4();
    bottomSky = colours.middle.getVec4();
#endif

    if (theme.billboardModel.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardModel))
    {
        LogE << "Missing or invalid billboard model definition" << std::endl;
        error = true;
    }
    if (theme.billboardSprites.empty()
        || !cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + theme.billboardSprites))
    {
        LogE << "Missing or invalid billboard sprite sheet" << std::endl;
        error = true;
    }
    if (holeStrings.empty())
    {
        LOG("No hole files in course data", cro::Logger::Type::Error);
        error = true;
    }

    //check the rules and truncate hole list
    //if requested - 1 front holes, 1 back holes
    if (m_sharedData.holeCount == 1)
    {
        auto size = std::max(std::size_t(1), holeStrings.size() / 2);
        holeStrings.resize(size);
    }
    else if (m_sharedData.holeCount == 2)
    {
        auto start = holeStrings.size() / 2;
        std::vector<std::string> newStrings(holeStrings.begin() + start, holeStrings.end());
        holeStrings.swap(newStrings);
    }

    //and reverse if rules request
    if (m_sharedData.reverseCourse)
    {
        std::reverse(holeStrings.begin(), holeStrings.end());
    }


    cro::ConfigFile holeCfg;
    cro::ModelDefinition modelDef(m_resources);
    std::string prevHoleString;
    cro::Entity prevHoleEntity;
    std::vector<cro::Entity> prevProps;
    std::vector<cro::Entity> prevParticles;
    std::vector<cro::Entity> prevAudio;
    std::vector<cro::Entity> leaderboardProps;
    std::int32_t holeModelCount = 0; //use this to get a guestimate of how many holes per model there are to adjust the camera offset

    cro::AudioScape propAudio;
    propAudio.loadFromFile("assets/golf/sound/props.xas", m_resources.audio);

    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + hole))
        {
            LOG("Hole file is missing", cro::Logger::Type::Error);
            error = true;
        }

        if (!holeCfg.loadFromFile(hole))
        {
            LOG("Failed opening hole file", cro::Logger::Type::Error);
            error = true;
        }

        static constexpr std::int32_t MaxProps = 6;
        std::int32_t propCount = 0;
        auto& holeData = m_holeData.emplace_back();
        bool duplicate = false;

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                auto path = holeProp.getValue<std::string>();
                if (!m_currentMap.loadFromFile(path)
                    || m_currentMap.getFormat() != cro::ImageFormat::RGBA)
                {
                    LogE << path << ": image file not RGBA" << std::endl;
                    error = true;
                }
                holeData.mapPath = holeProp.getValue<std::string>();
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                holeData.pin = holeProp.getValue<glm::vec3>();
                holeData.pin.x = glm::clamp(holeData.pin.x, 0.f, 320.f);
                holeData.pin.z = glm::clamp(holeData.pin.z, -200.f, 0.f);
                propCount++;
            }
            else if (name == "tee")
            {
                holeData.tee = holeProp.getValue<glm::vec3>();
                holeData.tee.x = glm::clamp(holeData.tee.x, 0.f, 320.f);
                holeData.tee.z = glm::clamp(holeData.tee.z, -200.f, 0.f);
                propCount++;
            }
            else if (name == "target")
            {
                holeData.target = holeProp.getValue<glm::vec3>();
                if (glm::length2(holeData.target) > 0)
                {
                    propCount++;
                }
            }
            else if (name == "par")
            {
                holeData.par = holeProp.getValue<std::int32_t>();
                if (holeData.par < 1 || holeData.par > 10)
                {
                    LOG("Invalid PAR value", cro::Logger::Type::Error);
                    error = true;
                }
                propCount++;
            }
            else if (name == "model")
            {
                auto modelPath = holeProp.getValue<std::string>();

                if (modelPath != prevHoleString)
                {
                    //attept to load model
                    if (modelDef.loadFromFile(modelPath))
                    {
                        holeData.modelPath = modelPath;

                        holeData.modelEntity = m_gameScene.createEntity();
                        holeData.modelEntity.addComponent<cro::Transform>().setPosition(OriginOffset);
                        holeData.modelEntity.getComponent<cro::Transform>().setOrigin(OriginOffset);
                        holeData.modelEntity.addComponent<cro::Callback>();
                        modelDef.createModel(holeData.modelEntity);
                        holeData.modelEntity.getComponent<cro::Model>().setHidden(true);
                        for (auto m = 0u; m < holeData.modelEntity.getComponent<cro::Model>().getMeshData().submeshCount; ++m)
                        {
                            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
                            applyMaterialData(modelDef, material, m);
                            holeData.modelEntity.getComponent<cro::Model>().setMaterial(m, material);
                        }
                        propCount++;

                        prevHoleString = modelPath;
                        prevHoleEntity = holeData.modelEntity;

                        holeModelCount++;
                    }
                    else
                    {
                        LOG("Failed loading model file", cro::Logger::Type::Error);
                        error = true;
                    }
                }
                else
                {
                    //duplicate the hole by copying the previous model entitity
                    holeData.modelPath = prevHoleString;
                    holeData.modelEntity = prevHoleEntity;
                    duplicate = true;
                    propCount++;
                }
            }
        }

        if (propCount != MaxProps)
        {
            LOG("Missing hole property", cro::Logger::Type::Error);
            error = true;
        }
        else
        {
            if (!duplicate) //this hole wasn't a duplicate of the previous
            {
                //look for prop models (are optional and can fail to load no problem)
                const auto& propObjs = holeCfg.getObjects();
                for (const auto& obj : propObjs)
                {
                    const auto& name = obj.getName();
                    if (name == "prop")
                    {
                        const auto& modelProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        float rotation = 0.f;
                        glm::vec3 scale(1.f);
                        std::string path;

                        std::vector<glm::vec3> curve;
                        bool loopCurve = true;
                        float loopDelay = 4.f;
                        float loopSpeed = 6.f;

                        std::string particlePath;
                        std::string emitterName;

                        for (const auto& modelProp : modelProps)
                        {
                            auto propName = modelProp.getName();
                            if (propName == "position")
                            {
                                position = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "model")
                            {
                                path = modelProp.getValue<std::string>();
                            }
                            else if (propName == "rotation")
                            {
                                rotation = modelProp.getValue<float>();
                            }
                            else if (propName == "scale")
                            {
                                scale = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "particles")
                            {
                                particlePath = modelProp.getValue<std::string>();
                            }
                            else if (propName == "emitter")
                            {
                                emitterName = modelProp.getValue<std::string>();
                            }
                        }

                        const auto modelObjs = obj.getObjects();
                        for (const auto& o : modelObjs)
                        {
                            if (o.getName() == "path")
                            {
                                const auto points = o.getProperties();
                                for (const auto& p : points)
                                {
                                    if (p.getName() == "point")
                                    {
                                        curve.push_back(p.getValue<glm::vec3>());
                                    }
                                    else if (p.getName() == "loop")
                                    {
                                        loopCurve = p.getValue<bool>();
                                    }
                                    else if (p.getName() == "delay")
                                    {
                                        loopDelay = std::max(0.f, p.getValue<float>());
                                    }
                                    else if (p.getName() == "speed")
                                    {
                                        loopSpeed = std::max(0.f, p.getValue<float>());
                                    }
                                }

                                break;
                            }
                        }

                        if (!path.empty()
                            && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + path))
                        {
                            if (modelDef.loadFromFile(path))
                            {
                                auto ent = m_gameScene.createEntity();
                                ent.addComponent<cro::Transform>().setPosition(position);
                                ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                                ent.getComponent<cro::Transform>().setScale(scale);
                                modelDef.createModel(ent);
                                if (modelDef.hasSkeleton())
                                {
                                    for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                    {
                                        auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                                        applyMaterialData(modelDef, texturedMat, i);
                                        ent.getComponent<cro::Model>().setMaterial(i, texturedMat);
                                    }

                                    auto& skel = ent.getComponent<cro::Skeleton>();
                                    if (!skel.getAnimations().empty())
                                    {
                                        //this is the default behaviour
                                        const auto& anims = skel.getAnimations();
                                        if (anims.size() == 1)
                                        {
                                            //ent.getComponent<cro::Skeleton>().play(0); // don't play this until unhidden
                                            skel.getAnimations()[0].looped = true;
                                            skel.setMaxInterpolationDistance(100.f);
                                        }
                                        //however spectator models need fancier animation
                                        //control... and probably a smaller interp distance
                                        else
                                        {
                                            //TODO we could improve this by disabling when hidden?
                                            ent.addComponent<cro::Callback>().active = true;
                                            ent.getComponent<cro::Callback>().function = SpectatorCallback(anims);
                                            ent.getComponent<cro::Callback>().setUserData<bool>(false);
                                            ent.addComponent<cro::CommandTarget>().ID = CommandID::Spectator;

                                            skel.setMaxInterpolationDistance(80.f);
                                        }
                                    }
                                }
                                else
                                {
                                    for (auto i = 0u; i < modelDef.getMaterialCount(); ++i)
                                    {
                                        auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                                        applyMaterialData(modelDef, texturedMat, i);
                                        ent.getComponent<cro::Model>().setMaterial(i, texturedMat);

                                        auto shadowMat = m_resources.materials.get(m_materialIDs[MaterialID::ShadowMap]);
                                        applyMaterialData(modelDef, shadowMat);
                                        //shadowMat.setProperty("u_alphaClip", 0.5f);
                                        ent.getComponent<cro::Model>().setShadowMaterial(i, shadowMat);
                                    }
                                }
                                ent.getComponent<cro::Model>().setHidden(true);
                                ent.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                                holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                                holeData.propEntities.push_back(ent);

                                //special case for leaderboard model, cos, y'know
                                if (cro::FileSystem::getFileName(path) == "leaderboard.cmt")
                                {
                                    leaderboardProps.push_back(ent);
                                }

                                //add path if it exists
                                if (curve.size() > 3)
                                {
                                    Path propPath;
                                    for (auto p : curve)
                                    {
                                        propPath.addPoint(p);
                                    }

                                    ent.addComponent<PropFollower>().path = propPath;
                                    ent.getComponent<PropFollower>().loop = loopCurve;
                                    ent.getComponent<PropFollower>().idleTime = loopDelay;
                                    ent.getComponent<PropFollower>().speed = loopSpeed;
                                    ent.getComponent<cro::Transform>().setPosition(curve[0]);
                                }

                                //add child particles if they exist
                                if (!particlePath.empty())
                                {
                                    cro::EmitterSettings settings;
                                    if (settings.loadFromFile(particlePath, m_resources.textures))
                                    {
                                        auto pEnt = m_gameScene.createEntity();
                                        pEnt.addComponent<cro::Transform>();
                                        pEnt.addComponent<cro::ParticleEmitter>().settings = settings;
                                        pEnt.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                        pEnt.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                        ent.getComponent<cro::Transform>().addChild(pEnt.getComponent<cro::Transform>());
                                        holeData.particleEntities.push_back(pEnt);
                                    }
                                }

                                //and child audio
                                if (propAudio.hasEmitter(emitterName))
                                {
                                    struct AudioCallbackData final
                                    {
                                        glm::vec3 prevPos = glm::vec3(0.f);
                                        float fadeAmount = 0.f;
                                        float currentVolume = 0.f;
                                    };

                                    auto audioEnt = m_gameScene.createEntity();
                                    audioEnt.addComponent<cro::Transform>();
                                    audioEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                                    auto baseVolume = audioEnt.getComponent<cro::AudioEmitter>().getVolume();
                                    audioEnt.addComponent<cro::CommandTarget>().ID = CommandID::AudioEmitter;
                                    audioEnt.addComponent<cro::Callback>().setUserData<AudioCallbackData>();
                                    
                                    if (ent.hasComponent<PropFollower>())
                                    {
                                        audioEnt.getComponent<cro::Callback>().function =
                                            [&, ent, baseVolume](cro::Entity e, float dt)
                                        {
                                            auto& [prevPos, fadeAmount, currentVolume] = e.getComponent<cro::Callback>().getUserData<AudioCallbackData>();
                                            auto pos = ent.getComponent<cro::Transform>().getPosition();
                                            auto velocity = (pos - prevPos) * 60.f; //frame time
                                            prevPos = pos;
                                            e.getComponent<cro::AudioEmitter>().setVelocity(velocity);

                                            const float speed = ent.getComponent<PropFollower>().speed + 0.001f; //prevent div0
                                            float pitch = std::min(1.f, glm::length2(velocity) / (speed * speed));
                                            e.getComponent<cro::AudioEmitter>().setPitch(pitch);

                                            //fades in when callback first started
                                            fadeAmount = std::min(1.f, fadeAmount + dt);

                                            //rather than just jump to volume, we move towards it for a
                                            //smoother fade
                                            auto targetVolume = (baseVolume * 0.1f) + (pitch * (baseVolume * 0.9f));
                                            auto diff = targetVolume - currentVolume;
                                            if (std::abs(diff) > 0.001f)
                                            {
                                                currentVolume += (diff * dt);
                                            }
                                            else
                                            {
                                                currentVolume = targetVolume;
                                            }

                                            e.getComponent<cro::AudioEmitter>().setVolume(currentVolume * fadeAmount);
                                        };
                                    }
                                    else
                                    {
                                        //add a dummy function which will still be updated on hole end to remove this ent
                                        audioEnt.getComponent<cro::Callback>().function = [](cro::Entity,float) {};
                                    }
                                    ent.getComponent<cro::Transform>().addChild(audioEnt.getComponent<cro::Transform>());
                                    holeData.audioEntities.push_back(audioEnt);
                                }
                            }
                        }
                    }
                    else if (name == "particles")
                    {
                        const auto& particleProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        std::string path;

                        for (auto particleProp : particleProps)
                        {
                            auto propName = particleProp.getName();
                            if (propName == "path")
                            {
                                path = particleProp.getValue<std::string>();
                            }
                            else if (propName == "position")
                            {
                                position = particleProp.getValue<glm::vec3>();
                            }
                        }

                        if (!path.empty())
                        {
                            cro::EmitterSettings settings;
                            if (settings.loadFromFile(path, m_resources.textures))
                            {
                                auto ent = m_gameScene.createEntity();
                                ent.addComponent<cro::Transform>().setPosition(position);
                                ent.addComponent<cro::ParticleEmitter>().settings = settings;
                                ent.getComponent<cro::ParticleEmitter>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));
                                ent.addComponent<cro::CommandTarget>().ID = CommandID::ParticleEmitter;
                                holeData.particleEntities.push_back(ent);
                            }
                        }
                    }
                    else if (name == "crowd")
                    {
                        const auto& modelProps = obj.getProperties();
                        glm::vec3 position(0.f);
                        float rotation = 0.f;

                        for (const auto& modelProp : modelProps)
                        {
                            auto propName = modelProp.getName();
                            if (propName == "position")
                            {
                                position = modelProp.getValue<glm::vec3>();
                            }
                            else if (propName == "rotation")
                            {
                                rotation = modelProp.getValue<float>();
                            }
                        }

                        std::vector<glm::vec3> curve;
                        const auto& modelObjs = obj.getObjects();
                        for (const auto& o : modelObjs)
                        {
                            if (o.getName() == "path")
                            {
                                const auto& points = o.getProperties();
                                for (const auto& p : points)
                                {
                                    if (p.getName() == "point")
                                    {
                                        curve.push_back(p.getValue<glm::vec3>());
                                    }
                                }

                                break;
                            }
                        }

                        if (curve.size() < 4)
                        {
                            addCrowd(holeData, position, rotation);
                        }
                        else
                        {
                            auto& spline = holeData.crowdCurves.emplace_back();
                            for (auto p : curve)
                            {
                                spline.addPoint(p);
                            }
                            hasSpectators = true;
                        }
                    }
                    else if (name == "speaker")
                    {
                        std::string emitterName;
                        glm::vec3 position = glm::vec3(0.f);

                        const auto& speakerProps = obj.getProperties();
                        for (const auto& speakerProp : speakerProps)
                        {
                            const auto& propName = speakerProp.getName();
                            if (propName == "emitter")
                            {
                                emitterName = speakerProp.getValue<std::string>();
                            }
                            else if (propName == "position")
                            {
                                position = speakerProp.getValue<glm::vec3>();
                            }
                        }

                        if (!emitterName.empty() &&
                            propAudio.hasEmitter(emitterName))
                        {
                            auto emitterEnt = m_gameScene.createEntity();
                            emitterEnt.addComponent<cro::Transform>().setPosition(position);
                            emitterEnt.addComponent<cro::AudioEmitter>() = propAudio.getEmitter(emitterName);
                            float baseVol = emitterEnt.getComponent<cro::AudioEmitter>().getVolume();
                            emitterEnt.getComponent<cro::AudioEmitter>().setVolume(0.f);
                            emitterEnt.addComponent<cro::Callback>().function =
                                [baseVol](cro::Entity e, float dt)
                            {
                                auto vol = e.getComponent<cro::AudioEmitter>().getVolume();
                                vol = std::min(baseVol, vol + dt);
                                e.getComponent<cro::AudioEmitter>().setVolume(vol);

                                if (vol == baseVol)
                                {
                                    e.getComponent<cro::Callback>().active = false;
                                }
                            };
                            holeData.audioEntities.push_back(emitterEnt);
                        }
                    }
                }

                prevProps = holeData.propEntities;
                prevParticles = holeData.particleEntities;
                prevAudio = holeData.audioEntities;
            }
            else
            {
                holeData.propEntities = prevProps;
                holeData.particleEntities = prevParticles;
                holeData.audioEntities = prevAudio;
            }

            //cos you know someone is dying to try and break the game :P
            if (holeData.pin != holeData.tee)
            {
                holeData.distanceToPin = glm::length(holeData.pin - holeData.tee);
            }
        }
        std::shuffle(holeData.crowdPositions.begin(), holeData.crowdPositions.end(), cro::Util::Random::rndEngine);
    }

    //add the dynamically updated model to any leaderboard props
    if (!leaderboardProps.empty())
    {
        if (md.loadFromFile("assets/golf/models/leaderboard_panel.cmt"))
        {
            for (auto lb : leaderboardProps)
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>();
                md.createModel(entity);
                
                auto material = m_resources.materials.get(m_materialIDs[MaterialID::Leaderboard]);
                material.setProperty("u_subrect", glm::vec4(0.f, 0.5f, 1.f, 0.5f));
                entity.getComponent<cro::Model>().setMaterial(0, material);
                m_leaderboardTexture.addTarget(entity);

                //updates the texture rect depending on hole number
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<std::size_t>(m_currentHole);
                entity.getComponent<cro::Callback>().function =
                    [&,lb](cro::Entity e, float)
                {
                    //the leaderboard might get tidies up on hole change
                    //so remove this ent if that's the case
                    if (lb.destroyed())
                    {
                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                    else
                    {
                        auto currentHole = e.getComponent<cro::Callback>().getUserData<std::size_t>();
                        if (currentHole != m_currentHole)
                        {
                            if (m_currentHole > 8)
                            {
                                e.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", glm::vec4(0.f, 0.f, 1.f, 0.5f));
                            }
                        }
                        currentHole = m_currentHole;
                        e.getComponent<cro::Model>().setHidden(lb.getComponent<cro::Model>().isHidden());
                    }
                };
                entity.getComponent<cro::Model>().setHidden(true);

                lb.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            }
        }
    }


    //remove holes which failed to load - TODO should we delete partially loaded props here?
    m_holeData.erase(std::remove_if(m_holeData.begin(), m_holeData.end(), 
        [](const HoleData& hd)
        {
            return !hd.modelEntity.isValid();
        }), m_holeData.end());

    //check the crowd positions on every hole and set the height
    for (auto& hole : m_holeData)
    {
        m_collisionMesh.updateCollisionMesh(hole.modelEntity.getComponent<cro::Model>().getMeshData());

        for (auto& m : hole.crowdPositions)
        {
            glm::vec3 pos = m[3];
            pos.x += MapSize.x / 2;
            pos.z -= MapSize.y / 2;

            auto result = m_collisionMesh.getTerrain(pos);
            m[3][1] = result.height;
        }

        for (auto& c : hole.crowdCurves)
        {
            for (auto& p : c.getPoints())
            {
                auto result = m_collisionMesh.getTerrain(p);
                p.y = result.height;
            }
        }

        //while we're here check if this is a putting
        //course by looking to see if the tee is on the green
        hole.puttFromTee = m_collisionMesh.getTerrain(hole.tee).terrain == TerrainID::Green;

        if (hole.puttFromTee)
        {
            auto& model = hole.modelEntity.getComponent<cro::Model>();
            auto matCount = model.getMeshData().submeshCount;
            for (auto i = 0u; i < matCount; ++i)
            {
                auto mat = model.getMaterialData(cro::Mesh::IndexData::Final, i);
                mat.setShader(*gridShader);
                model.setMaterial(i, mat);
            }
        }
        /*else
        {
            auto& model = hole.modelEntity.getComponent<cro::Model>();
            auto matCount = model.getMeshData().submeshCount;
            for (auto i = 0u; i < matCount; ++i)
            {
                model.setMaterialProperty(i, "u_holeHeight", hole.pin.y);
            }
        }*/
    }


    if (error)
    {
        m_sharedData.errorMessage = "Failed to load course data\nSee console for more information";
        requestStackPush(StateID::Error);
    }
    else
    {
        m_holeToModelRatio = static_cast<float>(holeModelCount) / m_holeData.size();

        if (hasSpectators)
        {
            loadSpectators();
        }

        m_depthMap.setModel(m_holeData[0]);
        m_depthMap.update(40);
    }


    m_terrainBuilder.create(m_resources, m_gameScene, theme);

    //terrain builder will have loaded some shaders from which we need to capture some uniforms
    shader = &m_resources.shaders.get(ShaderID::Terrain);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);

    shader = &m_resources.shaders.get(ShaderID::Slope);
    m_windBuffer.addShader(*shader);

    createClouds(theme.cloudPath);
    if (cro::SysTime::now().months() == 6)
    {
        buildBow();
    }

    //reserve the slots for each hole score
    for (auto& client : m_sharedData.connectionData)
    {
        for (auto& player : client.playerData)
        {
            player.score = 0;
            player.holeScores.clear();
            player.holeScores.resize(holeStrings.size());
            std::fill(player.holeScores.begin(), player.holeScores.end(), 0);
        }
    }

    for (auto& data : m_sharedData.timeStats)
    {
        data.totalTime = 0.f;
        data.holeTimes.clear();
        data.holeTimes.resize(holeStrings.size());
        std::fill(data.holeTimes.begin(), data.holeTimes.end(), 0.f);
    }

    initAudio(theme.treesets.size() > 2);
}

void GolfState::loadSpectators()
{
    cro::ModelDefinition md(m_resources);
    std::array modelPaths =
    {
        "assets/golf/models/spectators/01.cmt",
        "assets/golf/models/spectators/02.cmt",
        "assets/golf/models/spectators/03.cmt",
        "assets/golf/models/spectators/04.cmt"
    };


    for (auto i = 0; i < 2; ++i)
    {
        for (const auto& path : modelPaths)
        {
            if (md.loadFromFile(path))
            {
                for (auto j = 0; j < 3; ++j)
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>();
                    md.createModel(entity);
                    entity.getComponent<cro::Model>().setHidden(true);
                    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));

                    if (md.hasSkeleton())
                    {
                        auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
                        applyMaterialData(md, material);

                        glm::vec4 rect((1.f / 3.f) * j, 0.f, (1.f / 3.f), 1.f);
                        material.setProperty("u_subrect", rect);
                        entity.getComponent<cro::Model>().setMaterial(0, material);

                        auto& skel = entity.getComponent<cro::Skeleton>();
                        if (!skel.getAnimations().empty())
                        {
                            auto& spectator = entity.addComponent<Spectator>();
                            for(auto k = 0u; k < skel.getAnimations().size(); ++k)
                            {
                                if (skel.getAnimations()[k].name == "Walk")
                                {
                                    spectator.anims[Spectator::AnimID::Walk] = k;
                                }
                                else if (skel.getAnimations()[k].name == "Idle")
                                {
                                    spectator.anims[Spectator::AnimID::Idle] = k;
                                }
                            }

                            skel.setMaxInterpolationDistance(30.f);
                        }
                    }

                    m_spectatorModels.push_back(entity);
                }
            }
        }
    }

    std::shuffle(m_spectatorModels.begin(), m_spectatorModels.end(), cro::Util::Random::rndEngine);
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem<InterpolationType::Linear>>(mb);
    m_gameScene.addSystem<CloudSystem>(mb);
    m_gameScene.addSystem<ClientCollisionSystem>(mb, m_holeData, m_collisionMesh);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<SpectatorSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<PropFollowSystem>(mb, m_collisionMesh);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<VatAnimationSystem>(mb);
    m_gameScene.addSystem<BallAnimationSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem3D>(mb, 10.f); //water rings sprite :D
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<CameraFollowSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb)->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);
//#ifdef CRO_DEBUG_
    m_gameScene.addSystem<FpsCameraSystem>(mb);
//#endif
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    //m_gameScene.setSystemActive<InterpolationSystem<InterpolationType::Linear>>(false);
    m_gameScene.setSystemActive<CameraFollowSystem>(false);
#ifdef CRO_DEBUG_
    m_gameScene.setSystemActive<FpsCameraSystem>(false);
#endif

    m_gameScene.addDirector<GolfParticleDirector>(m_resources.textures);
    m_gameScene.addDirector<GolfSoundDirector>(m_resources.audio);


    if (m_sharedData.tutorial)
    {
        m_gameScene.addDirector<TutorialDirector>(m_sharedData, m_inputParser);
    }

    m_skyScene.addSystem<cro::CallbackSystem>(mb);
    m_skyScene.addSystem<cro::SkeletalAnimator>(mb);
    m_skyScene.addSystem<cro::CameraSystem>(mb);
    m_skyScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<NotificationSystem>(mb);
    m_uiScene.addSystem<FloatingTextSystem>(mb);
    m_uiScene.addSystem<MiniBallSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    m_trophyScene.addSystem<TrophyDisplaySystem>(mb);
    m_trophyScene.addSystem<cro::SpriteSystem3D>(mb, 300.f);
    m_trophyScene.addSystem<cro::SpriteAnimator>(mb);
    m_trophyScene.addSystem<cro::ParticleSystem>(mb);
    m_trophyScene.addSystem<cro::CameraSystem>(mb);
    m_trophyScene.addSystem<cro::ModelRenderer>(mb);
    m_trophyScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void GolfState::buildScene()
{
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

    auto holeEntity = entity; //each of these entities are added to the entity with CommandID::Hole - below

    md.loadFromFile("assets/golf/models/flag.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Flag;
    entity.addComponent<float>() = 0.f;
    md.createModel(entity);
    if (md.hasSkeleton())
    {
        entity.getComponent<cro::Skeleton>().play(0);
    }
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
    md.createModel(entity);

    auto beaconMat = m_resources.materials.get(m_materialIDs[MaterialID::Beacon]);
    applyMaterialData(md, beaconMat);

    entity.getComponent<cro::Model>().setMaterial(0, beaconMat);
    entity.getComponent<cro::Model>().setHidden(!m_sharedData.showBeacon);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colourRotation", m_sharedData.beaconColour);
    auto beaconEntity = entity;

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
    auto pos = m_holeData[0].tee;
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
    entity.getComponent<cro::Model>().setRenderFlags(~(/*RenderFlags::MiniGreen |*/ RenderFlags::MiniMap | RenderFlags::Reflection));


    //a 'fan' which shows max rotation
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    material.blendMode = cro::Material::BlendMode::Additive;
    material.enableDepthTest = false;
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_FAN));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeArc;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        float scale = m_currentPlayer.terrain != TerrainID::Green ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
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


    //when putting this shows the distance/power ratio
    material = m_resources.materials.get(m_materialIDs[MaterialID::PuttAssist]);
    material.enableDepthTest = true;
    material.setProperty("u_colourRotation", m_sharedData.beaconColour);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_TRIANGLE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::StrokeArc | CommandID::BeaconColour; //we can recycle this as it behaves (mostly) the same way
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap | RenderFlags::Reflection));
    entity.addComponent<cro::Transform>().setPosition(pos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        bool hidden = !m_sharedData.showPuttingPower
            || (m_currentPlayer.terrain != TerrainID::Green)
            || m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU;

        if (!hidden)
        {
            float scale = /*cro::Util::Easing::easeOutSine*/(m_inputParser.getPower()) * 0.85f; //bit of a fudge to try making the representation more accurate
            e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            //fade with proximity to hole
            auto dist = m_holeData[m_currentHole].pin - e.getComponent<cro::Transform>().getWorldPosition();
            float amount = 0.3f + (smoothstep(0.5f, 2.f, glm::length2(dist)) * 0.7f);
            cro::Colour c(amount, amount, amount); //additive blending so darker == more transparent
            e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);
        }
        e.getComponent<cro::Model>().setHidden(hidden);
    };
    
    verts.clear();
    indices.clear();
    //vertex colour of the beacon model. I don't remember why I chose this specifically,
    //but it needs to match so that the hue rotation in the shader outputs the same result
    c = glm::vec3(1.f,0.f,0.781f);

    auto j = 0u;
    for (auto i = 0.f; i < cro::Util::Const::TAU; i += (cro::Util::Const::TAU / 16.f))
    {
        auto x = std::cos(i) * Clubs[ClubID::Putter].getTarget(10000.f); //distance isn't calc's yet so make sure it's suitable large
        auto z = -std::sin(i) * Clubs[ClubID::Putter].getTarget(10000.f);

        verts.push_back(x);
        verts.push_back(Ball::Radius);
        verts.push_back(z);
        verts.push_back(c.r);
        verts.push_back(c.g);
        verts.push_back(c.b);
        verts.push_back(1.f);
        indices.push_back(j++);

        verts.push_back(x);
        verts.push_back(Ball::Radius + 0.3f);
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

    //draw the flag pole as a single line which can be
    //seen from a distance - hole and model are also attached to this
    material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrameCulled]);
    material.setProperty("u_colour", cro::Colour::White);
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Hole;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    entity.getComponent<cro::Transform>().addChild(holeEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(flagEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(beaconEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(arrowEntity.getComponent<cro::Transform>());

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
    meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(240.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    waterEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, -30.f });
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Water]));
    waterEnt.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniMap | RenderFlags::Refraction));
    waterEnt.addComponent<cro::Callback>().active = true;
    waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(m_holeData[0].pin);
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
    md.loadFromFile("assets/golf/models/tee_balls.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].tee);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Tee;
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    
    auto targetDir = m_holeData[m_currentHole].target - m_holeData[0].tee;
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

    //carts
    md.loadFromFile("assets/golf/models/cart.cmt");
    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    applyMaterialData(md, texturedMat, 1);
    std::array cartPositions =
    {
        glm::vec3(-0.4f, 0.f, -5.9f),
        glm::vec3(2.6f, 0.f, -6.9f),
        glm::vec3(2.2f, 0.f, 6.9f),
        glm::vec3(-1.2f, 0.f, 5.2f)
    };

    //add a cart for each connected client :3
    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
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
            entity.getComponent<cro::Model>().setMaterial(1, texturedMat);
            entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
            teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    //update the 3D view - applied on player cam and transition cam
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = std::floor(winSize.y / vpSize.y);
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_sharedData.antialias =
            m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples)
            && m_sharedData.multisamples != 0
            && !m_sharedData.pixelScale;

        auto invScale = (maxScale + 1.f) - scale;
        glCheck(glPointSize(invScale * BallPointSize));
        glCheck(glLineWidth(invScale));

        m_scaleBuffer.setData(invScale);

        m_resolutionUpdate.resolutionData.resolution = texSize / invScale;
        m_resolutionBuffer.setData(m_resolutionUpdate.resolutionData);


        //this lets the shader scale leaf billboards correctly
        if (m_sharedData.treeQuality == SharedStateData::High)
        {
            auto targetHeight = texSize.y;
            glUseProgram(m_resources.shaders.get(ShaderID::TreesetLeaf).getGLHandle());
            glUniform1f(m_resources.shaders.get(ShaderID::TreesetLeaf).getUniformID("u_targetHeight"), targetHeight);
        }

        //fetch this explicitly so the transition cam also gets the correct zoom
        float zoom = m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().fov;
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad * zoom, texSize.x / texSize.y, 0.1f, static_cast<float>(MapSize.x), shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    camEnt.addComponent<CameraFollower::ZoomData>().target = 1.f; //used to set zoom when putting.
    camEnt.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& cam = e.getComponent<cro::Camera>();
        auto& zoom =  e.getComponent<CameraFollower::ZoomData>();
        float diff = zoom.target - zoom.fov;

        CRO_ASSERT(zoom.target > 0, "");

        if (std::fabs(diff) > 0.001f)
        {
            zoom.fov += (diff * dt) * 3.f;
        }
        else
        {
            zoom.fov = zoom.target;
            e.getComponent<cro::Callback>().active = false;
        }

        auto fov = m_sharedData.fov * cro::Util::Const::degToRad * zoom.fov;
        cam.setPerspective(fov, cam.getAspectRatio(), 0.1f, static_cast<float>(MapSize.x), shadowQuality.cascadeCount);
        m_cameras[CameraID::Transition].getComponent<cro::Camera>().setPerspective(fov, cam.getAspectRatio(), 0.1f, static_cast<float>(MapSize.x), shadowQuality.cascadeCount);
    };

    m_cameras[CameraID::Player] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    static constexpr std::uint32_t ReflectionMapSize = 1024u;
    const std::uint32_t ShadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;

    //used by transition callback to interp camera
    camEnt.addComponent<TargetInfo>().waterPlane = waterEnt;
    camEnt.getComponent<TargetInfo>().targetLookAt = m_holeData[0].target;
    cam.reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    cam.reflectionBuffer.setSmooth(true);
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    cam.setMaxShadowDistance(shadowQuality.shadowFarDistance);
    cam.setShadowExpansion(20.f);

    //create an overhead camera
    auto setPerspective = [&](cro::Camera& cam)
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, /*vpSize.x*/320.f, shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition(DefaultSkycamPosition);
    camEnt.addComponent<cro::Camera>().resizeCallback = 
        [&,camEnt](cro::Camera& cam) //use explicit callback so we can capture the entity and use it to zoom via CamFollowSystem
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
            vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
            shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize / 2, ShadowMapSize / 2);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = SkyCamRadius * SkyCamRadius;
    camEnt.getComponent<CameraFollower>().maxOffsetDistance = 100.f;
    camEnt.getComponent<CameraFollower>().id = CameraID::Sky;
    camEnt.getComponent<CameraFollower>().zoom.target = 0.25f;// 0.1f;
    camEnt.getComponent<CameraFollower>().zoom.speed = SkyCamZoomSpeed;
    camEnt.getComponent<CameraFollower>().targetOffset = { 0.f,0.65f,0.f };
    camEnt.addComponent<cro::AudioListener>();

    //this holds the water plane ent when active
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Sky] = camEnt;


    //and a green camera
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&,camEnt](cro::Camera& cam)
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * camEnt.getComponent<CameraFollower>().zoom.fov,
            vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
            shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::CommandTarget>().ID = CommandID::SpectatorCam;
    camEnt.addComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
    camEnt.getComponent<CameraFollower>().id = CameraID::Green;
    camEnt.getComponent<CameraFollower>().maxTargetDiff = 16.f;
    camEnt.getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Green] = camEnt;

    //bystander cam (when remote or CPU player is swinging)
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&,camEnt](cro::Camera& cam)
    {
        //this cam has a slightly narrower FOV
        auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

        auto vpSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
            vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
            shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();

    CameraFollower::ZoomData zoomData;
    zoomData.speed = 3.f;
    zoomData.target = 0.45f;
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData);
    camEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& zoom = e.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>();

        zoom.progress = std::min(1.f, zoom.progress + (dt * zoom.speed));
        zoom.fov = glm::mix(1.f, zoom.target, cro::Util::Easing::easeInOutQuad(zoom.progress));
        e.getComponent<cro::Camera>().resizeCallback(e.getComponent<cro::Camera>());

        if (zoom.progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Bystander] = camEnt;


    //idle cam when player AFKs
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&, camEnt](cro::Camera& cam)
    {
        //this cam has a slightly narrower FOV
        auto zoomFOV = camEnt.getComponent<cro::Callback>().getUserData<CameraFollower::ZoomData>().fov;

        auto vpSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective((m_sharedData.fov * cro::Util::Const::degToRad) * zoomFOV * 0.7f,
            vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f,
            shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowNearDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(25.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    camEnt.addComponent<cro::Callback>().setUserData<CameraFollower::ZoomData>(zoomData); //needed by resize callback, but not used HUM
    camEnt.getComponent<cro::Callback>().active = true;
    camEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        if (e.getComponent<cro::Camera>().active)
        {
            static constexpr glm::vec3 TargetOffset(0.f, 1.f, 0.f);
            auto target = m_currentPlayer.position + TargetOffset;

            static float rads = 0.f;
            rads += (dt * 0.1f);
            glm::vec3 pos(std::cos(rads), 0.f, std::sin(rads));
            pos *= 5.f;
            pos += target;
            pos.y = m_collisionMesh.getTerrain(pos).height + 2.f;

            auto lookAt = glm::lookAt(pos, target, cro::Transform::Y_AXIS);
            e.getComponent<cro::Transform>().setLocalTransform(glm::inverse(lookAt));
        }
    };
    setPerspective(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().updateMatrices(camEnt.getComponent<cro::Transform>());
    m_cameras[CameraID::Idle] = camEnt;

    //fly-by cam for transition
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(45.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<TargetInfo>();
    updateView(camEnt.getComponent<cro::Camera>());
    m_cameras[CameraID::Transition] = camEnt;

//#ifdef CRO_DEBUG_
    //free cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback =
        [&,camEnt](cro::Camera& cam)
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, static_cast<float>(MapSize.x) * 1.25f, shadowQuality.cascadeCount);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    camEnt.getComponent<cro::Camera>().reflectionBuffer.create(ReflectionMapSize, ReflectionMapSize);
    camEnt.getComponent<cro::Camera>().reflectionBuffer.setSmooth(true);
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(15.f);
    camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<FpsCamera>();
    setPerspective(camEnt.getComponent<cro::Camera>());
    m_freeCam = camEnt;

#ifdef PATH_TRACING
    initBallDebug();
#endif
//#endif

    //drone model to follow camera
    createDrone();
   
    m_currentPlayer.position = m_holeData[m_currentHole].tee; //prevents the initial camera movement

    buildUI(); //put this here because we don't want to do this if the map data didn't load
    setCurrentHole(4); //need to do this here to make sure everything is loaded for rendering

    auto sunEnt = m_gameScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -130.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -75.f * cro::Util::Const::degToRad);

//#ifdef CRO_DEBUG_
    if (cro::SysTime::now().months() == 12
        && cro::Util::Random::value(0, 20) == 0)
    {
        createWeather();
    }
//#endif
}

void GolfState::initAudio(bool loadTrees)
{
    //6 evenly spaced points with ambient audio
    auto envOffset = glm::vec2(MapSize) / 3.f;
    cro::AudioScape as;
    if (as.loadFromFile(m_audioPath, m_resources.audio))
    {
        std::array emitterNames =
        {
            std::string("01"),
            std::string("02"),
            std::string("03"),
            std::string("04"),
            std::string("05"),
            std::string("06"),
            std::string("03"),
            std::string("04"),
        };

        for (auto i = 0; i < 2; ++i)
        {
            for (auto j = 0; j < 2; ++j)
            {
                static constexpr float height = 4.f;
                glm::vec3 position(envOffset.x * (i + 1), height, -envOffset.y * (j + 1));

                auto idx = i * 2 + j;

                if (as.hasEmitter(emitterNames[idx + 4]))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(position);
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx + 4]);
                    entity.getComponent<cro::AudioEmitter>().play();
                    entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(5.f));
                }

                position = { i * MapSize.x, height, -static_cast<float>(MapSize.y) * j };

                if (as.hasEmitter(emitterNames[idx]))
                {
                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(position);
                    entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx]);
                    entity.getComponent<cro::AudioEmitter>().play();
                }
            }
        }

        //random incidental audio
        if (as.hasEmitter("incidental01")
            && as.hasEmitter("incidental02"))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental01");
            entity.getComponent<cro::AudioEmitter>().setLooped(false);
            auto plane01 = entity;

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter("incidental02");
            entity.getComponent<cro::AudioEmitter>().setLooped(false);
            auto plane02 = entity;


            //we'll shoehorn the plane in here. won't make much sense
            //if the audioscape has different audio but hey...
            cro::ModelDefinition md(m_resources);
            cro::Entity planeEnt;
            if (md.loadFromFile("assets/golf/models/plane.cmt"))
            {
                static constexpr glm::vec3 Start(-32.f, 60.f, 20.f);
                static constexpr glm::vec3 End(352.f, 60.f, -220.f);

                entity = m_gameScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(Start);
                entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 32.f * cro::Util::Const::degToRad);
                entity.getComponent<cro::Transform>().setScale({ 0.01f, 0.01f, 0.01f });
                md.createModel(entity);

                entity.addComponent<cro::Callback>().function =
                    [](cro::Entity e, float dt)
                {
                    static constexpr float Speed = 10.f;
                    const float MaxLen = glm::length2((Start - End) / 2.f);

                    auto& tx = e.getComponent<cro::Transform>();
                    auto dir = glm::normalize(tx.getRightVector()); //scaling means this isn't normalised :/
                    tx.move(dir * Speed * dt);

                    float currLen = glm::length2((Start + ((Start + End) / 2.f)) - tx.getPosition());
                    float scale = std::max(1.f - (currLen / MaxLen), 0.001f); //can't scale to 0 because it breaks normalizing the right vector above
                    tx.setScale({ scale, scale, scale });

                    if (tx.getPosition().x > End.x)
                    {
                        tx.setPosition(Start);
                        e.getComponent<cro::Callback>().active = false;
                    }
                };

                auto material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                applyMaterialData(md, material);
                entity.getComponent<cro::Model>().setMaterial(0, material);

                //engine
                entity.addComponent<cro::AudioEmitter>(); //always needs one in case audio doesn't exist
                if (as.hasEmitter("plane"))
                {
                    entity.getComponent<cro::AudioEmitter>() = as.getEmitter("plane");
                    entity.getComponent<cro::AudioEmitter>().setLooped(false);
                }

                planeEnt = entity;
            }

            struct AudioData final
            {
                float currentTime = 0.f;
                float timeout = static_cast<float>(cro::Util::Random::value(32, 64));
                cro::Entity activeEnt;
            };

            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<AudioData>();
            entity.getComponent<cro::Callback>().function =
                [plane01, plane02, planeEnt](cro::Entity e, float dt) mutable
            {
                auto& [currTime, timeOut, activeEnt] = e.getComponent<cro::Callback>().getUserData<AudioData>();
                
                if (!activeEnt.isValid()
                    || activeEnt.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                {
                    currTime += dt;

                    if (currTime > timeOut)
                    {
                        currTime = 0.f;
                        timeOut = static_cast<float>(cro::Util::Random::value(120, 240));

                        auto id = cro::Util::Random::value(0, 2);
                        if (id == 0)
                        {
                            //fly the plane
                            if (planeEnt.isValid())
                            {
                                planeEnt.getComponent<cro::Callback>().active = true;
                                planeEnt.getComponent<cro::AudioEmitter>().play();
                                activeEnt = planeEnt;
                            }
                        }
                        else
                        {
                            auto ent = (id == 1) ? plane01 : plane02;
                            if (ent.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
                            {
                                ent.getComponent<cro::AudioEmitter>().play();
                                activeEnt = ent;
                            }
                        }
                    }
                }
            };
        }

        //put the new hole music on the cam for accessibilty
        //this is done *before* m_cameras is updated 
        if (as.hasEmitter("music"))
        {
            m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>() = as.getEmitter("music");
            m_gameScene.getActiveCamera().getComponent<cro::AudioEmitter>().setLooped(false);
        }
    }
    else
    {
        //still needs an emitter to stop crash playing non-loaded music
        m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>();
        LogE << "Invalid AudioScape file was found" << std::endl;
    }

    if (loadTrees)
    {
        const std::array<std::string, 3u> paths =
        {
            "assets/golf/sound/ambience/trees01.ogg",
            "assets/golf/sound/ambience/trees03.ogg",
            "assets/golf/sound/ambience/trees02.ogg"
        };

        /*const std::array positions =
        {
            glm::vec3(80.f, 4.f, -66.f),
            glm::vec3(240.f, 4.f, -66.f),
            glm::vec3(160.f, 4.f, -66.f),
            glm::vec3(240.f, 4.f, -123.f),
            glm::vec3(160.f, 4.f, -123.f),
            glm::vec3(80.f, 4.f, -123.f)
        };

        auto callback = [&](cro::Entity e, float)
        {
            float amount = std::min(1.f, m_windUpdate.currentWindSpeed);
            float pitch = 0.5f + (0.8f * amount);
            float volume = 0.05f + (0.3f * amount);

            e.getComponent<cro::AudioEmitter>().setPitch(pitch);
            e.getComponent<cro::AudioEmitter>().setVolume(volume);
        };

        //this works but... meh
        for (auto i = 0u; i < paths.size(); ++i)
        {
            if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + paths[i]))
            {
                for (auto j = 0u; j < 2u; ++j)
                {
                    auto id = m_resources.audio.load(paths[i], true);

                    auto entity = m_gameScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition(positions[i + (j * paths.size())]);
                    entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
                    entity.getComponent<cro::AudioEmitter>().setVolume(0.f);
                    entity.getComponent<cro::AudioEmitter>().setLooped(true);
                    entity.getComponent<cro::AudioEmitter>().setRolloff(0.f);
                    entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Effects);
                    entity.getComponent<cro::AudioEmitter>().play();

                    entity.addComponent<cro::Callback>().active = true;
                    entity.getComponent<cro::Callback>().function = callback;
                }
            }
        }*/
    }


    //fades in the audio
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + dt);

        cro::AudioMixer::setPrefadeVolume(cro::Util::Easing::easeOutQuad(progress), MixerChannel::Effects);

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };
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


            auto& [currRotation, acceleration, target] = e.getComponent<cro::Callback>().getUserData<DroneCallbackData>();

            //move towards skycam
            static constexpr float MoveSpeed = 20.f;
            static constexpr float MinRadius = MoveSpeed * MoveSpeed;
            static constexpr float AccelerationRadius = 40.f;

            auto movement = target.getComponent<cro::Transform>().getPosition() - oldPos;
            if (auto len2 = glm::length2(movement); len2 > MinRadius)
            {
                const float len = std::sqrt(len2);
                movement /= len;
                movement *= MoveSpeed;

                //go slower over short distances
                const float multiplier = 0.6f + (0.4f * std::min(1.f, len / AccelerationRadius));

                acceleration = std::min(1.f, acceleration + ((dt / 2.f) * multiplier));
                movement *= cro::Util::Easing::easeInSine(acceleration);

                currRotation += cro::Util::Maths::shortestRotation(currRotation, rotation) * dt;
            }
            else
            {
                acceleration = 0.f;
                currRotation = std::fmod(currRotation + (dt * 0.5f), cro::Util::Const::TAU);
            }
            e.getComponent<cro::Transform>().move(movement * dt);
            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, currRotation);

            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());

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
        targetEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            auto wind = m_windUpdate.currentWindSpeed * (m_windUpdate.currentWindVector * 0.3f);
            wind += m_windUpdate.currentWindVector * 0.7f;

            e.getComponent<cro::Transform>().move(wind * dt);

            if (!m_drone.isValid())
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };

        m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>().target = targetEnt;
        m_cameras[CameraID::Sky].getComponent<TargetInfo>().postProcess = nullptr;
    }
}

void GolfState::spawnBall(const ActorInfo& info)
{
    auto ballID = m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballID;

    //render the ball as a point so no perspective is applied to the scale
    cro::Colour miniBallColour;
    bool rollAnimation = true;
    auto material = m_resources.materials.get(m_ballResources.materialID);
    auto ball = std::find_if(m_sharedData.ballModels.begin(), m_sharedData.ballModels.end(),
        [ballID](const SharedStateData::BallInfo& ballPair)
        {
            return ballPair.uid == ballID;
        });
    if (ball != m_sharedData.ballModels.end())
    {
        material.setProperty("u_colour", ball->tint);
        miniBallColour = ball->tint;
        rollAnimation = ball->rollAnimation;
    }
    else
    {
        //this should at least line up with the fallback model
        material.setProperty("u_colour", m_sharedData.ballModels.cbegin()->tint);
        miniBallColour = m_sharedData.ballModels.cbegin()->tint;
    }

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    //entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.003f, 0.f }); //pushes the ent above the ground a bit to stop Z fighting
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>(
        InterpolationPoint(info.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(info.rotation), info.timestamp)).id = info.serverID;
    entity.addComponent<ClientCollider>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.ballMeshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);

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

    entity.addComponent<cro::ParticleEmitter>().settings.loadFromFile("assets/golf/particles/trail.cps", m_resources.textures);

    //cro::AudioScape propAudio;
    //propAudio.loadFromFile("assets/golf/sound/props.xas", m_resources.audio);
    //entity.addComponent<cro::AudioEmitter>() = propAudio.getEmitter("ball");
    //entity.getComponent<cro::AudioEmitter>().play();


    //ball shadow
    auto ballEnt = entity;
    material.setProperty("u_colour", cro::Colour::White);
    //material.blendMode = cro::Material::BlendMode::Multiply; //causes shadow to actually get darker as alpha reaches zero.. duh

    //point shadow seen from distance
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();// .setPosition(info.position);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt](cro::Entity e, float)
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
                || ballEnt.getComponent<cro::Transform>().getPosition() == m_holeData[m_currentHole].tee)
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
            }
        }
    };
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //large shadow seen close up
    auto shadowEnt = entity;
    entity = m_gameScene.createEntity();
    shadowEnt.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 0.003f, 0.f });
    m_modelDefs[ModelID::BallShadow]->createModel(entity);
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    //entity.getComponent<cro::Transform>().setScale(glm::vec3(1.3f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&,ballEnt](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        e.getComponent<cro::Model>().setHidden(ballEnt.getComponent<cro::Model>().isHidden());
        e.getComponent<cro::Transform>().setScale(ballEnt.getComponent<cro::Transform>().getScale()/* * 0.95f*/);
    };

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
            defaultBall->loadFromFile(m_sharedData.ballModels[0].modelPath);
        }

        //a bit dangerous assuming we're not empty, but we
        //shouldn't have made it this far if there are no ball files
        //as they are vetted by the menu state.
        LogW << "Ball with ID " << (int)ballID << " not found" << std::endl;
        defaultBall->createModel(entity);
        applyMaterialData(*m_ballModels.begin()->second, material);
    };

    material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    if (m_ballModels.count(ballID) != 0
        && ball != m_sharedData.ballModels.end())
    {
        if (!m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->loadFromFile(ball->modelPath);
        }

        if (m_ballModels[ballID]->isLoaded())
        {
            m_ballModels[ballID]->createModel(entity);
            applyMaterialData(*m_ballModels[ballID], material);
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
    entity.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //name label for the ball's owner
    glm::vec2 texSize(LabelTextureSize);
    texSize.y -= LabelIconSize.y;

    auto playerID = info.playerID;
    auto clientID = info.clientID;
    auto depthOffset = (clientID * 4) + playerID;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ texSize.x / 2.f, 0.f, -0.1f - (0.1f * depthOffset) });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>().setTexture(m_sharedData.nameTextures[info.clientID].getTexture());
    entity.getComponent<cro::Sprite>().setTextureRect({ 0.f, playerID * (texSize.y / 4.f), texSize.x, texSize.y / 4.f });
    entity.getComponent<cro::Sprite>().setColour(cro::Colour(1.f, 1.f, 1.f, 0.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, playerID, clientID](cro::Entity e, float dt)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }

        auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
        auto colour = e.getComponent<cro::Sprite>().getColour();

        auto position = ballEnt.getComponent<cro::Transform>().getPosition();
        position.y += Ball::Radius * 3.f;

        auto labelPos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(position, m_gameSceneTexture.getSize());
        const float halfWidth = m_gameSceneTexture.getSize().x / 2.f;
        
        /*static constexpr float MaxLabelOffset = 70.f;
        float diff = labelPos.x - halfWidth;
        if (diff < -1)
        {
            labelPos.x = std::min(labelPos.x, halfWidth - MaxLabelOffset);
        }
        else
        {
            labelPos.x = std::max(labelPos.x, halfWidth + MaxLabelOffset);
        }*/

        e.getComponent<cro::Transform>().setPosition(labelPos);

        if (terrain == TerrainID::Green)
        {
            if (m_currentPlayer.player == playerID
                && m_sharedData.clientConnection.connectionID == clientID)
            {
                 //set target fade to zero
                colour.setAlpha(std::max(0.f, colour.getAlpha() - dt));               
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
                //prevent blocking the view
                float halfPos = labelPos.x - halfWidth;
                float amount = std::min(1.f, std::max(0.f, std::abs(halfPos) / halfWidth));
                amount = 0.1f + (smoothstep(0.12f, 0.26f, amount) * 0.85f); //remember tex size is probably a lot wider than the window
                alpha *= amount;

                float currentAlpha = colour.getAlpha();
                colour.setAlpha(std::max(0.f, std::min(1.f, currentAlpha + (dt * cro::Util::Maths::sgn(alpha - currentAlpha)))));
            }
            e.getComponent<cro::Sprite>().setColour(colour);
        }
        else
        {
            colour.setAlpha(std::max(0.f, colour.getAlpha() - dt));
            e.getComponent<cro::Sprite>().setColour(colour);
        }

        float scale = m_sharedData.pixelScale ? 1.f : m_viewScale.x;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    m_courseEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //miniball for player
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(-0.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(0.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::MiniBall;
    entity.addComponent<MiniBall>().playerID = depthOffset;

    m_minimapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //and indicator icon when putting
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-2.5f, 6.5f), miniBallColour),
        cro::Vertex2D(glm::vec2(0.f), miniBallColour),
        cro::Vertex2D(glm::vec2(2.5f, 6.5), miniBallColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt, depthOffset](cro::Entity e, float)
    {
        if (m_currentPlayer.terrain == TerrainID::Green)
        {
            auto pos = ballEnt.getComponent<cro::Transform>().getWorldPosition();
            auto iconPos = m_greenCam.getComponent<cro::Camera>().coordsToPixel(pos, m_greenBuffer.getSize());

            static const glm::vec2 Centre(m_greenBuffer.getSize() / 2u);

            iconPos -= Centre;
            iconPos *= std::min(1.f, Centre.x / glm::length(iconPos));
            iconPos += Centre;

            e.getComponent<cro::Transform>().setPosition(glm::vec3(iconPos, depthOffset));


            auto terrain = ballEnt.getComponent<ClientCollider>().terrain;
            float scale = terrain == TerrainID::Green ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        }
    };

    m_miniGreenEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_sharedData.connectionData[info.clientID].playerData[info.playerID].ballTint = miniBallColour;

#ifdef CRO_DEBUG_
    ballEntity = ballEnt;
#endif
}

void GolfState::handleNetEvent(const net::NetEvent& evt)
{
    switch (evt.type)
    {
    case net::NetEvent::PacketReceived:
        switch (evt.packet.getID())
        {
        default: break;
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
        }
            break;
        case PacketID::Emote:
            showEmote(evt.packet.as<std::uint32_t>());
            break;
        case PacketID::MaxStrokes:
            showNotification("Stroke Limit Reached.");
            {
                auto* msg = postMessage<Social::SocialEvent>(Social::MessageID::SocialMessage);
                msg->type = Social::SocialEvent::PlayerAchievement;
                msg->level = 1; //sad sound
            }
            break;
        case PacketID::PingTime:
        {
            auto data = evt.packet.as<std::uint32_t>();
            auto pingTime = data & 0xffff;
            auto client = (data & 0xffff0000) >> 16;

            m_sharedData.connectionData[client].pingTime = pingTime;
        }
            break;
        case PacketID::CPUThink:
        {
            auto direction = evt.packet.as<std::uint8_t>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::ThinkBubble;
            cmd.action = [direction](cro::Entity e, float)
            {
                auto& [dir, _] = e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>();
                dir = direction;
                e.getComponent<cro::Callback>().active = true;
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::ReadyQuitStatus:
            m_readyQuitFlags = evt.packet.as<std::uint8_t>();
            break;
        case PacketID::AchievementGet:
            notifyAchievement(evt.packet.as<std::array<std::uint8_t, 2u>>());
            break;
        case PacketID::ServerAchievement:
        {
            auto [client, achID] = evt.packet.as<std::array<std::uint8_t, 2u>>();
            if (client == m_sharedData.localConnectionData.connectionID
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

                //inflate this so that the message board is correct - the update will come
                //in to assert this is correct afterwards
                m_sharedData.connectionData[client].playerData[player].holeScores[m_currentHole]++;
                showMessageBoard(MessageBoardID::Gimme);

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
                }
            }
            break;
        case PacketID::BallLanded:
        {
            auto update = evt.packet.as<BallUpdate>();
            switch (update.terrain)
            {
            default: break;
            case TerrainID::Bunker:
                showMessageBoard(MessageBoardID::Bunker);
                break;
            case TerrainID::Scrub:
                showMessageBoard(MessageBoardID::Scrub);
                m_hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Water:
                showMessageBoard(MessageBoardID::Water);
                m_hadFoul = (m_currentPlayer.client == m_sharedData.clientConnection.connectionID
                    && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU);
                break;
            case TerrainID::Hole:
                

                if (m_sharedData.tutorial)
                {
                    Achievements::awardAchievement(AchievementStrings[AchievementID::CluedUp]);
                }

                //check if this is our own score
                bool special = false;
                if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
                {
                    std::int32_t score = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole];
                    if (score == 1)
                    {
                        //achievement is awarded in showMessageBoard() where other achievements/stats are updated.
                        //Achievements::awardAchievement(AchievementStrings[AchievementID::HoleInOne]);
                        auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                        msg->type = GolfEvent::HoleInOne;
                        msg->position = m_holeData[m_currentHole].pin;
                    }

                    if (m_currentHole == 17)
                    {
                        //just completed the course
                        Achievements::incrementStat(StatStrings[StatID::HolesPlayed]);
                        Achievements::awardAchievement(AchievementStrings[AchievementID::JoinTheClub]);
                        Social::awardXP(XPValues[XPID::CompleteCourse]);
                    }

                    //check putt distance / if this was in fact a putt
                    if (getClub() == ClubID::Putter)
                    {
                        if (glm::length(update.position - m_currentPlayer.position) > LongPuttDistance)
                        {
                            Achievements::incrementStat(StatStrings[StatID::LongPutts]);
                            Achievements::awardAchievement(AchievementStrings[AchievementID::PuttStar]);
                            Social::awardXP(XPValues[XPID::Special] / 2);
                            special = true;
                        }
                    }
                    else
                    {
                        Achievements::awardAchievement(AchievementStrings[AchievementID::TopChip]);
                        Social::awardXP(XPValues[XPID::Special]);
                    }
                }
                showMessageBoard(MessageBoardID::HoleScore, special);

                break;
            }

            auto* msg = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg->type = GolfEvent::BallLanded;
            msg->terrain = update.position.y <= WaterLevel ? TerrainID::Water : update.terrain;
            msg->club = getClub();
            msg->travelDistance = glm::length2(update.position - m_currentPlayer.position);
            msg->pinDistance = glm::length2(update.position - m_holeData[m_currentHole].pin);

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
            updateActor( evt.packet.as<ActorInfo>());            
            break;
        case PacketID::ActorAnimation:
        {
            if (m_activeAvatar)
            {
                auto animID = evt.packet.as<std::uint8_t>();

                /*if (animID == AnimationID::Swing)
                {
                    if(m_inputParser.getPower() * Clubs[getClub()].target < 20.f)
                    {
                        animID = AnimationID::Chip;
                    }
                }*/

                m_activeAvatar->model.getComponent<cro::Skeleton>().play(m_activeAvatar->animationIDs[animID], 1.f, 0.4f);
            }
        }
            break;
        case PacketID::WindDirection:
            updateWindDisplay(cro::Util::Net::decompressVec3(evt.packet.as<std::array<std::int16_t, 3u>>()));
            break;
        case PacketID::SetHole:
            setCurrentHole(evt.packet.as<std::uint16_t>());
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
                }
            }
        }
            break;
        case PacketID::HoleWon:
        if(m_sharedData.scoreType != ScoreType::Stroke)
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            auto client = (data & 0xff00) >> 8;
            auto player = (data & 0x00ff);

            if (client == 255)
            {
                if (m_sharedData.scoreType == ScoreType::Skins)
                {
                    showNotification("Skins pot increased to " + std::to_string(player));
                }
                else
                {
                    showNotification("Hole was drawn");
                }

                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::HoleDrawn;
            }
            else
            {
                auto txt = m_sharedData.connectionData[client].playerData[player].name;
                if (m_sharedData.scoreType == ScoreType::Match)
                {
                    txt += " has won the hole!";
                }
                else
                {
                    txt += " has won the pot!";
                }
                showNotification(txt);

                auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
                msg->type = GolfEvent::HoleWon;
                msg->player = player;
                msg->client = client;
            }
        }
        break;
        case PacketID::GameEnd:
            showCountdown(evt.packet.as<std::uint8_t>());
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
            cmd.action = [&,idx](cro::Entity e, float)
            {
                if (e.getComponent<InterpolationComponent<InterpolationType::Linear>>().id == idx)
                {
                    m_gameScene.destroyEntity(e);
                    LOG("Packet removed ball entity", cro::Logger::Type::Warning);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        }
        break;
    case net::NetEvent::ClientDisconnect:
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(StateID::Error);
        break;
    default: break;
    }
}

void GolfState::removeClient(std::uint8_t clientID)
{
    //tidy up minimap
    for (auto i = 0u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        auto pid = clientID * 4 + i;
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::MiniBall;
        cmd.action = [&,pid](cro::Entity e, float)
        {
            if (e.getComponent<MiniBall>().playerID == pid)
            {
                m_uiScene.destroyEntity(e);
            }
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
}

void GolfState::setCurrentHole(std::uint16_t holeInfo)
{
    std::uint8_t hole = (holeInfo & 0xff00) >> 8;
    m_holeData[hole].par = (holeInfo & 0x00ff);

    if (hole != m_currentHole
        && m_sharedData.logBenchmarks)
    {
        dumpBenchmark();
    }

    //update all the total hole times
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        m_sharedData.timeStats[i].totalTime += m_sharedData.timeStats[i].holeTimes[m_currentHole];

        Achievements::incrementStat(StatStrings[StatID::TimeOnTheCourse], m_sharedData.timeStats[i].holeTimes[m_currentHole]);
    }

    updateScoreboard();
    m_hadFoul = false;

    //CRO_ASSERT(hole < m_holeData.size(), "");
    if (hole >= m_holeData.size())
    {
        m_sharedData.errorMessage = "Server requested hole\nnot found";
        requestStackPush(StateID::Error);
        return;
    }

    m_terrainBuilder.update(hole);
    m_gameScene.getSystem<ClientCollisionSystem>()->setMap(hole);
    m_collisionMesh.updateCollisionMesh(m_holeData[hole].modelEntity.getComponent<cro::Model>().getMeshData());

    //create hole model transition
    bool rescale = (hole == 0) || (m_holeData[hole - 1].modelPath != m_holeData[hole].modelPath);
    auto* propModels = &m_holeData[m_currentHole].propEntities;
    auto* particles = &m_holeData[m_currentHole].particleEntities;
    auto* audio = &m_holeData[m_currentHole].audioEntities;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().active = true;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().function =
        [&, propModels, particles, audio, rescale](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + (dt / 2.f));

        if (rescale)
        {
            float scale = 1.f - progress;
            e.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
            m_waterEnt.getComponent<cro::Transform>().setScale({ scale, scale, scale });
        }

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(rescale);

            for (auto i = 0u; i < propModels->size(); ++i)
            {
                //if we're not rescaling we're recycling the model so don't hide its props
                propModels->at(i).getComponent<cro::Model>().setHidden(rescale);
            }

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
            }

            //index should be updated by now (as this is a callback)
            //so we're actually targetting the next hole entity
            auto entity = m_holeData[m_currentHole].modelEntity;
            entity.getComponent<cro::Model>().setHidden(false);

            if (rescale)
            {
                entity.getComponent<cro::Transform>().setScale({ 0.f, 1.f, 0.f });
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
            }
        }
    };

    m_currentHole = hole;
    //m_inputParser.setMaxRotation(m_holeData[m_currentHole].puttFromTee ? MaxPuttRotation : MaxRotation); //pretty sure this is overridden in setCurrentPlayer()...
    startFlyBy(); //requires current hole

    //restore the drone if someone hit it
    if (!m_drone.isValid())
    {
        createDrone();
    }

    //set putting grid values
    float height = m_holeData[m_currentHole].pin.y;
    glUseProgram(m_gridShader.shaderID);
    glUniform1f(m_gridShader.minHeight, height - 0.025f);
    glUniform1f(m_gridShader.maxHeight, height + 0.08f);
    glUseProgram(0);

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
            cmd.action = [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);
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
                auto terrain = m_collisionMesh.getTerrain(pos);
                pos.y = terrain.height + 0.0001f;

                en.getComponent<cro::Transform>().setPosition(pos);
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
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
    m_currentPlayer.terrain = TerrainID::Fairway; //this will be overwritten from the server but setting this to non-green makes sure the mini cam stops updating in time
    m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin); //limits club selection based on hole size

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
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = "Hole: " + std::to_string(m_currentHole + 1);
        e.getComponent<cro::Callback>().active = true;
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

    auto title = m_sharedData.tutorial ? cro::String("Tutorial").toUtf8() : m_courseTitle.substr(0, MaxTitleLen).toUtf8();
    auto holeNumber = std::to_string(m_currentHole + 1);
    auto holeTotal = std::to_string(m_holeData.size());
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
}

void GolfState::setCameraPosition(glm::vec3 position, float height, float viewOffset)
{
    static constexpr float MinDist = 6.f;
    static constexpr float MaxDist = 270.f;
    static constexpr float DistDiff = MaxDist - MinDist;
    float heightMultiplier = 1.f; //goes to -1.f at max dist

    auto camEnt = m_cameras[CameraID::Player];
    auto& targetInfo = camEnt.getComponent<TargetInfo>();
    auto target = targetInfo.currentLookAt - position;

    auto dist = glm::length(target);
    float distNorm = std::min(1.f, (dist - MinDist) / DistDiff);
    heightMultiplier -= (2.f * distNorm);

    target *= 1.f - ((1.f - 0.08f) * distNorm);
    target += position;

    auto result = m_collisionMesh.getTerrain(position);

    camEnt.getComponent<cro::Transform>().setPosition({ position.x, result.height + height, position.z });


    auto currentCamTarget = glm::vec3(target.x, result.height + (height * heightMultiplier), target.z);
    camEnt.getComponent<TargetInfo>().finalLookAt = currentCamTarget;

    auto oldPos = camEnt.getComponent<cro::Transform>().getPosition();
    auto lookat = glm::lookAt(oldPos, currentCamTarget, cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(lookat));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * viewOffset);

    //clamp above ground height and hole radius
    auto newPos = camEnt.getComponent<cro::Transform>().getPosition();

    static constexpr float MinRad = 0.6f + CameraPuttOffset;
    static constexpr float MinRadSqr = MinRad * MinRad;

    auto holeDir = m_holeData[m_currentHole].pin - newPos;
    auto holeDist = glm::length2(holeDir);
    /*if (holeDist < MinRadSqr)
    {
        auto len = std::sqrt(holeDist);
        auto move = MinRad - len;
        holeDir /= len;
        holeDir *= move;
        newPos -= holeDir;
    }*/

    //lower height as we get closer to hole
    heightMultiplier = std::min(1.f, std::max(0.f, holeDist / MinRadSqr));
    //if (!m_holeData[m_currentHole].puttFromTee)
    {
        heightMultiplier *= CameraTeeMultiplier;
    }

    auto groundHeight = m_collisionMesh.getTerrain(newPos).height;
    newPos.y = std::max(groundHeight + (CameraPuttHeight * heightMultiplier), newPos.y);

    camEnt.getComponent<cro::Transform>().setPosition(newPos);
    
    //hmm this stops the putt cam jumping when the position has been offset
    //however the look at point is no longer the hole, so the rotation
    //is weird and we need to interpolate back through the offset
    camEnt.getComponent<TargetInfo>().finalLookAt += newPos - oldPos;
    camEnt.getComponent<TargetInfo>().finalLookAtOffset = newPos - oldPos;

    //also updated by camera follower...
    if (targetInfo.waterPlane.isValid())
    {
        targetInfo.waterPlane.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
    }
}

void GolfState::requestNextPlayer(const ActivePlayer& player)
{
    if (!m_sharedData.tutorial)
    {
        m_currentPlayer = player;
        //setCurrentPlayer() is called when the sign closes

        showMessageBoard(MessageBoardID::PlayerName);
        showScoreboard(false);
    }
    else
    {
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
    m_turnTimer.restart();
    m_idleTimer.restart();
    idleTime = cro::seconds(90.f);

    m_gameScene.getDirector<GolfSoundDirector>()->setActivePlayer(player.client, player.player);
    m_avatars[player.client][player.player].ballModel.getComponent<cro::Transform>().setScale(glm::vec3(1.f));

    m_resolutionUpdate.targetFade = player.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

    updateScoreboard();
    showScoreboard(false);

    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);
    auto isCPU = m_sharedData.localConnectionData.playerData[player.player].isCPU;

    m_sharedData.inputBinding.playerID = localPlayer ? player.player : 0; //this also affects who can emote, so if we're currently emoting when it's not our turn always be player 0(??)
    m_inputParser.setActive(localPlayer && !m_photoMode, isCPU);
    m_restoreInput = localPlayer; //if we're in photo mode should we restore input parser?
    Achievements::setActive(localPlayer && !isCPU && allowAchievements);

    if (player.terrain == TerrainID::Bunker)
    {
        m_inputParser.setMaxClub(ClubID::PitchWedge);
    }
    else
    {
        m_inputParser.setMaxClub(m_holeData[m_currentHole].distanceToPin);
    }

    //player UI name
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].name;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerIcon;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
        e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PinDistance;
    cmd.action =
        [&](cro::Entity e, float)
    {
        formatDistanceString(m_distanceToHole, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements);

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action =
        [&,player](cro::Entity e, float)
    {
        //e.getComponent<cro::Transform>().setPosition(glm::vec3(toMinimapCoords(player.position), 0.1f));

        auto pid = player.client * 4 + player.player;

        if (e.getComponent<MiniBall>().playerID == pid)
        {
            //play the callback animation
            e.getComponent<MiniBall>().state = MiniBall::Animating;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show ui if this is our client    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [&,localPlayer](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = localPlayer ? 0 : 1;
        e.getComponent<cro::Callback>().active = true;
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
        e.getComponent<cro::Model>().setDepthTestEnabled(0, player.terrain == TerrainID::Green);

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
    
    auto rotation = isCPU ? MaxRotation / 3.f : MaxRotation;
    m_inputParser.setMaxRotation(m_holeData[m_currentHole].puttFromTee ? MaxPuttRotation : 
        player.terrain == TerrainID::Green ? rotation / 3.f : rotation);


    //set this separately because target might not necessarily be the pin.
    //if (m_currentPlayer != player)
    {
        m_inputParser.setClub(glm::length(m_holeData[m_currentHole].pin - player.position));
    }

    //check if input is CPU
    if (localPlayer
        && isCPU)
    {
        //if the player can rotate enough prefer the hole as the target
        auto pin = m_holeData[m_currentHole].pin - player.position;
        auto targetPoint = target - player.position;

        auto p = glm::normalize(glm::vec2(pin.x, -pin.z));
        auto t = glm::normalize(glm::vec2(targetPoint.x, -targetPoint.z));

        float dot = glm::dot(p, t);
        float det = (p.x * t.y) - (p.y * t.x);
        float targetAngle = std::abs(std::atan2(det, dot));

        if (targetAngle < m_inputParser.getMaxRotation())
        {
            m_cpuGolfer.activate(m_holeData[m_currentHole].pin);
        }
        else
        {
            m_cpuGolfer.activate(target);
        }
#ifdef CRO_DEBUG_
        //CPUTarget.getComponent<cro::Transform>().setPosition(m_cpuGolfer.getTarget());
#endif
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
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);


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

        auto tx = glm::inverse(glm::lookAt(eye, target, cro::Transform::Y_AXIS));
        m_cameras[CameraID::Bystander].getComponent<cro::Transform>().setLocalTransform(tx);

        //if this is a CPU player or a remote player, show a bystander cam automatically
        if (player.client != m_sharedData.localConnectionData.connectionID
            || 
            (player.client == m_sharedData.localConnectionData.connectionID &&
            m_sharedData.localConnectionData.playerData[player.player].isCPU))
        {
            static constexpr float MinCamDist = 25.f;
            if (cro::Util::Random::value(0,2) != 0 &&
                glm::length2(player.position - m_holeData[m_currentHole].pin) > (MinCamDist * MinCamDist))
            {
                auto entity = m_gameScene.createEntity();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(2.7f);
                entity.getComponent<cro::Callback>().function =
                    [&/*, player, playerRotation*/](cro::Entity e, float dt)
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
        m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowNearDistance);
    }
    setActiveCamera(CameraID::Player);

    //show or hide the slope indicator depending if we're on the green
    //or if we're on a putting map (in which case we're using the contour material)
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [&,player](cro::Entity e, float)
    {
        bool hidden = (player.terrain != TerrainID::Green) || m_holeData[m_currentHole].puttFromTee;

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

    //this is just so that the particle director knows if we're on a new hole
    if (m_currentPlayer.position == m_holeData[m_currentHole].tee)
    {
        msg2->travelDistance = -1.f;
    }

    m_gameScene.setSystemActive<CameraFollowSystem>(false);

    const auto setCamTarget = [&](glm::vec3 pos)
    {
        if (m_drone.isValid())
        {
            auto& data = m_drone.getComponent<cro::Callback>().getUserData<DroneCallbackData>();
            data.target.getComponent<cro::Transform>().setPosition(pos);
            //data.canRetarget = true;
        }
        else
        {
            m_cameras[CameraID::Sky].getComponent<cro::Transform>().setPosition(pos);
        }
    };

    //see where the player is and move the sky cam if possible
    //else set it to the default position
    static constexpr float MinPlayerDist = 40.f * 40.f; //TODO should this be the sky cam radius^2?
    auto dir = m_holeData[m_currentHole].pin - player.position;
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
        auto pos = m_holeData[m_currentHole].puttFromTee ? glm::vec3(0.f, SkyCamHeight, 0.f) : DefaultSkycamPosition;
        setCamTarget(pos);
    }

    if (!m_holeData[m_currentHole].puttFromTee)
    {
        setGreenCamPosition();
    }

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

    /*if (player.terrain != TerrainID::Green
        || m_holeData[m_currentHole].puttFromTee)
    {
        updateMiniMap();
    }*/
}

void GolfState::setGreenCamPosition()
{
    auto holePos = m_holeData[m_currentHole].pin;

    m_cameras[CameraID::Green].getComponent<cro::Transform>().setPosition({ holePos.x, GreenCamHeight, holePos.z });

    float heightOffset = GreenCamHeight;

    if (m_holeData[m_currentHole].puttFromTee)
    {
        auto direction = holePos - m_holeData[m_currentHole].tee;
        direction = glm::normalize(direction) * 2.f;
        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = 4.5f * 4.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = 0.8f;
        heightOffset *= 0.18f;
    }
    else if (auto direction = holePos - m_currentPlayer.position; m_currentPlayer.terrain == TerrainID::Green)
    {
        direction = glm::normalize(direction) * 4.2f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusSmall * GreenCamRadiusSmall;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 1.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().isSnapped = true; //this actually gets overridden when camera is set active...
        m_cameras[CameraID::Green].getComponent<CameraFollower>().maxTargetDiff = 4.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.3f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow;

        heightOffset *= 0.15f;
    }

    else if ((glm::length2(direction) < (50.f * 50.f)))
    {
        //player is within larger radius
        direction = glm::normalize(direction) * 5.5f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.5f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusMedium * GreenCamRadiusMedium;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 9.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().isSnapped = true; //this actually gets overridden when camera is set active...
        m_cameras[CameraID::Green].getComponent<CameraFollower>().maxTargetDiff = 8.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.5f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomSlow / 2.f;

        heightOffset *= 0.15f;
    }

    else
    {
        direction = glm::normalize(direction) * 15.f;

        auto r = (cro::Util::Random::value(0, 1) * 2) - 1;
        direction = glm::rotate(direction, 0.35f * static_cast<float>(r), glm::vec3(0.f, 1.f, 0.f));

        m_cameras[CameraID::Green].getComponent<cro::Transform>().move(direction);
        m_cameras[CameraID::Green].getComponent<CameraFollower>().radius = GreenCamRadiusLarge * GreenCamRadiusLarge;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoomRadius = 16.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().isSnapped = false;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().maxTargetDiff = 16.f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.target = 0.25f;
        m_cameras[CameraID::Green].getComponent<CameraFollower>().zoom.speed = GreenCamZoomFast;
    }

    //double check terrain height and make sure camera is always above
    auto result = m_collisionMesh.getTerrain(m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition());
    result.height = std::max(result.height, holePos.y);
    result.height += heightOffset;

    auto pos = m_cameras[CameraID::Green].getComponent<cro::Transform>().getPosition();
    pos.y = result.height;

    auto tx = glm::inverse(glm::lookAt(pos, m_holeData[m_currentHole].pin, cro::Transform::Y_AXIS));
    m_cameras[CameraID::Green].getComponent<cro::Transform>().setLocalTransform(tx);
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
    auto power = Clubs[club].getPower(m_distanceToHole) * powerPct;

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= power;
    impulse *= Dampening[m_currentPlayer.terrain];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::BallPrediction, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void GolfState::hitBall()
{
    auto club = getClub();

    auto pitch = Clubs[club].getAngle(); //TODO lower / raise with top/back spin

    auto yaw = m_inputParser.getYaw();

    auto power = Clubs[club].getPower(m_distanceToHole);

    //add hook/slice to yaw
    auto hook = m_inputParser.getHook();
    if (club != ClubID::Putter)
    {
        auto s = cro::Util::Maths::sgn(hook);
        //changing this func changes how accurate a player needs to be
        //sine, quad, cubic, quart, quint in steepness order
        if (Achievements::getActive())
        {
            auto level = Social::getLevel();
            switch (level / 25)
            {
            default:
                hook = cro::Util::Easing::easeOutQuint(hook * s) * s;
                break;
            case 3:
                hook = cro::Util::Easing::easeOutQuart(hook * s) * s;
                break;
            case 2:
                hook = cro::Util::Easing::easeOutCubic(hook * s) * s;
                break;
            case 1:
                hook = cro::Util::Easing::easeOutQuad(hook * s) * s;
                break;
            case 0:
                hook = cro::Util::Easing::easeOutSine(hook * s) * s;
                break;
            }
        }
        else
        {
            hook = cro::Util::Easing::easeOutQuad(hook * s) * s;
        }

        power *= cro::Util::Easing::easeOutSine(m_inputParser.getPower());
    }
    else
    {
        power *= m_inputParser.getPower();
    }
    yaw += MaxHook * hook;

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= power;
    impulse *= Dampening[m_currentPlayer.terrain];
    impulse *= godmode;

    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_inputParser.setActive(false);
    m_restoreInput = false;

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

    if (m_currentCamera == CameraID::Bystander
        && cro::Util::Random::value(0,1) == 0)
    {
        //activate the zoom
        m_cameras[CameraID::Bystander].getComponent<cro::Callback>().active = true;
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
                //glm::quat(1.f, 0.f, 0.f, 0.f)
                interp.addPoint({ update.position, /*update.velocity*/glm::vec3(0.f), cro::Util::Net::decompressQuat(update.rotation), update.timestamp});


                //e.getComponent<cro::Transform>().setPosition(update.position);

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
                }
            }

            e.getComponent<ClientCollider>().active = active;
            e.getComponent<ClientCollider>().state = update.state;

            if (update.state != static_cast<std::uint8_t>(Ball::State::Flight))
            {
                e.getComponent<cro::ParticleEmitter>().stop();
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    if (update == m_currentPlayer)
    {
        //set the green cam zoom as appropriate
        float ballDist = glm::length(update.position - m_holeData[m_currentHole].pin);

#ifdef PATH_TRACING
        updateBallDebug(update.position);
#endif // CRO_DEBUG_

        m_greenCam.getComponent<cro::Callback>().getUserData<MiniCamData>().targetSize =
            interpolate(MiniCamData::MinSize, MiniCamData::MaxSize, smoothstep(MiniCamData::MinSize, MiniCamData::MaxSize, ballDist + 0.4f)); //pad the dist so ball is always in view

        //this is the active ball so update the UI
        cmd.targetFlags = CommandID::UI::PinDistance;
        cmd.action = [&, ballDist](cro::Entity e, float)
        {
            formatDistanceString(ballDist, e.getComponent<cro::Text>(), m_sharedData.imperialMeasurements);

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cmd.targetFlags = CommandID::UI::MiniBall;
        cmd.action =
            [&, update](cro::Entity e, float)
        {
            auto pid = m_currentPlayer.client * 4 + m_currentPlayer.player;
            if (e.getComponent<MiniBall>().playerID == pid)
            {
                e.getComponent<cro::Transform>().setPosition(glm::vec3(toMinimapCoords(update.position), 0.1f));

                //set scale based on height
                static constexpr float MaxHeight = 40.f;
                float scale = 1.f + ((update.position.y / MaxHeight) * 2.f);
                e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
            }
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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

void GolfState::createTransition(const ActivePlayer& playerData)
{
    float targetDistance = glm::length2(playerData.position - m_currentPlayer.position);

    //set the target zoom on the player camera
    float zoom = 1.f;
    if (playerData.terrain == TerrainID::Green)
    {
        const float dist = 1.f - std::min(1.f, glm::length(playerData.position - m_holeData[m_currentHole].pin));
        zoom = m_holeData[m_currentHole].puttFromTee ? (PuttingZoom * (1.f - (0.56f * dist))) : GolfZoom;

        //reduce the zoom within the final metre
        float diff = 1.f - zoom;
        zoom += diff * dist;
    }

    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = zoom;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;

    //hide player avatar
    if (m_activeAvatar)
    {
        //check distance and animate 
        if (/*targetDistance > 0.01
            ||*/ playerData.terrain == TerrainID::Green)
        {
            auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
            scale.y = 0.f;
            scale.z = 0.f;
            m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
            m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

            if (m_activeAvatar->hands)
            {
                //we have to free this up alse the model might
                //become attached to two avatars...
                m_activeAvatar->hands->setModel({});
            }
        }
        else
        {
            m_activeAvatar->model.getComponent<cro::Callback>().getUserData<PlayerCallbackData>().direction = 1;
            m_activeAvatar->model.getComponent<cro::Callback>().active = true;
        }
    }

    
    //hide hud
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float) 
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerName;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(" ");
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::PlayerIcon;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();
    if (playerData.terrain == TerrainID::Green)
    {
        targetInfo.targetHeight = CameraPuttHeight;
        //if (!m_holeData[m_currentHole].puttFromTee)
        {
            targetInfo.targetHeight *= CameraTeeMultiplier;
        }
        targetInfo.targetOffset = CameraPuttOffset;
    }
    else
    {
        targetInfo.targetHeight = CameraStrokeHeight;
        targetInfo.targetOffset = CameraStrokeOffset;
    }

    auto targetDir = m_holeData[m_currentHole].target - playerData.position;
    auto pinDir = m_holeData[m_currentHole].pin - playerData.position;
    targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
    
    //if both the pin and the target are in front of the player
    if (glm::dot(glm::normalize(targetDir), glm::normalize(pinDir)) > 0.4)
    {
        //set the target depending on how close it is
        auto pinDist = glm::length2(pinDir);
        auto targetDist = glm::length2(targetDir);
        if (pinDist < targetDist)
        {
            //always target pin if its closer
            targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
        }
        else
        {
            //target the pin if the target is too close
            //TODO this is really to do with whether or not we're putting
            //when this check happens, but it's unlikely to have
            //a target on the green in other cases.
            const float MinDist = m_holeData[m_currentHole].puttFromTee ? 9.f : 2500.f;
            if (targetDist < MinDist) //remember this in len2
            {
                targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
            }
            else
            {
                targetInfo.targetLookAt = m_holeData[m_currentHole].target;
            }
        }
    }
    else
    {
        //else set the pin as the target
        targetInfo.targetLookAt = m_holeData[m_currentHole].pin;
    }


    //creates an entity which calls setCamPosition() in an
    //interpolated manner until we reach the dest,
    //at which point we update the active player and
    //the ent destroys itself
    auto startPos = m_currentPlayer.position;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(startPos);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<ActivePlayer>(playerData);
    entity.getComponent<cro::Callback>().function =
        [&, startPos](cro::Entity e, float dt)
    {
        const auto& playerData = e.getComponent<cro::Callback>().getUserData<ActivePlayer>();

        auto currPos = e.getComponent<cro::Transform>().getPosition();
        auto travel = playerData.position - currPos;
        auto& targetInfo = m_cameras[CameraID::Player].getComponent<TargetInfo>();

        auto targetDir = targetInfo.currentLookAt - currPos;
        m_camRotation = std::atan2(-targetDir.z, targetDir.x);

        float minTravel = playerData.terrain == TerrainID::Green ? 0.000001f : 0.005f;
        if (glm::length2(travel) < minTravel)
        {
            //we're there
            targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
            targetInfo.startHeight = targetInfo.targetHeight;
            targetInfo.startOffset = targetInfo.targetOffset;

            //hmm the final result is not always the same as the flyby - so snapping this here
            //can cause a jump in view

            //setCameraPosition(playerData.position, targetInfo.targetHeight, targetInfo.targetOffset);
            requestNextPlayer(playerData);

            m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(glm::vec3(0.f));

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
        else
        {
            const auto totalDist = glm::length(playerData.position - startPos);
            const auto currentDist = glm::length(travel);
            const auto percent = 1.f - (currentDist / totalDist);

            targetInfo.currentLookAt = targetInfo.prevLookAt + ((targetInfo.targetLookAt - targetInfo.prevLookAt) * percent);

            auto height = targetInfo.targetHeight - targetInfo.startHeight;
            auto offset = targetInfo.targetOffset - targetInfo.startOffset;

            static constexpr float Speed = 4.f;
            e.getComponent<cro::Transform>().move(travel * Speed * dt);
            setCameraPosition(e.getComponent<cro::Transform>().getPosition(), 
                targetInfo.startHeight + (height * percent), 
                targetInfo.startOffset + (offset * percent));

            m_gameScene.getActiveListener().getComponent<cro::AudioListener>().setVelocity(travel * Speed);
        }
    };
}

void GolfState::startFlyBy()
{
    m_idleTimer.restart();
    idleTime = cro::seconds(90.f);

    //reset the zoom if not putting from tee
    m_cameras[CameraID::Player].getComponent<CameraFollower::ZoomData>().target = m_holeData[m_currentHole].puttFromTee ? PuttingZoom : 1.f;
    m_cameras[CameraID::Player].getComponent<cro::Callback>().active = true;
    m_cameras[CameraID::Player].getComponent<cro::Camera>().setMaxShadowDistance(shadowQuality.shadowFarDistance);


    //static for lambda capture
    static constexpr float MoveSpeed = 50.f; //metres per sec
    static constexpr float MaxHoleDistance = 275.f; //this scales the move speed based on the tee-pin distance
    float SpeedMultiplier = (0.25f + ((m_holeData[m_currentHole].distanceToPin / MaxHoleDistance) * 0.75f));
    float heightMultiplier = 1.f;

    //only slow down if current and previous were putters - in cases of custom courses
    bool previousPutt = (m_currentHole > 0) ? m_holeData[m_currentHole - 1].puttFromTee : m_holeData[m_currentHole].puttFromTee;
    if (m_holeData[m_currentHole].puttFromTee)
    {
        if (previousPutt)
        {
            SpeedMultiplier /= 3.f;
        }
        heightMultiplier = 0.35f;
    }

    struct FlyByTarget final
    {
        std::function<float(float)> ease = std::bind(&cro::Util::Easing::easeInOutQuad, std::placeholders::_1);
        std::int32_t currentTarget = 0;
        float progress = 0.f;
        std::array<glm::mat4, 4u> targets = {};
        std::array<float, 4u> speeds = {};
    }targetData;

    targetData.targets[0] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();


    static constexpr glm::vec3 BaseOffset(10.f, 5.f, 0.f);
    const auto& holeData = m_holeData[m_currentHole];

    //calc offset based on direction of initial target to tee
    glm::vec3 dir = holeData.tee - holeData.pin;
    float rotation = std::atan2(-dir.z, dir.x);
    glm::quat q = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rotation, cro::Transform::Y_AXIS);
    glm::vec3 offset = q * BaseOffset;
    offset.y *= heightMultiplier;

    //set initial transform to look at pin from offset position
    auto transform = glm::inverse(glm::lookAt(offset + holeData.pin, holeData.pin, cro::Transform::Y_AXIS));
    targetData.targets[1] = transform;

    float moveDist = glm::length(glm::vec3(targetData.targets[0][3]) - glm::vec3(targetData.targets[1][3]));
    targetData.speeds[0] = moveDist / MoveSpeed;
    targetData.speeds[0] *= 1.f / SpeedMultiplier;

    //translate the transform to look at target point or half way point if not set
    constexpr float MinTargetMoveDistance = 100.f;
    auto diff = holeData.target - holeData.pin;
    if (glm::length2(diff) < MinTargetMoveDistance)
    {
        diff = (holeData.tee - holeData.pin) / 2.f;
    }
    diff.y = 10.f * SpeedMultiplier;


    transform[3] += glm::vec4(diff, 0.f);
    targetData.targets[2] = transform;

    moveDist = glm::length(glm::vec3(targetData.targets[1][3]) - glm::vec3(targetData.targets[2][3]));
    auto moveSpeed = MoveSpeed;
    if (!m_holeData[m_currentHole].puttFromTee)
    {
        moveSpeed *= std::min(1.f, (moveDist / (MinTargetMoveDistance / 2.f)));
    }
    targetData.speeds[1] = moveDist / moveSpeed;
    targetData.speeds[1] *= 1.f / SpeedMultiplier;

    //the final transform is set to what should be the same as the initial player view
    //this is actually more complicated than it seems, so the callback interrogates the
    //player camera when it needs to.

    //set to initial position
    m_cameras[CameraID::Transition].getComponent<cro::Transform>().setLocalTransform(targetData.targets[0]);


    //interp the transform targets
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<FlyByTarget>(targetData);
    entity.getComponent<cro::Callback>().function =
        [&, SpeedMultiplier](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<FlyByTarget>();
        data.progress = /*std::min*/(data.progress + (dt / data.speeds[data.currentTarget])/*, 1.f*/);

        auto& camTx = m_cameras[CameraID::Transition].getComponent<cro::Transform>();

        //find out 'lookat' point as it would appear on the water plane and set the water there
        glm::vec3 intersection(0.f);
        if (planeIntersect(camTx.getLocalTransform(), intersection))
        {
            intersection.y = WaterLevel;
            m_cameras[CameraID::Transition].getComponent<TargetInfo>().waterPlane.getComponent<cro::Transform>().setPosition(intersection);
        }

        if (data.progress >= 1)
        {
            data.progress -= 1.f;
            data.currentTarget++;
            //camTx.setLocalTransform(data.targets[data.currentTarget]);

            switch (data.currentTarget)
            {
            default: break;
            case 2:
                //hope the player cam finished...
                //which it hasn't on smaller holes, and annoying.
                //not game-breaking. but annoying.
            {
                data.targets[3] = m_cameras[CameraID::Player].getComponent<cro::Transform>().getLocalTransform();
                //data.ease = std::bind(&cro::Util::Easing::easeInSine, std::placeholders::_1);
                float moveDist = glm::length(glm::vec3(data.targets[2][3]) - glm::vec3(data.targets[3][3]));
                data.speeds[2] = moveDist / MoveSpeed;
                data.speeds[2] *= 1.f / SpeedMultiplier;

                //play the transition music
                if (m_sharedData.tutorial)
                {
                    m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();
                }
                //else we'll play it when the score board shows, below
            }
                break;
            case 3:
                //we're done here
                camTx.setLocalTransform(data.targets[data.currentTarget]);

                m_gameScene.getSystem<CameraFollowSystem>()->resetCamera();
                setActiveCamera(CameraID::Player);
            {
                if (m_sharedData.tutorial)
                {
                    auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                    msg->type = SceneEvent::TransitionComplete;
                }
                else
                {
                    showScoreboard(true);
                    m_newHole = true;
                    m_cameras[CameraID::Player].getComponent<cro::AudioEmitter>().play();

                    //delayed ent just to show the score board for a while
                    auto de = m_gameScene.createEntity();
                    de.addComponent<cro::Callback>().active = true;
                    de.getComponent<cro::Callback>().setUserData<float>(0.2f);
                    de.getComponent<cro::Callback>().function =
                        [&](cro::Entity ent, float dt)
                    {
                        auto& currTime = ent.getComponent<cro::Callback>().getUserData<float>();
                        currTime -= dt;
                        if (currTime < 0)
                        {
                            auto* msg = cro::App::getInstance().getMessageBus().post<SceneEvent>(MessageID::SceneMessage);
                            msg->type = SceneEvent::TransitionComplete;

                            ent.getComponent<cro::Callback>().active = false;
                            m_gameScene.destroyEntity(ent);
                        }
                    };
                }
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
                break;
            }
        }

        if (data.currentTarget < 3)
        {
            auto rot = glm::slerp(glm::quat_cast(data.targets[data.currentTarget]), glm::quat_cast(data.targets[data.currentTarget + 1]), data.progress);
            camTx.setRotation(rot);

            auto pos = interpolate(glm::vec3(data.targets[data.currentTarget][3]), glm::vec3(data.targets[data.currentTarget + 1][3]), data.ease(data.progress));
            camTx.setPosition(pos);
        }
    };


    setActiveCamera(CameraID::Transition);


    //hide the minimap ball
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
        e.getComponent<cro::Transform>().setPosition(glm::vec2(-1000.f));
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide hud
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().getUserData<std::pair<std::int32_t, float>>().first = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //show wait message
    cmd.targetFlags = CommandID::UI::WaitMessage;
    cmd.action =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide player
    if (m_activeAvatar)
    {
        auto scale = m_activeAvatar->model.getComponent<cro::Transform>().getScale();
        scale.y = 0.f;
        scale.z = 0.f;
        m_activeAvatar->model.getComponent<cro::Transform>().setScale(scale);
        m_activeAvatar->model.getComponent<cro::Model>().setHidden(true);

        if (m_activeAvatar->hands)
        {
            //we have to free this up alse the model might
            //become attached to two avatars...
            m_activeAvatar->hands->setModel({});
        }
    }

    //hide stroke indicator
    cmd.targetFlags = CommandID::StrokeIndicator | CommandID::StrokeArc;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Model>().setHidden(true);
        e.getComponent<cro::Callback>().active = false;
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //hide ball models so they aren't seen floating mid-air
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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

void GolfState::setActiveCamera(std::int32_t camID)
{
    if (m_photoMode)
    {
        return;
    }

    CRO_ASSERT(camID >= 0 && camID < CameraID::Count, "");

    if (m_cameras[camID].isValid()
        && camID != m_currentCamera)
    {
        if (camID != CameraID::Player
            && (camID < m_currentCamera))
        {
            //don't switch back to the previous camera
            //ie if we're on the green cam don't switch
            //back to sky
            return;
        }

        //reset existing zoom
        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
        }

        //set the water plane ent on the active camera
        if (m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane.isValid())
        {
            m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = {};
        }
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = false;

        if (camID == CameraID::Player)
        {
            auto target = m_currentPlayer.position;
            m_waterEnt.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
        }

        //set scene camera
        m_gameScene.setActiveCamera(m_cameras[camID]);
        if (camID != CameraID::Sky)
        {
            m_gameScene.setActiveListener(m_cameras[camID]);
        }
        m_currentCamera = camID;

        m_cameras[m_currentCamera].getComponent<TargetInfo>().waterPlane = m_waterEnt;
        m_cameras[m_currentCamera].getComponent<cro::Camera>().active = true;
        
        if (m_cameras[m_currentCamera].hasComponent<CameraFollower>())
        {
            m_cameras[m_currentCamera].getComponent<CameraFollower>().reset(m_cameras[m_currentCamera]);
            m_cameras[m_currentCamera].getComponent<CameraFollower>().currentFollowTime = CameraFollower::MinFollowTime;
        }
        m_courseEnt.getComponent<cro::Drawable2D>().setShader(m_cameras[m_currentCamera].getComponent<TargetInfo>().postProcess);
    }
}

void GolfState::updateCameraHeight(float movement)
{
    if (m_currentCamera == CameraID::Player
        && m_currentPlayer.terrain == TerrainID::Green)
    {
        auto camPos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getPosition();

        static constexpr float DistanceIncrease = 5.f;
        float distanceToHole = glm::length(m_holeData[m_currentHole].pin - camPos);
        float heightMultiplier = std::clamp(distanceToHole - DistanceIncrease, 0.f, DistanceIncrease);

        const auto MaxOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().targetHeight;
        const auto TargetHeight = MaxOffset + m_collisionMesh.getTerrain(camPos).height;

        camPos.y = std::clamp(camPos.y + movement, TargetHeight - (MaxOffset * 0.5f), TargetHeight + (MaxOffset * 0.6f) + (heightMultiplier / DistanceIncrease));


        //correct for any target offset that may have been added in transition
        auto& lookAtOffset = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAtOffset;
        auto movement = lookAtOffset * (1.f / 30.f);
        lookAtOffset += movement;

        auto& lookAt = m_cameras[CameraID::Player].getComponent<TargetInfo>().finalLookAt;
        lookAt -= movement;

        auto tx = glm::lookAt(camPos, lookAt, cro::Transform::Y_AXIS);
        m_cameras[CameraID::Player].getComponent<cro::Transform>().setLocalTransform(glm::inverse(tx));
    }
}

void GolfState::toggleFreeCam()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeArc | CommandID::StrokeIndicator;

    m_photoMode = !m_photoMode;
    if (m_photoMode)
    {
        m_defaultCam = m_gameScene.setActiveCamera(m_freeCam);
        m_defaultCam.getComponent<cro::Camera>().active = false;
        m_gameScene.setActiveListener(m_freeCam);

        auto tx = glm::lookAt(m_currentPlayer.position + glm::vec3(0.f, 3.f, 0.f), m_holeData[m_currentHole].pin, glm::vec3(0.f, 1.f, 0.f));
        m_freeCam.getComponent<cro::Transform>().setLocalTransform(glm::inverse(tx));
        m_freeCam.getComponent<FpsCamera>().resetOrientation(m_freeCam);
        m_freeCam.getComponent<cro::Camera>().active = true;

        //hide stroke indicator
        cmd.action = [](cro::Entity e, float) {e.getComponent<cro::Model>().setHidden(true); };

        //reduce fade distance
        m_resolutionUpdate.targetFade = 0.2f;

        //hide UI by bringing scene ent to front
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = -3.f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);
    }
    else
    {
        m_gameScene.setActiveCamera(m_defaultCam);
        m_gameScene.setActiveListener(m_defaultCam);

        m_defaultCam.getComponent<cro::Camera>().active = true;
        m_freeCam.getComponent<cro::Camera>().active = false;

        //restore fade distance
        m_resolutionUpdate.targetFade = m_currentPlayer.terrain == TerrainID::Green ? GreenFadeDistance : CourseFadeDistance;

        //unhide UI
        auto origin = m_courseEnt.getComponent<cro::Transform>().getOrigin();
        origin.z = 0.5f;
        m_courseEnt.getComponent<cro::Transform>().setOrigin(origin);


        //and stroke indicator
        cmd.action = [&](cro::Entity e, float)
        {
            auto localPlayer = m_currentPlayer.client == m_sharedData.clientConnection.connectionID;
            e.getComponent<cro::Model>().setHidden(!(localPlayer && !m_sharedData.localConnectionData.playerData[m_currentPlayer.player].isCPU));
        };
    }
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    m_gameScene.setSystemActive<FpsCameraSystem>(m_photoMode);
    m_waterEnt.getComponent<cro::Callback>().active = !m_photoMode;
    m_inputParser.setActive(!m_photoMode && m_restoreInput);
    cro::App::getWindow().setMouseCaptured(m_photoMode);
}

void GolfState::applyShadowQuality()
{
    shadowQuality.update(m_sharedData.hqShadows);

    m_gameScene.getSystem<cro::ShadowMapRenderer>()->setRenderInterval(m_sharedData.hqShadows ? 2 : 3);

    auto applyShadowSettings = 
        [&](cro::Camera& cam, std::int32_t camID)
    {
        std::uint32_t shadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;
        if (camID == CameraID::Sky)
        {
            shadowMapSize /= 2;
        }

        float camDistance = shadowQuality.shadowFarDistance;
        if ((camID == CameraID::Player
            && m_currentPlayer.terrain == TerrainID::Green)
            || camID == CameraID::Bystander)
        {
            camDistance = shadowQuality.shadowNearDistance;
        }

        cam.shadowMapBuffer.create(shadowMapSize, shadowMapSize);
        cam.setMaxShadowDistance(camDistance);

        cam.setPerspective(cam.getFOV(), cam.getAspectRatio(), cam.getNearPlane(), cam.getFarPlane(), shadowQuality.cascadeCount);
    };

    for (auto i = 0; i < static_cast<std::int32_t>(m_cameras.size()); ++i)
    {
        applyShadowSettings(m_cameras[i].getComponent<cro::Camera>(), i);
    }

#ifdef CRO_DEBUG_
    applyShadowSettings(m_freeCam.getComponent<cro::Camera>(), -1);
#endif // CRO_DEBUG_

}

void GolfState::gamepadNotify(std::int32_t type)
{
    if (m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
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