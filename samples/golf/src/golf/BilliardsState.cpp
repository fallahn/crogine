/*-----------------------------------------------------------------------

Matt Marchant - 2022 - 2023
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

#include "BilliardsState.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "CommandIDs.hpp"
#include "MessageIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "BilliardsSystem.hpp"
#include "BilliardsClientCollision.hpp"
#include "InterpolationSystem.hpp"
#include "NotificationSystem.hpp"
#include "PocketBallSystem.hpp"
#include "BilliardsSoundDirector.hpp"
#include "MessageIDs.hpp"
#include "server/ServerPacketData.hpp"
#include "server/ServerMessages.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/AudioListener.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/Network.hpp>
#include <crogine/util/Random.hpp>

#include <Achievements.hpp>

using namespace cl;

namespace
{
#include "shaders/WireframeShader.inl"
#include "shaders/CelShader.inl"
#include "shaders/ShaderIncludes.inl"

    constexpr float MaxShadowDistance = 12.f;
    constexpr float ShadowExpansion = 10.f;

    const std::array FoulStrings =
    {
        std::string("Foul! Wrong Ball Hit"),
        std::string("Foul! Wrong Ball Potted"),
        std::string("Foul! Off the Table!"),
        std::string("Game Forfeit"),
        std::string("Foul! No Ball Hit"),
        std::string("Cue Ball Potted"),
        std::string("Free Table")
    };

    const std::array CameraStrings =
    {
        std::string("Spectate"),
        std::string("Overhead"),
        std::string("Player"),
        std::string("Transition"),
    };

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    struct CameraProperties final
    {
        float FOVAdjust = 1.f;
        float farPlane = 5.f;
    };

    struct CueCallbackData final
    {
        float currentScale = 0.f;
        enum
        {
            In, Out
        }direction = In;
    };
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_gameScene         (ctx.appInstance.getMessageBus(), 512),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_inputParser       (sd, ctx.appInstance.getMessageBus()),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_viewScale         (2.f),
    m_ballDefinition    (m_resources),
    m_fleaDefinition    (m_resources),
    m_wantsGameState    (true),
    m_wantsNotify       (false),
    m_gameEnded         (false),
    m_activeCamera      (CameraID::Spectate),
    m_gameMode          (TableData::Void),
    m_readyQuitFlags    (0)
{
    ctx.mainWindow.loadResources([&]()
        {
            loadAssets();
            addSystems();
            buildScene();
        });

    ctx.mainWindow.setMouseCaptured(true);

    //this is already set to Clubhouse so the pause
    //menu knows where to go when quitting.
    //sd.baseState = StateID::Billiards;

    Achievements::setActive(sd.localConnectionData.playerCount == 1);

#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Buns"))
    //        {

    //            //ImGui::Text("Power %3.3f", m_inputParser.getPower());

    //            /*auto pos = m_cameras[CameraID::Player].getComponent<cro::Transform>().getPosition();
    //            if (ImGui::SliderFloat("Height", &pos.y, 0.1f, 1.f))
    //            {
    //                m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(pos);
    //            }
    //            
    //            if (ImGui::SliderFloat("Distance", &pos.z, 0.1f, 1.f))
    //            {
    //                m_cameras[CameraID::Player].getComponent<cro::Transform>().setPosition(pos);
    //            }

    //            ImGui::Text("Position %3.3f, %3.3f", pos.y, pos.z);

    //            static float rotation = -10.f;
    //            if (ImGui::SliderFloat("Rotation", &rotation, -90.f, 0.f))
    //            {
    //                m_cameras[CameraID::Player].getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation * cro::Util::Const::degToRad);
    //            }*/
    //        }
    //        ImGui::End();
    //    });

    /*registerWindow([&]()
        {
            if (ImGui::Begin("Window"))
            {
                ImGui::Text("Camera: %s", CameraStrings[m_activeCamera].c_str());

                auto& cam = m_cameras[m_activeCamera];
                auto dist = cam.getComponent<cro::Camera>().getMaxShadowDistance();
                if (ImGui::SliderFloat("Dist", &dist, 0.5f, 10.f))
                {
                    cam.getComponent<cro::Camera>().setMaxShadowDistance(dist);
                }

                auto depth = cam.getComponent<cro::Camera>().getShadowExpansion();
                if (ImGui::SliderFloat("depth", &depth, 0.f, 20.f))
                {
                    cam.getComponent<cro::Camera>().setShadowExpansion(depth);
                }
            }
            ImGui::End();
        });*/
#endif

    m_inputParser.setActive(false, false); //activates spectator cam input on start up
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        if (evt.type == SDL_MOUSEMOTION
            && cro::App::getWindow().getMouseCaptured())
        {
            cro::App::getWindow().setMouseCaptured(false);
        }

        return false;
    }

#ifdef CRO_DEBUG_
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint16_t(ServerCommand::SpawnBall), net::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint16_t(ServerCommand::StrikeBall), net::NetFlag::Reliable);
            break;
        case SDLK_F4:
            //addPocketBall(1);
            break;
            //F8 toggles chat!
        case SDLK_HOME:
            m_gameScene.getSystem<BilliardsCollisionSystem>()->toggleDebug();
            break;
        }
    }
    else 
#endif
    //TODO we need a good way to release the mouse without interfering with game play...
    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_p:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
        case SDLK_SPACE:
            toggleQuitReady();
            break;
#ifdef CRO_DEBUG_
        case SDLK_KP_1:
        case SDLK_1:
            toggleOverhead();
            break;
        case SDLK_KP_7:
            setActiveCamera(CameraID::Player);
            break;
        case SDLK_KP_8:
            setActiveCamera(CameraID::Spectate);
            break;
        case SDLK_KP_9:
            setActiveCamera(CameraID::Overhead);
            break;
        case SDLK_F6:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint16_t(ServerCommand::EndGame), net::NetFlag::Reliable);
            break;
        case SDLK_F7:
        {
            /*static std::int8_t num = 0;
            addPocketBall(num);
            num = (num + 1) % 15;*/

            spawnFlea();
        }
            break;
        case SDLK_F8:
            //m_gameScene.getSystem<PocketBallSystem>()->removeBall(8);
            spawnFace();
            break;
        case SDLK_F9:
            spawnSnail();
            break;
#else
        case SDLK_KP_1:
        case SDLK_1:
            toggleOverhead();
            break;
#endif
        
        case SDLK_KP_5:
        case SDLK_5:
            //setActiveCamera(CameraID::Follower);
            break;
#ifdef CRO_DEBUG_
        case SDLK_TAB:
            //if (m_activeCamera == CameraID::Player)
            {
                cro::App::getWindow().setMouseCaptured(false);
            }
            break;
#endif //CRO_DEBUG_
        }

        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            sendReadyNotify();
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(activeControllerID(m_sharedData.inputBinding.playerID)))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
                toggleQuitReady();
                sendReadyNotify();
                break;
            case cro::GameController::ButtonY:
                toggleOverhead();
                break;
            }
        }
//#ifdef CRO_DEBUG_
//        else
//        {
//            LogI << "Event button ID " << evt.cbutton.which << ", controller ID " << cro::GameController::deviceID(activeControllerID(m_sharedData.inputBinding.playerID)) << std::endl;
//        }
//#endif
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(activeControllerID(m_sharedData.inputBinding.playerID)))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonStart:
                requestStackPush(StateID::Pause);
                break;
            }
        }
//#ifdef CRO_DEBUG_
//        else
//        {
//            LogI << "Event button ID " << evt.cbutton.which << ", controller ID " << cro::GameController::deviceID(activeControllerID(m_sharedData.inputBinding.playerID)) << std::endl;
//        }
//#endif
    }

    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            sendReadyNotify();
        }
    }

    m_inputParser.handleEvent(evt);
    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case cro::Message::WindowMessage:
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            resizeBuffers();
        }
    }
        break;
    case cro::Message::SkeletalAnimationMessage:
    {
        const auto& data = msg.getData<cro::Message::SkeletalAnimationEvent>();
        if (data.userType == 0
            && data.entity == m_localCue)
        {
            BilliardBallInput input;
            auto [impulse, offset] = m_inputParser.getImpulse();
            input.impulse = impulse;
            input.offset = offset;
            input.client = m_sharedData.localConnectionData.connectionID;
            input.player = m_currentPlayer.player;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, input, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            cro::GameController::rumbleStart(activeControllerID(m_currentPlayer.player), 60000 * m_sharedData.enableRumble, 0, 300);

            if (m_activeCamera != CameraID::Overhead)
            {
                setActiveCamera(CameraID::Spectate);
            }
        }
        else if (data.userType == cro::Message::SkeletalAnimationEvent::Stopped)
        {
            //hide the cues
            m_localCue.getComponent<cro::Callback>().getUserData<CueCallbackData>().direction = CueCallbackData::Out;
            m_localCue.getComponent<cro::Callback>().active = true;

            m_remoteCue.getComponent<cro::Callback>().getUserData<CueCallbackData>().direction = CueCallbackData::Out;
            m_remoteCue.getComponent<cro::Callback>().active = true;

            //setActiveCamera(CameraID::Spectate);
        }
    }
    break;
    case MessageID::BilliardsMessage:
    {
        const auto& data = msg.getData<BilliardBallEvent>();
        if (data.type == BilliardBallEvent::BallPlaced)
        {
            BilliardBallInput input;
            input.offset = data.position;
            input.client = m_sharedData.localConnectionData.connectionID;
            input.player = m_currentPlayer.player;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::BallPlaced, input, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else if (data.type == BilliardBallEvent::ShotTaken)
        {
            /*BilliardBallInput input;
            auto [impulse, offset] = m_inputParser.getImpulse();
            input.impulse = impulse;
            input.offset = offset;
            input.client = m_sharedData.localConnectionData.connectionID;
            input.player = m_currentPlayer.player;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, input, net::NetFlag::Reliable, ConstVal::NetChannelReliable);*/

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ActorAnimation, std::uint8_t(1), net::NetFlag::Reliable, ConstVal::NetChannelReliable);

            //hide free table sign if active
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::WindSock;
            cmd.action = [](cro::Entity e, float)
            {
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
    }
        break;
    case MessageID::BilliardsCameraMessage:
    {
        const auto& data = msg.getData<BilliardsCameraEvent>();
        if (data.type == BilliardsCameraEvent::NewTarget)
        {
            //m_cameras[CameraID::Follower].getComponent<StudioCamera>().setTarget(data.target);
        }
    }
        break;
    case MessageID::SystemMessage:
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::ShadowQualityChanged)
        {
            auto shadowRes = m_sharedData.hqShadows ? 4096 : 2048;
            for (auto& cam : m_cameras)
            {
                cam.getComponent<cro::Camera>().shadowMapBuffer.create(shadowRes, shadowRes, 2);
            }
        }
    }
    break;
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        for (const auto& evt : m_sharedData.clientConnection.eventBuffer)
        {
            handleNetEvent(evt);
        }
        m_sharedData.clientConnection.eventBuffer.clear();

        net::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            //handle events
            handleNetEvent(evt);
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

    static float timeAccum = 0.f;
    timeAccum += dt;

    glCheck(glUseProgram(m_indicatorProperties.shaderID));
    glCheck(glUniform1f(m_indicatorProperties.timeUniform, timeAccum * (10.f + (40.f * m_inputParser.getPower()))));

    m_inputParser.update(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    if (m_inputParser.getActive()
        && getStateCount() == 1)
    {
        if (!cro::App::getWindow().getMouseCaptured())
        {
#ifdef CRO_DEBUG_
            if (!cro::Keyboard::isKeyPressed(SDLK_TAB))
#endif
                cro::App::getWindow().setMouseCaptured(true);
        }

        //send our cue model data (less frequently) so remote clients
        //can see an approximate ghost version
        static int32_t netCounter = 0;
        netCounter = (netCounter + 1) % 3;

        if (netCounter == 0
            && m_currentPlayer.client == m_sharedData.clientConnection.connectionID)
        {
            auto position = m_localCue.getComponent<cro::Transform>().getWorldPosition();
            auto rotation = glm::quat_cast(m_localCue.getComponent<cro::Transform>().getWorldTransform());
            auto timestamp = m_readyClock.elapsed().asMilliseconds(); //re-use this clock: timestamps are just used for ordering.

            BilliardsUpdate info;
            info.position = cro::Util::Net::compressVec3(position, 4);
            info.rotation = cro::Util::Net::compressQuat(rotation);
            info.timestamp = timestamp;

            m_sharedData.clientConnection.netClient.sendPacket(PacketID::CueUpdate, info, net::NetFlag::Unreliable);
        }
    }
    /*else
    {
        if (cro::App::getWindow().getMouseCaptured())
        {
            cro::App::getWindow().setMouseCaptured(false);
        }
    }*/

    return false;
}

void BilliardsState::render()
{
    m_gameSceneTexture.clear(/*cro::Colour::AliceBlue*/);
    m_gameScene.render();
    
#ifdef CRO_DEBUG_
    m_gameScene.getSystem<BilliardsCollisionSystem>()->renderDebug(
        m_gameScene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix, m_gameSceneTexture.getSize());
#endif
    
    m_gameSceneTexture.display();

    auto oldCam = m_gameScene.setActiveCamera(m_topspinCamera);
    m_topspinTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_topspinTexture.display();


    m_gameScene.setActiveCamera(m_pocketedCamera);
    m_pocketedTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_pocketedTexture.display();


    if (m_gameEnded)
    {
        m_gameScene.setActiveCamera(m_trophyCamera);
        m_trophyTexture.clear(cro::Colour::Transparent);
        m_gameScene.render();
        m_trophyTexture.display();
    }
    m_gameScene.setActiveCamera(oldCam);

    m_uiScene.render();

#ifdef CRO_DEBUG_
    /*oldCam = m_gameScene.getActiveCamera();
    m_gameScene.setActiveCamera(m_cameras[CameraID::Spectate]);
    m_debugBuffer.clear();
    m_gameScene.render();
    m_debugBuffer.display();
    m_gameScene.setActiveCamera(oldCam);*/
#endif // CRO_DEBUG_

}

//private
void BilliardsState::loadAssets()
{
    m_audioScape.loadFromFile("assets/golf/sound/billiards/menu.xas", m_resources.audio);
    m_smokePuff.loadFromFile("assets/golf/particles/puff.cps", m_resources.textures);

    m_ballDefinition.loadFromFile("assets/golf/models/hole_19/billiard_ball.cmt");
    m_fleaDefinition.loadFromFile("assets/golf/models/flea.cmt");

    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    m_resources.shaders.loadFromString(ShaderID::Wireframe, WireframeVertex, WireframeFragment, "#define DASHED\n");
    auto* shader = &m_resources.shaders.get(ShaderID::Wireframe);
    m_indicatorProperties.shaderID = shader->getGLHandle();
    m_indicatorProperties.timeUniform = shader->getUniformID("u_time");
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(*shader);
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Table] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define REFLECTIONS\n#define TEXTURED\n#define SUBRECT\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ball] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define REFLECTIONS\n");
    shader = &m_resources.shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Trophy] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n");
    shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::TrophyBase] = m_resources.materials.add(*shader);

    if (m_trophyReflectionMap.loadFromFile("assets/golf/images/skybox/billiards/trophy.ccm"))
    {
        m_resources.materials.get(m_materialIDs[MaterialID::Trophy]).setProperty("u_reflectMap", cro::CubemapID(m_trophyReflectionMap.getGLHandle()));
    }

    if (m_reflectionMap.loadFromFile("assets/golf/images/skybox/billiards/sky.ccm"))
    {
        m_resources.materials.get(m_materialIDs[MaterialID::Ball]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));
    }

    m_resources.shaders.loadFromString(ShaderID::CelSkinned, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define SKINNED\n" + wobble);
    shader = &m_resources.shaders.get(ShaderID::CelSkinned);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cue] = m_resources.materials.add(*shader);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/eyes.spt", m_resources.textures);
    m_sprites[SpriteID::Face] = spriteSheet.getSprite("eyes");

    m_faceAnimationIDs[AnimationID::In] = spriteSheet.getAnimationIndex("open", "eyes");
    m_faceAnimationIDs[AnimationID::Cycle] = spriteSheet.getAnimationIndex("blink", "eyes");
    m_faceAnimationIDs[AnimationID::Out] = spriteSheet.getAnimationIndex("close", "eyes");

    spriteSheet.loadFromFile("assets/golf/sprites/snail.spt", m_resources.textures);
    m_sprites[SpriteID::Snail] = spriteSheet.getSprite("snail");

    m_snailAnimationIDs[AnimationID::In] = spriteSheet.getAnimationIndex("dig_up", "snail");
    m_snailAnimationIDs[AnimationID::Cycle] = spriteSheet.getAnimationIndex("walk", "snail");
    m_snailAnimationIDs[AnimationID::Out] = spriteSheet.getAnimationIndex("dig_down", "snail");
}

void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem<InterpolationType::Hermite>>(mb);
    m_gameScene.addSystem<InterpolationSystem<InterpolationType::Linear>>(mb); //updates the ghost cue
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<BilliardsCollisionSystem>(mb);
    m_gameScene.addSystem<PocketBallSystem>(mb);
    //m_gameScene.addSystem<StudioCameraSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem3D>(mb, 16.f / BilliardBall::Radius);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);

    m_gameScene.addDirector<BilliardsSoundDirector>(m_resources.audio);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<NotificationSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void BilliardsState::buildScene()
{
    //table data should have already been validated by the menu state
    //so we assume here that it's safe to load.
    
    float spectateOffset = 0.f; //spectator camera offset is based on the size of the table model
    float overheadOffset = 0.f;

    std::string path = "assets/golf/tables/" + m_sharedData.mapDirectory + ".table";
    TableData tableData;
    if (tableData.loadFromFile(path))
    {
        cro::ModelDefinition md(m_resources);
        if (md.loadFromFile(tableData.viewModel))
        {
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            md.createModel(entity);

            spectateOffset = entity.getComponent<cro::Model>().getMeshData().boundingBox[1].z * 1.5f;
            overheadOffset = spectateOffset * 1.25f;

            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Table]);
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);

            if (tableData.tableSkins.size() > m_sharedData.tableSkinIndex)
            {
                entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", 
                    cro::TextureID(m_resources.textures.get(tableData.tableSkins[m_sharedData.tableSkinIndex])));
            }
        }
        else
        {
            m_sharedData.errorMessage = "Unable to load table model";
            requestStackPush(StateID::Error);
        }
        m_gameMode = tableData.rules;
        
        Social::setStatus(Social::InfoID::Billiards, { TableStrings[tableData.rules].toAnsiString().c_str() });

        if (tableData.ballSkins.size() > m_sharedData.ballSkinIndex)
        {
            m_ballTexture = m_resources.textures.get(tableData.ballSkins[m_sharedData.ballSkinIndex]);
        }

        m_gameScene.getSystem<BilliardsCollisionSystem>()->initTable(tableData);
        m_inputParser.setSpawnArea(tableData.spawnArea);
    }
    else
    {
        //push error for missing data.
        m_sharedData.errorMessage = "Unable to load table data";
        requestStackPush(StateID::Error);
    }

    //update the 3D view
    resizeBuffers();
    

    auto setPerspective = [&](cro::Camera& cam)
    {
        auto vpSize = glm::vec2(cro::App::getWindow().getSize());

        //fudge to see which camera we're working on 
        auto ent = std::find_if(m_cameras.begin(), m_cameras.end(), [&cam](const cro::Entity& c)
            {
                return c.isValid() && c.getComponent<cro::Camera>() == cam;
            });

        float fovMultiplier = 1.f;
        float farPlane = 5.f;

        if (ent != m_cameras.end() && 
            ent->isValid())
        {
            fovMultiplier = ent->getComponent<CameraProperties>().FOVAdjust;
            farPlane = ent->getComponent<CameraProperties>().farPlane;
        }

        cam.setPerspective(m_sharedData.fov * fovMultiplier * cro::Util::Const::degToRad, vpSize.x / vpSize.y, 0.1f, farPlane, 2);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    //spectate cam
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.8f * (spectateOffset / 1.6f), spectateOffset});
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -30.f * cro::Util::Const::degToRad);
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.8f;
    camEnt.getComponent<CameraProperties>().farPlane = 7.f;
    m_cameras[CameraID::Spectate] = camEnt;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = setPerspective;
    cam.setRenderFlags(cro::Camera::Pass::Final, ~RenderFlags::Cue);
    setPerspective(cam);

    const std::uint32_t ShadowMapSize = m_sharedData.hqShadows ? 4096u : 2048u;
    cam.shadowMapBuffer.create(ShadowMapSize, ShadowMapSize, 3);
    cam.setMaxShadowDistance(MaxShadowDistance);
    cam.setShadowExpansion(ShadowExpansion * 2.f);

    auto spectateController = m_gameScene.createEntity();
    spectateController.addComponent<cro::Transform>().setPosition({0.f, 0.2f, 0.f});
    spectateController.addComponent<ControllerRotation>().activeCamera = &cam.active;
    spectateController.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());

    spectateController.addComponent<cro::Callback>().active = true;
    spectateController.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if(m_gameScene.getActiveCamera() == m_cameras[CameraID::Player])
        {
            auto rotation = m_cameraController.getComponent<ControllerRotation>().rotation;
            e.getComponent<ControllerRotation>().rotation = rotation;
        }
    };

    //overhead cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, overheadOffset, 0.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, -90.f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(MaxShadowDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(ShadowExpansion);
    //camEnt.getComponent<cro::Camera>().renderFlags = ~RenderFlags::Cue;
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.75f;
    camEnt.getComponent<CameraProperties>().farPlane = 6.f;
    camEnt.addComponent<cro::AudioListener>();
    m_cameras[CameraID::Overhead] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());


    //transition cam
    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>();
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(MaxShadowDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(ShadowExpansion * 2.f);
    camEnt.getComponent<cro::Camera>().setRenderFlags(cro::Camera::Pass::Final, ~RenderFlags::Cue);
    camEnt.addComponent<CameraProperties>().farPlane = 7.f;
    camEnt.getComponent<CameraProperties>().FOVAdjust = 0.8f; //needs to match spectate cam initial value to prevent popping
    camEnt.addComponent<cro::AudioListener>();
    m_cameras[CameraID::Transition] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());

    //player cam is a bit more complicated because it can be rotated
    //by the player, and transformed by the game.
    m_cameraController = m_gameScene.createEntity();
    m_cameraController.addComponent<cro::Transform>();
    m_cameraController.addComponent<ControllerRotation>();

    auto camTilt = m_gameScene.createEntity();
    camTilt.addComponent<cro::Transform>().setPosition({ 0.f, 0.34f, 0.f });
    camTilt.addComponent<ControllerRotation>();
    m_cameraController.getComponent<cro::Transform>().addChild(camTilt.getComponent<cro::Transform>());

    camEnt = m_gameScene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.45f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -26.7f * cro::Util::Const::degToRad);
    camEnt.addComponent<cro::Camera>().resizeCallback = setPerspective;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(ShadowMapSize, ShadowMapSize);
    camEnt.getComponent<cro::Camera>().active = false;
    camEnt.getComponent<cro::Camera>().setMaxShadowDistance(MaxShadowDistance);
    camEnt.getComponent<cro::Camera>().setShadowExpansion(ShadowExpansion * 2.f);
    camEnt.addComponent<CameraProperties>().FOVAdjust = 0.8f;
    camEnt.getComponent<CameraProperties>().farPlane = 6.f;
    camEnt.addComponent<cro::AudioListener>();
    m_cameras[CameraID::Player] = camEnt;
    setPerspective(camEnt.getComponent<cro::Camera>());

    camTilt.getComponent<cro::Transform>().addChild(camEnt.getComponent<cro::Transform>());

    m_cameraController.getComponent<ControllerRotation>().activeCamera = &camEnt.getComponent<cro::Camera>().active;
    m_cameraController.addComponent<cro::Callback>();// .active = true;
    m_cameraController.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_cameraController.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        if (m_cueball.isValid()
            && !m_inputParser.getActive())
        {
            //turn to face cue ball
            auto dir = /*glm::normalize*/(m_cueball.getComponent<cro::Transform>().getPosition() - e.getComponent<cro::Transform>().getPosition());
            
            float adjustment = glm::dot(dir, e.getComponent<cro::Transform>().getRightVector());

            auto absDiff = std::abs(adjustment);
            if (absDiff > 0.2f)
            {
                adjustment *= (cro::Util::Const::PI / 2.f);
                e.getComponent<ControllerRotation>().rotation -= adjustment * (absDiff - 0.2f) * dt;
            }

            //after some time check if the ball is too close
            //and change the view
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime += dt;

            if (currTime > 3
                && glm::dot(dir, e.getComponent<cro::Transform>().getForwardVector()) < 0.2f
                && m_activeCamera == CameraID::Player)
            {
                setActiveCamera(CameraID::Overhead);
                currTime = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<float>() = 0.f;
        }
    };

    ControlEntities controlEntities;
    controlEntities.camera = m_cameraController;
    controlEntities.cameraTilt = camTilt;
    controlEntities.spectator = spectateController;

    auto cueScaleCallback = [](cro::Entity e, float dt)
    {
        auto& [currScale, direction] = e.getComponent<cro::Callback>().getUserData<CueCallbackData>();
        if (direction == CueCallbackData::In)
        {
            e.getComponent<cro::Model>().setHidden(false);
            currScale = std::min(1.f, currScale + dt);

            if (currScale == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                direction = CueCallbackData::Out;
            }
        }
        else
        {
            currScale = std::max(0.f, currScale - (dt * 4.f));
            if (currScale == 0)
            {
                //mini-optimisation - hiding the model stops the
                //renderer from sending the model to be drawn which
                //otherwise happens even when scaled to 0
                e.getComponent<cro::Model>().setHidden(true);
                e.getComponent<cro::Callback>().active = false;
                direction = CueCallbackData::In;
            }
        }
        float scale = cro::Util::Easing::easeOutBack(currScale);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<ControllerRotation>();
    entity.addComponent<cro::Callback>().setUserData<CueCallbackData>();
    entity.getComponent<cro::Callback>().function = cueScaleCallback;

    //two cue models - one for local player,
    //one to display what the remote player is doing
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/golf/models/hole_19/cue.cmt"))
    {
        md.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        m_cameraController.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cue]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);
        entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Cue);
    }
    m_localCue = entity;
    controlEntities.cue = entity;

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<InterpolationComponent<InterpolationType::Linear>>();
    entity.addComponent<cro::Callback>().setUserData<CueCallbackData>();
    entity.getComponent<cro::Callback>().function = cueScaleCallback;
    if (md.loadFromFile("assets/golf/models/hole_19/remote_cue.cmt"))
    {
        md.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
    }
    m_remoteCue = entity;

    //semi-transparent ball for placing the cueball
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    if (md.loadFromFile("assets/golf/models/hole_19/preview_ball.cmt"))
    {
        md.createModel(entity);
        entity.getComponent<cro::Model>().setHidden(true);
        entity.addComponent<BilliardBall>().id = -1;
        //need this to work with client collision system
        entity.addComponent<InterpolationComponent<InterpolationType::Hermite>>();
    }
    controlEntities.previewBall = entity;
    


    //direction indicator
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour | cro::VertexProperty::UV0, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setHidden(true);
    entity.getComponent<cro::Model>().setRenderFlags(RenderFlags::Cue);

    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->boundingBox = { glm::vec3(3.f), glm::vec3(-3.f) };
    //UV is used for dashed line effect
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,   1.f, 0.97f, 0.f, 1.f,  0.f, 0.f,
        0.f, 0.f, -1.f,  1.f, 0.97f, 0.f, 1.f,  1.f, 0.f,
        0.f, 0.f, -1.f,  1.f, 0.97f, 0.f, 1.f,  1.f, 0.f
    };

    auto vertStride = (meshData->vertexSize / sizeof(float));
    meshData->vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, verts.data(), GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    std::vector<std::uint32_t> indices =
    {
        0,1
    };

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, meshData](cro::Entity e, float)
    {
        static std::vector<float> vertexData;
        static const auto c = TextGoldColour;
        static constexpr float alpha = 0.7f;

        auto position = m_localCue.getComponent<cro::Transform>().getWorldPosition();
        auto direction = m_localCue.getComponent<cro::Transform>().getForwardVector();

        auto rayResult = m_gameScene.getSystem<BilliardsCollisionSystem>()->rayCast(position, direction);

        if (!rayResult.hasHit)
        {
            auto endpoint = position + direction;
            vertexData =
            {
                position.x, position.y, position.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, 0.f, 0.f,
                endpoint.x, endpoint.y, endpoint.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, 1.f, 0.f,
                endpoint.x, endpoint.y, endpoint.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, 1.f, 0.f
            };
        }
        else
        {
            auto midpoint = rayResult.hitPointWorld;
            auto endpoint = (position + direction) - midpoint;
            endpoint = glm::reflect(endpoint, rayResult.normalWorld);
            endpoint += midpoint;
            vertexData =
            {
                position.x, position.y, position.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, 0.f, 0.f,
                midpoint.x, midpoint.y, midpoint.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, glm::length(midpoint - position), 0.f,
                endpoint.x, endpoint.y, endpoint.z,  c.getRed(), c.getGreen(), c.getBlue(), alpha, 1.f, 0.f
            };
        }

        auto vertStride = (meshData->vertexSize / sizeof(float));
        meshData->vertexCount = vertexData.size() / vertStride;
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, meshData->vertexSize * meshData->vertexCount, vertexData.data(), GL_DYNAMIC_DRAW));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        e.getComponent<cro::Callback>().active = !e.getComponent<cro::Model>().isHidden();
    };

    controlEntities.indicator = entity;
    m_inputParser.setControlEntities(controlEntities);


    //lights point straight down in billiards.
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);

    createUI();


    //TODO this wants to be done after any loading animation
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::TransitionComplete,
        m_sharedData.clientConnection.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);



    //entity to cause random events if player not moving
    struct TimerCallbackData final
    {
        float currTime = 0.f;
        const float Timeout = 30.f;
    };
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<TimerCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [currTime, Timeout] = e.getComponent<cro::Callback>().getUserData<TimerCallbackData>();
        currTime += dt;

        if (m_inputParser.hasInput())
        {
            currTime = 0;
        }
        else if (currTime > Timeout)
        {
            currTime = 0;

            if (!m_cueball.isValid())
            {
                for (auto i = 0; i < 6; ++i)
                {
                    spawnFlea();
                }
            }
            else
            {
                switch  (cro::Util::Random::value(0, 2))
                {
                default:
                case 0:
                    spawnFace();
                    break;
                case 1:
                    spawnSnail();
                    break;
                case 2:
                    for (auto i = 0; i < 6; ++i)
                    {
                        spawnFlea();
                    }
                    break;
                }
            }
        }
    };



    //delayed message to announce start
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(3.5f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if (currTime < 0)
        {
            auto* msg = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
            msg->type = BilliardBallEvent::GameStarted;

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };
}

void BilliardsState::handleNetEvent(const net::NetEvent& evt)
{
    if (evt.type == net::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::TargetID:
        {
            auto data = evt.packet.as<std::uint16_t>();
            std::int8_t playerID = (data & 0xff00) >> 8;
            std::int8_t ballID = (data & 0x00ff);
            if (playerID < 2)
            {
                updateTargetTexture(playerID, ballID);
            }
        }
            break;
        case PacketID::FoulEvent:
        {
            auto id = evt.packet.as<std::int8_t>();
            
            if (id == BilliardsEvent::FreeTable)
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::UI::WindSock;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Callback>().active = true;
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
            else if(id < FoulStrings.size())
            {
                showNotification(FoulStrings[id]);

                auto* msg = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
                msg->type = BilliardBallEvent::Foul;
            }
        }
            break;
        case PacketID::ActorAnimation:
            if (!m_remoteCue.getComponent<cro::Model>().isHidden())
            {
                m_remoteCue.getComponent<cro::Skeleton>().play(1);
            }
            break;
        case PacketID::TurnReady:
        {
            //remove message from UI
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::MessageBoard;
            cmd.action = [&](cro::Entity e, float) {m_uiScene.destroyEntity(e); };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::NotifyPlayer:
            showReadyNotify(evt.packet.as<BilliardsPlayer>());
            break;
        case PacketID::ReadyQuitStatus:
            m_readyQuitFlags = evt.packet.as<std::uint8_t>();
            break;
        case PacketID::GameEnd:
            showGameEnd(evt.packet.as<BilliardsPlayer>());
            break;
        case PacketID::CueUpdate:
            updateGhost(evt.packet.as<BilliardsUpdate>());
            break;
        case PacketID::TableInfo:
            m_tableInfo = evt.packet.as<TableInfo>();
            break;
        case PacketID::ActorUpdate:
            updateBall(evt.packet.as<BilliardsUpdate>());
            break;
        case PacketID::EntityRemoved:
        {
            auto id = evt.packet.as<std::uint32_t>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::Ball;
            cmd.action = [&, id](cro::Entity e, float)
            {
                if (e.getComponent<InterpolationComponent<InterpolationType::Hermite>>().id == id)
                {
                    auto ballID = e.getComponent<BilliardBall>().id;
                    if (ballID != 0)
                    {
                        addPocketBall(ballID);
                    }
                    else
                    {
                        spawnPuff(e.getComponent<cro::Transform>().getPosition());
                    }

                    m_gameScene.destroyEntity(e);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::ActorSpawn:
            spawnBall(evt.packet.as<ActorInfo>());
            break;
        case PacketID::SetPlayer:
            m_wantsGameState = false;
            setPlayer(evt.packet.as<BilliardsPlayer>());
            break;
        case PacketID::ClientDisconnected:
            removeClient(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::AchievementGet:
            LogI << "TODO: Notify Achievement" << std::endl;
            break;
        case PacketID::ServerError:
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                m_sharedData.errorMessage = "Server Error (Unknown)";
                break;
            case MessageType::MapNotFound:
                m_sharedData.errorMessage = "Server Failed To Load Table Data";
                break;
            }
            requestStackPush(StateID::Error);
            break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Lobby)
            {
                requestStackClear();
                requestStackPush(StateID::Clubhouse);
            }
            break;
        }
    }
    else if (evt.type == net::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Disconnected From Server";
        requestStackPush(StateID::Error);
    }
}

void BilliardsState::removeClient(std::uint8_t clientID)
{
    cro::String str = m_sharedData.connectionData[clientID].playerData[0].name;
    for (auto i = 1u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        str += ", " + m_sharedData.connectionData[clientID].playerData[i].name;
    }
    str += " left the game";

    showNotification(str);

    m_sharedData.connectionData[clientID].playerCount = 0;
}

void BilliardsState::spawnBall(const ActorInfo& info)
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    entity.getComponent<cro::Transform>().setRotation(cro::Util::Net::decompressQuat(info.rotation));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent<InterpolationType::Hermite>>(
        InterpolationPoint(info.position, glm::vec3(0.f), cro::Util::Net::decompressQuat(info.rotation), info.timestamp)).id = info.serverID;
    entity.addComponent<BilliardBall>().id = info.state;

    m_ballDefinition.createModel(entity);
    auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    applyMaterialData(m_ballDefinition, material);
    entity.getComponent<cro::Model>().setMaterial(0, material);
    
    if (m_ballTexture.textureID)
    {
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_ballTexture);
    }

    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + (dt * 3.f));
        
        float scale = cro::Util::Easing::easeInOutBack(progress);
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    if (info.state == 0)
    {
        m_cueball = entity;

        //if the ball was placed somewhere in which the server
        //needed to clamp/correct position, we need to update
        //the player camera position accordingly
        auto correctionEnt = m_gameScene.createEntity();
        correctionEnt.addComponent<cro::Callback>().active = true;
        correctionEnt.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt) mutable
        {
            auto pos = m_cueball.getComponent<cro::Transform>().getPosition();
            auto dir = pos - m_cameraController.getComponent<cro::Transform>().getPosition();

            if (glm::length2(dir) > 0.0001f)
            {
                m_cameraController.getComponent<cro::Transform>().move(dir * (dt * 8.f));
            }
            else
            {
                m_cameraController.getComponent<cro::Transform>().setPosition(pos);
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
        };
    }

    //set colour/model based on type stored in info.state
    if (m_gameMode == TableData::Eightball
        || m_gameMode == TableData::Nineball)
    {
        auto rect = getSubrect(info.state % 16);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);
    }
    else if (m_gameMode == TableData::Snooker)
    {
        auto rect = getSubrect(info.state % 8);
        rect.z = 1.f;
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);
    }

    //remove it from the pocket display if it exists
    m_gameScene.getSystem<PocketBallSystem>()->removeBall(info.state);
}

void BilliardsState::updateBall(const BilliardsUpdate& info)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [info](cro::Entity e, float)
    {
        auto& interp = e.getComponent<InterpolationComponent<InterpolationType::Hermite>>();
        if (interp.id == info.serverID)
        {
            interp.addPoint(
                {
                    cro::Util::Net::decompressVec3(info.position, ConstVal::PositionCompressionRange),
                    cro::Util::Net::decompressVec3(info.velocity, ConstVal::VelocityCompressionRange),
                    cro::Util::Net::decompressQuat(info.rotation),
                    info.timestamp
                });
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void BilliardsState::updateGhost(const BilliardsUpdate& info)
{
    m_remoteCue.getComponent<InterpolationComponent<InterpolationType::Linear>>().addPoint(
        { 
            cro::Util::Net::decompressVec3(info.position, ConstVal::PositionCompressionRange),
            glm::vec3(0.f),
            cro::Util::Net::decompressQuat(info.rotation),
            info.timestamp 
        });
}

void BilliardsState::setPlayer(const BilliardsPlayer& playerInfo)
{
    m_localPlayerInfo[playerInfo.client][playerInfo.player].score = playerInfo.score;

    m_cameraController.getComponent<cro::Callback>().active = false;
    m_cameraController.getComponent<cro::Callback>().setUserData<float>(0.f);


    struct TargetData final
    {
        glm::vec3 camStart = glm::vec3(0.f);
        glm::vec3 camTarget = glm::vec3(0.f);

        float rotationStart = 0.f;
        float rotationTarget = 0.f;

        float elapsedTime = 0.f;
    };

    TargetData data;
    data.camStart = m_cameraController.getComponent<cro::Transform>().getPosition();
    data.rotationStart = m_cameraController.getComponent<ControllerRotation>().rotation;

    glm::vec3 lookAtPosition(0.f); //default to table centre
    const auto& balls = m_gameScene.getSystem<InterpolationSystem<InterpolationType::Hermite>>()->getEntities();
    auto result = std::find_if(balls.begin(), balls.end(),
        [playerInfo](const cro::Entity& e)
        {
            return e.getComponent<InterpolationComponent<InterpolationType::Hermite>>().id == playerInfo.targetID;
        });
    if (result != balls.end())
    {
        m_localPlayerInfo[playerInfo.client][playerInfo.player].targetBall = result->getComponent<BilliardBall>().id;
    }
    else
    {
        //TODO only do this if snooker?
        m_localPlayerInfo[playerInfo.client][playerInfo.player].targetBall = -1;
    }

    if (m_cueball.isValid())
    {
        data.camTarget = m_cueball.getComponent<cro::Transform>().getPosition();
        
        if (result != balls.end())
        {
            //don't look at cueball or ghost ball
            if (result->getComponent<BilliardBall>().id > 0)
            {
                lookAtPosition = result->getComponent<cro::Transform>().getPosition();
            }
        }
    }
    else
    {
        data.camTarget = m_tableInfo.cueballPosition;
    }

    auto targetDir = data.camTarget - lookAtPosition;
    data.rotationTarget = std::atan2(-targetDir.z, targetDir.x) + (cro::Util::Const::PI / 2.f);

    //entity performs animation in callback then pops itself when done.
    cro::Entity entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<TargetData>(data);
    entity.getComponent<cro::Callback>().function =
        [&, playerInfo](cro::Entity e, float dt) mutable
    {
        const float updateSpeed = dt;

        auto& data = e.getComponent<cro::Callback>().getUserData<TargetData>();
        data.elapsedTime = std::min(1.f, data.elapsedTime + updateSpeed);

        float interpTime = cro::Util::Easing::easeOutQuint(data.elapsedTime);

        //update controller position/rotation
        auto currRotation = data.rotationStart + cro::Util::Maths::shortestRotation(data.rotationStart, data.rotationTarget) * interpTime;
        m_cameraController.getComponent<ControllerRotation>().rotation = currRotation;
        m_cameraController.getComponent<cro::Transform>().setPosition(interpolate(data.camStart, data.camTarget, interpTime));
        
        auto cueRot = m_localCue.getComponent<ControllerRotation>().rotation;
        m_localCue.getComponent<ControllerRotation>().rotation -= cueRot / 2.f;

        if (data.elapsedTime == 1)
        {
            //wait for animation to finish first

            m_localCue.getComponent<ControllerRotation>().rotation = 0.f;

            m_cameraController.getComponent<ControllerRotation>().rotation = data.rotationTarget;
            m_cameraController.getComponent<cro::Transform>().setPosition(data.camTarget);

            m_currentPlayer.player = playerInfo.player;
            m_currentPlayer.client = playerInfo.client;

            auto playerIndex = (playerInfo.client | playerInfo.player);

            updateTargetTexture(playerIndex, m_localPlayerInfo[playerInfo.client][playerInfo.player].targetBall);
                        
            if (playerInfo.client == m_sharedData.localConnectionData.connectionID)
            {
                m_inputParser.setActive(true, !m_cueball.isValid());
                //m_uiScene.getSystem<NotificationSystem>()->clearCurrent();
                m_sharedData.inputBinding.playerID = playerInfo.player;

                m_localCue.getComponent<cro::Callback>().getUserData<CueCallbackData>().direction = CueCallbackData::In;
                m_localCue.getComponent<cro::Callback>().active = true;

                setActiveCamera(CameraID::Player);
            }
            else
            {
                m_inputParser.setActive(false, false);

                //show the remote 'ghost' cue
                m_remoteCue.getComponent<cro::Callback>().getUserData<CueCallbackData>().direction = CueCallbackData::In;
                m_remoteCue.getComponent<cro::Callback>().active = true;

                setActiveCamera(CameraID::Spectate);
            }

            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };

    auto* msg = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
    msg->type = BilliardBallEvent::TurnStarted;
    //TODO - include anything else in this message?
}

void BilliardsState::sendReadyNotify()
{
    m_uiScene.getSystem<NotificationSystem>()->clearCurrent();

    if (m_wantsNotify)
    {
        m_wantsNotify = false;
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::TurnReady, m_sharedData.localConnectionData.connectionID, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void BilliardsState::setActiveCamera(std::int32_t camID)
{
    if (camID == m_activeCamera
        || m_gameScene.getActiveCamera() == m_cameras[CameraID::Transition])
    {
        //we're already set, or already mid-transition
        return;
    }

    CRO_ASSERT(camID != CameraID::Transition, "don't try to get meta.");

    //calc start / end points
    //REMEMBER not all cameras share an FOV so this is interpolated too
    struct TransitionData final
    {
        glm::vec3 startPos = glm::vec3(0.f);
        glm::quat startRot = glm::quat(1.f, 0.f, 0.f, 0.f);
        float startFOV = 1.f;

        glm::vec3 endPos = glm::vec3(0.f);
        glm::quat endRot = glm::quat(1.f, 0.f, 0.f, 0.f);
        float endFOV = 1.f;

        float currentTime = 0.f;
    }transitionData;

    transitionData.startFOV = m_cameras[m_activeCamera].getComponent<cro::Camera>().getFOV();
    transitionData.startPos = m_cameras[m_activeCamera].getComponent<cro::Transform>().getWorldPosition();
    transitionData.startRot = glm::quat_cast(m_cameras[m_activeCamera].getComponent<cro::Transform>().getWorldTransform());

    transitionData.endFOV = m_cameras[camID].getComponent<cro::Camera>().getFOV();
    /*transitionData.endPos = m_cameras[camID].getComponent<cro::Transform>().getWorldPosition();
    transitionData.endRot = glm::quat_cast(m_cameras[camID].getComponent<cro::Transform>().getWorldTransform());*/

    //set the current cam to the transition one
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = false;
    m_gameScene.setActiveCamera(m_cameras[CameraID::Transition]);
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(transitionData.startPos);
    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(transitionData.startRot);
    m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = true;
    m_gameScene.setActiveListener(m_cameras[CameraID::Transition]);

    //create a temp entity to interp the transition camera
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, transitionData, camID](cro::Entity e, float dt) mutable
    {
        static constexpr std::array<float, CameraID::Count> TransitionSpeeds =
        {
            0.7f, 2.f, 2.f, 1.f
        };

        transitionData.currentTime = std::min(1.f, transitionData.currentTime + (dt * TransitionSpeeds[camID]));

        transitionData.endPos = m_cameras[camID].getComponent<cro::Transform>().getWorldPosition();
        transitionData.endRot = glm::quat_cast(m_cameras[camID].getComponent<cro::Transform>().getWorldTransform());

        auto t = cro::Util::Easing::easeInOutCubic(transitionData.currentTime);

        auto fov = interpolate(transitionData.startFOV, transitionData.endFOV, t);
        auto pos = interpolate(transitionData.startPos, transitionData.endPos, t);
        auto rot = glm::slerp(transitionData.startRot, transitionData.endRot, t);

        
        auto camEnt = m_cameras[CameraID::Transition];
        auto aspect = camEnt.getComponent<cro::Camera>().getAspectRatio();

        camEnt.getComponent<cro::Transform>().setPosition(pos);
        camEnt.getComponent<cro::Transform>().setRotation(rot);
        camEnt.getComponent<cro::Camera>().setPerspective(fov, aspect, 0.1f, camEnt.getComponent<CameraProperties>().farPlane);

        if (transitionData.currentTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);

            m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = false;
            m_gameScene.setActiveCamera(m_cameras[camID]);
            m_gameScene.getActiveCamera().getComponent<cro::Camera>().active = true;

            m_gameScene.setActiveListener(m_cameras[camID]);

            m_activeCamera = camID;
        }
    };


}

void BilliardsState::toggleOverhead()
{
    auto target = (m_activeCamera == CameraID::Overhead) ?
        /*m_inputParser.getActive()*/(m_currentPlayer.client == m_sharedData.localConnectionData.connectionID) ? CameraID::Player : CameraID::Spectate :
        CameraID::Overhead;
    setActiveCamera(target);
}

void BilliardsState::resizeBuffers()
{
    glm::vec2 winSize(cro::App::getWindow().getSize());

    float maxScale = getViewScale();
    float scale = m_sharedData.pixelScale ? maxScale : 1.f;
    auto texSize = winSize / scale;

    std::uint32_t samples = m_sharedData.pixelScale ? 0 :
        m_sharedData.antialias ? m_sharedData.multisamples : 0;

    m_sharedData.antialias =
        m_gameSceneTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples)
        && m_sharedData.multisamples != 0
        && !m_sharedData.pixelScale;

#ifdef CRO_DEBUG_
    auto size = m_gameSceneTexture.getSize() / 2u;
    m_debugBuffer.create(size.x, size.y);
#endif // CRO_DEBUG_

    auto invScale = (maxScale + 1.f) - scale;
    /*glCheck(glPointSize(invScale * BallPointSize));*/
    glCheck(glLineWidth(invScale));

    m_scaleBuffer.setData(invScale);

    ResolutionData d;
    d.resolution = texSize / invScale;
    m_resolutionBuffer.setData(d);



    //update the topspin texture size
    constexpr float TopspinSize = 32.f;
    texSize = { TopspinSize, TopspinSize };
    texSize *= maxScale;
    texSize /= scale;
    m_topspinTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y),true, false, samples);

    //target ball texture...
    constexpr float TargetSize = 22.f;
    texSize = { TargetSize, TargetSize };
    texSize *= maxScale;
    texSize /= scale;

    std::array<std::pair<std::int32_t, std::int32_t>, 2u> indices = 
    {
        std::make_pair(0, 0),
        std::make_pair(1, 1),
    };

    //nnnnngg
    if (m_sharedData.connectionData[0].playerCount == 1)
    {
        indices[1].second = 0;
    }
    else
    {
        indices[1].first = 0;
    }

    for (auto i = 0; i < 2; ++i)
    {
        m_targetTextures[i].create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        auto [client, player] = indices[i];
        updateTargetTexture(i, m_localPlayerInfo[client][player].targetBall);
    }


    //and trophy texture
    texSize = { 128.f, 128.f };
    texSize *= maxScale;
    texSize /= scale;
    m_trophyTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples);


    //and texture to display pocketed balls
    texSize = { 228.f, 14.f };
    texSize *= maxScale;
    texSize /= scale;
    m_pocketedTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples);
}

glm::vec4 BilliardsState::getSubrect(std::int8_t id) const
{
    static constexpr float Width = 0.5f;
    static constexpr float Height = 1.f / 8.f;
    static constexpr std::uint8_t RowCount = 8;
    static constexpr float Border = 0.001f;

    float left = Width * (id / RowCount);
    float bottom = Height * (id % RowCount);

    //despite being 'glsl compatible' vec4 is xyzw and glm::vec4 is wxyz
    glm::vec4 rect;
    rect.x = left + Border;
    rect.y = bottom;
    rect.z = Width - (Border * 2.f);
    rect.w = Height;

    return rect;
}

void BilliardsState::addPocketBall(std::int8_t id)
{
    static constexpr float startX = (-100.f + (BilliardBall::Radius * 16.f)) + BilliardBall::Radius;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ startX, 0.f, 0.f });
    entity.addComponent<PocketBall>().id = id;
    entity.addComponent<cro::AudioEmitter>() = m_audioScape.getEmitter("rolling");
    entity.getComponent<cro::AudioEmitter>().play();
    m_ballDefinition.createModel(entity);

    auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
    applyMaterialData(m_ballDefinition, material);

    entity.getComponent<cro::Model>().setMaterial(0, material);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", getSubrect(id));

    if (m_ballTexture.textureID)
    {
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_ballTexture);
    }


    auto* msg = getContext().appInstance.getMessageBus().post<BilliardBallEvent>(MessageID::BilliardsMessage);
    msg->type = BilliardBallEvent::PocketStart;
}

void BilliardsState::spawnFlea()
{
    struct FleaCallback final
    {
        explicit FleaCallback(cro::Scene& s) : scene(s) {}

        cro::Scene& scene;
        glm::vec3 velocity = glm::vec3(0.f);
        const glm::vec3 Gravity = glm::vec3(0.f, -9.8f, 0.f);
        std::int32_t hops = 0;
        const std::int32_t MaxHops = 8;

        enum
        {
            Idle, Hop
        }state = Idle;
        float idleTime = cro::Util::Random::value(0.1f, 1.2f);

        void operator()(cro::Entity e, float dt)
        {
            if (state == Idle)
            {
                idleTime -= dt;

                if (idleTime < 0)
                {
                    if (hops < MaxHops)
                    {
                        hops++;

                        glm::vec3 impulse(0.f);
                        impulse.x = cro::Util::Random::value(-0.08f, 0.08f) + 0.0001f;
                        impulse.y = cro::Util::Random::value(0.7f, 1.f) + 0.0001f;
                        impulse.z = cro::Util::Random::value(-0.08f, 0.08f) + 0.0001f;

                        velocity += impulse;

                        state = Hop;
                    }
                    else
                    {
                        e.getComponent<cro::Callback>().active = false;
                        scene.destroyEntity(e);
                    }
                }
            }
            else
            {
                velocity += Gravity * dt;
                e.getComponent<cro::Transform>().move(velocity * dt);

                auto pos = e.getComponent<cro::Transform>().getPosition();
                if (pos.y < 0)
                {
                    pos.y = 0;
                    e.getComponent<cro::Transform>().setPosition(pos);
                    state = Idle;
                    idleTime = cro::Util::Random::value(0.5f, 1.f);
                }
            }
        }
    };

    static constexpr std::array Offsets =
    {
        glm::vec3(-BilliardBall::Radius, -BilliardBall::Radius, BilliardBall::Radius),
        glm::vec3(BilliardBall::Radius,  -BilliardBall::Radius, BilliardBall::Radius),
        glm::vec3(-BilliardBall::Radius, -BilliardBall::Radius, BilliardBall::Radius),
        glm::vec3(BilliardBall::Radius,  -BilliardBall::Radius, -BilliardBall::Radius),
        glm::vec3(-BilliardBall::Radius, -BilliardBall::Radius, -BilliardBall::Radius),
        glm::vec3(BilliardBall::Radius,  -BilliardBall::Radius, -BilliardBall::Radius)
    };

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_cameraController.getComponent<cro::Transform>().getPosition() + Offsets[cro::Util::Random::value(0u, Offsets.size() - 1)]);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = FleaCallback(m_gameScene);
    m_fleaDefinition.createModel(entity);
}

void BilliardsState::spawnFace()
{
    if (m_cueball.isValid())
    {
        auto cueballPos = m_cueball.getComponent<cro::Transform>().getPosition();
        auto camPos = cueballPos - m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition();
        float rotation = std::atan2(-camPos.z, camPos.x) - (cro::Util::Const::PI / 2.f);

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(cueballPos);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getForwardVector() * -BilliardBall::Radius);
        entity.addComponent<cro::Model>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Face];

        entity.addComponent<cro::SpriteAnimation>().play(m_faceAnimationIDs[AnimationID::In]);

auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
bounds.width /= m_gameScene.getSystem<cro::SpriteSystem3D>()->getPixelsPerUnit();
bounds.height /= m_gameScene.getSystem<cro::SpriteSystem3D>()->getPixelsPerUnit();

entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

struct AnimationData final
{
    float currentTime = 0.f;
    const float BlinkTime = 3.f;
    std::int32_t blinkCount = 0;
    const std::int32_t MaxBlinks = 4;
};

entity.addComponent<cro::Callback>().active = true;
entity.getComponent<cro::Callback>().setUserData<AnimationData>();
entity.getComponent<cro::Callback>().function =
[&](cro::Entity e, float dt)
{
    if (m_inputParser.hasInput())
    {
        e.getComponent<cro::Callback>().active = false;
        m_gameScene.destroyEntity(e);

        spawnPuff(e.getComponent<cro::Transform>().getPosition());
    }

    auto& data = e.getComponent<cro::Callback>().getUserData<AnimationData>();
    data.currentTime += dt;

    if (data.currentTime > data.BlinkTime)
    {
        data.currentTime = cro::Util::Random::value(-1.f, 0.2f);
        data.blinkCount++;

        if (data.blinkCount < data.MaxBlinks)
        {
            e.getComponent<cro::SpriteAnimation>().play(m_faceAnimationIDs[AnimationID::Cycle]);
        }
        else
        {
            e.getComponent<cro::SpriteAnimation>().play(m_faceAnimationIDs[AnimationID::Out]);
        }
    }

    if (data.blinkCount == data.MaxBlinks
        && !e.getComponent<cro::SpriteAnimation>().playing)
    {
        e.getComponent<cro::Callback>().active = false;
        m_gameScene.destroyEntity(e);
    }
};
    }
}

void BilliardsState::spawnPuff(glm::vec3 position)
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::ParticleEmitter>().settings = m_smokePuff;
    entity.getComponent<cro::ParticleEmitter>().start();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (e.getComponent<cro::ParticleEmitter>().stopped())
        {
            m_gameScene.destroyEntity(e);
        }
    };
}

void BilliardsState::spawnSnail()
{
    auto cuePos = m_cameraController.getComponent<cro::Transform>().getPosition();
    auto camPos = cuePos - m_cameras[CameraID::Player].getComponent<cro::Transform>().getWorldPosition();
    float rotation = std::atan2(-camPos.z, camPos.x) - (cro::Util::Const::PI / 2.f);

    cuePos.y = 0.f;

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(cuePos);
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, rotation);
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getForwardVector() * -BilliardBall::Radius);
    entity.addComponent<cro::Model>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Snail];
    entity.addComponent<cro::SpriteAnimation>().play(m_snailAnimationIDs[AnimationID::In]);

    struct AnimationData final
    {
        enum
        {
            In, Cycle, Out
        }state = In;
        float currentTime = 0.f;;
    };

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<AnimationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<AnimationData>();
        switch (data.state)
        {
        default:
        case AnimationData::In:
            if (!e.getComponent<cro::SpriteAnimation>().playing)
            {
                e.getComponent<cro::SpriteAnimation>().play(m_snailAnimationIDs[AnimationID::Cycle]);
                data.state = AnimationData::Cycle;
            }
            break;
        case AnimationData::Cycle:
            data.currentTime += dt;
            e.getComponent<cro::Transform>().move(-e.getComponent<cro::Transform>().getRightVector() * BilliardBall::Radius * dt);

            if (data.currentTime > 5.f)
            {
                e.getComponent<cro::SpriteAnimation>().play(m_snailAnimationIDs[AnimationID::Out]);
                data.state = AnimationData::Out;
            }
            break;
        case AnimationData::Out:
            if (!e.getComponent<cro::SpriteAnimation>().playing)
            {
                e.getComponent<cro::Callback>().active = false;
                m_gameScene.destroyEntity(e);
            }
            break;
        }
    };
}