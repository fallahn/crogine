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

#include "GolfState.hpp"
#include "GameConsts.hpp"
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

#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Gamecontroller.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include "../ErrorCheck.hpp"

#include <sstream>

namespace
{
#include "WaterShader.inl"
#include "TerrainShader.inl"
#include "MinimapShader.inl"

    const cro::Time ReadyPingFreq = cro::seconds(1.f);

    //used to set the camera target
    struct TargetInfo final
    {
        float targetHeight = CameraStrokeHeight;
        float targetOffset = CameraStrokeOffset;
        float startHeight = 0.f;
        float startOffset = 0.f;

        glm::vec3 targetLookAt = glm::vec3(0.f);
        glm::vec3 currentLookAt = glm::vec3(0.f);
        glm::vec3 prevLookAt = glm::vec3(0.f);

        cro::Entity waterPlane;
    };
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus(), 512),
    m_inputParser       (sd.inputBinding, context.appInstance.getMessageBus()),
    m_wantsGameState    (true),
    m_currentHole       (0),
    m_terrainBuilder    (m_holeData),
    m_camRotation       (0.f),
    m_roundEnded        (false),
    m_viewScale         (1.f),
    m_scoreColumnCount  (2)
{
    context.mainWindow.loadResources([this]() {
        loadAssets();
        addSystems();
        initAudio();
        buildScene();
        });

    context.mainWindow.setMouseCaptured(true);
    sd.baseState = StateID::Game;

    //glLineWidth(1.5f);
#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("buns"))
    //        {
    //            /*auto pin = m_holeData[m_currentHole].pin;
    //            auto target = m_holeData[m_currentHole].target;
    //            auto currLookAt = m_gameScene.getActiveCamera().getComponent<TargetInfo>().currentLookAt;
    //            ImGui::Text("Pin: %3.3f, %3.3f", pin.x, pin.z);
    //            ImGui::Text("Target: %3.3f, %3.3f", target.x, target.z);
    //            ImGui::Text("Look At: %3.3f, %3.3f", currLookAt.x, currLookAt.z);*/

    //            ImGui::Text("Cam Rotation: %3.3f", m_camRotation);

    //            //static float sunRot = 0.f;
    //            //auto prevRot = sunRot;
    //            //if (ImGui::SliderFloat("Sun", &sunRot, -90.f, 90.f))
    //            //{
    //            //    m_greenCam.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, (prevRot - sunRot) * cro::Util::Const::degToRad);
    //            //    //shadowEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, sunRot * cro::Util::Const::degToRad);
    //            //}
    //            //ImGui::Text("Shadow Rot: %3.3f", sunRot * cro::Util::Const::degToRad);

    //            /*auto rot = m_inputParser.getYaw();
    //            ImGui::Text("Rotation %3.2f", rot);

    //            auto power = m_inputParser.getPower();
    //            ImGui::Text("Power: %3.3f", power);

    //            auto hook = m_inputParser.getHook();
    //            ImGui::Text("Hook: %3.3f", hook);*/

    //            static float scale = 1.f;
    //            if (ImGui::SliderFloat("Scale", &scale, 0.f, 1.f))
    //            {
    //                m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });
    //            }

    //            float height = m_gameScene.getActiveCamera().getComponent<cro::Transform>().getPosition().y;
    //            ImGui::Text("Cam height: %3.3f", height);

    //            auto club = Clubs[getClub()];
    //            ImGui::Text("Club: %s", club);

    //            ImGui::Text("Terrain: %s", TerrainStrings[m_currentPlayer.terrain]);

    //            for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    //            {
    //                if (m_sharedData.connectionData[i].playerCount > 0)
    //                {
    //                    for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
    //                    {
    //                        ImGui::Image(m_sharedData.avatarTextures[i][j], { 338.f, 112.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //                    }
    //                }
    //            }

    //            //ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().reflectionBuffer.getTexture(), { 300.f, 300.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //            //ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(), { 300.f, 300.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //        }
    //        ImGui::End();
    //    });
#endif
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return true;
    }


    const auto scrollScores = [&](std::int32_t step)
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::ScoreScroll;
        cmd.action = [step](cro::Entity e, float)
        {
            e.getComponent<cro::Callback>().getUserData<std::int32_t>() = step;
            e.getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            showScoreboard(false);
            break;
#ifdef CRO_DEBUG_
        case SDLK_F2:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextHole), cro::NetFlag::Reliable);
            break;
        case SDLK_F3:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::NextPlayer), cro::NetFlag::Reliable);
            break;
        case SDLK_F4:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::GotoGreen), cro::NetFlag::Reliable);
            break;
        case SDLK_F6:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ServerCommand, std::uint8_t(ServerCommand::EndGame), cro::NetFlag::Reliable);
            break;
        case SDLK_F7:
            //showCountdown(10);
            //showMessageBoard(MessageBoardID::Scrub);
            requestStackPush(StateID::Tutorial);
            break;
        case SDLK_F8:
            //showMessageBoard(MessageBoardID::Bunker);
            //updateMiniMap();
            //removeClient(1);
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            showScoreboard(true);
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
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
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
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonBack:
            showScoreboard(false);
            break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
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
    }

    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //TODO check if any players are using the controller
        //and reassign any still connected devices
        for (auto i = 0; i < 4; ++i)
        {
            if (evt.cdevice.which == cro::GameController::deviceID(i))
            {

            }
        }
    }


    m_inputParser.handleEvent(evt);

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::SystemMessage:
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::StateRequest)
        {
            requestStackPush(data.data);
        }
    }
        break;
    case cro::Message::SpriteAnimationMessage:
    {
        const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
        if (data.userType == 0)
        {
            //relay this message with the info needed for particle/sound effects
            auto* msg2 = cro::App::getInstance().getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
            msg2->type = GolfEvent::ClubSwing;
            msg2->position = m_currentPlayer.position;
            msg2->terrain = m_currentPlayer.terrain;
            msg2->club = static_cast<std::uint8_t>(getClub());

            cro::GameController::rumbleStart(m_sharedData.inputBinding.controllerID, 0.5f, 100);
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
            updateMiniMap();
        }
            break;
        }
    }
        break;
    case MessageID::GolfMessage:
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::HitBall)
        {
            hitBall();
        }
        else if (data.type == GolfEvent::ClubChanged)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::StrokeIndicator;
            cmd.action = [&](cro::Entity e, float) 
            {
                float scale = Clubs[getClub()].power / Clubs[ClubID::Driver].power;
                e.getComponent<cro::Transform>().setScale({ scale, 1.f });
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            //update the sprite with correct club
            cmd.targetFlags = CommandID::UI::PlayerSprite;
            cmd.action = [&](cro::Entity e, float)
            {
                auto colour = e.getComponent<cro::Sprite>().getColour();
                if (getClub() < ClubID::FiveIron)
                {
                    e.getComponent<cro::Sprite>() = m_avatars[m_currentPlayer.client][m_currentPlayer.player].wood;
                }
                else
                {
                    e.getComponent<cro::Sprite>() = m_avatars[m_currentPlayer.client][m_currentPlayer.player].iron;
                }
                e.getComponent<cro::Sprite>().setColour(colour);
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            //update club text colour based on distance
            cmd.targetFlags = CommandID::UI::ClubName;
            cmd.action = [&](cro::Entity e, float)
            {
                auto dist = glm::length(m_currentPlayer.position - m_holeData[m_currentHole].pin) * 1.67f;
                if (getClub() < ClubID::NineIron &&
                    Clubs[getClub()].target > dist)
                {
                    e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                }
                else
                {
                    e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                }
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
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

                //make sure to set the correct controller
                m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[m_currentPlayer.player];
            }
        }
    }
        break;
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            //handle events
            handleNetEvent(evt);
        }

        if (m_wantsGameState)
        {
            if (m_readyClock.elapsed() > ReadyPingFreq)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.connectionID, cro::NetFlag::Reliable);
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

    //update time uniforms
    static float elapsed = dt;
    elapsed += dt;

    glUseProgram(m_waterShader.shaderID);
    glUniform1f(m_waterShader.timeUniform, elapsed * 15.f);
    //glUseProgram(0);

    m_terrainBuilder.updateTime(elapsed * 10.f);

    m_inputParser.update(dt);

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    return true;
}

void GolfState::render()
{
    //render reflections first
    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVP = cam.viewport;

    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);
    cam.reflectionBuffer.clear(cro::Colour::Red);
    m_gameScene.render(cam.reflectionBuffer);
    cam.reflectionBuffer.display();

    cam.setActivePass(cro::Camera::Pass::Final);
    cam.viewport = oldVP;

    //then render scene
    m_gameSceneTexture.clear();
    m_gameScene.render(m_gameSceneTexture);
    m_gameSceneTexture.display();

    //update mini green if ball is there
    if (m_currentPlayer.terrain == TerrainID::Green)
    {
        auto oldCam = m_gameScene.setActiveCamera(m_greenCam);
        m_greenBuffer.clear();
        m_gameScene.render(m_greenBuffer);
        m_greenBuffer.display();
        m_gameScene.setActiveCamera(oldCam);
    }

    m_uiScene.render(cro::App::getWindow());
}

//private
void GolfState::loadAssets()
{
    //load materials
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    //cel shaded material
    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n");
    auto* shader = &m_resources.shaders.get(ShaderID::Cel);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define NORMAL_MAP\n");
    shader = &m_resources.shaders.get(ShaderID::Course);
    m_materialIDs[MaterialID::Course] = m_resources.materials.add(*shader);


    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    m_materialIDs[MaterialID::WireFrame] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(m_materialIDs[MaterialID::WireFrame]).blendMode = cro::Material::BlendMode::Alpha;

    m_resources.shaders.loadFromString(ShaderID::Minimap, MinimapVertex, MinimapFragment);

    m_resources.shaders.loadFromString(ShaderID::Water, WaterVertex, WaterFragment);
    m_materialIDs[MaterialID::Water] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Water));

    m_waterShader.shaderID = m_resources.shaders.get(ShaderID::Water).getGLHandle();
    m_waterShader.timeUniform = m_resources.shaders.get(ShaderID::Water).getUniformMap().at("u_time");
    


    //model definitions
    for (auto& md : m_modelDefs)
    {
        md = std::make_unique<cro::ModelDefinition>(m_resources);
    }
    m_modelDefs[ModelID::Ball]->loadFromFile("assets/golf/models/ball.cmt");
    m_modelDefs[ModelID::BallShadow]->loadFromFile("assets/golf/models/ball_shadow.cmt");

    //UI stuffs
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);
    m_sprites[SpriteID::PowerBar] = spriteSheet.getSprite("power_bar");
    m_sprites[SpriteID::PowerBarInner] = spriteSheet.getSprite("power_bar_inner");
    m_sprites[SpriteID::HookBar] = spriteSheet.getSprite("hook_bar");
    m_sprites[SpriteID::WindIndicator] = spriteSheet.getSprite("wind_dir");
    m_sprites[SpriteID::MessageBoard] = spriteSheet.getSprite("message_board");
    m_sprites[SpriteID::Bunker] = spriteSheet.getSprite("bunker");
    m_sprites[SpriteID::Foul] = spriteSheet.getSprite("foul");
    auto flagSprite = spriteSheet.getSprite("flag03");
    m_flagQuad.setTexture(*flagSprite.getTexture());
    m_flagQuad.setTextureRect(flagSprite.getTextureRect());

    //these are doubled because odd skinIDs are flipped
    //versions of even numbered
    spriteSheet.loadFromFile("assets/golf/sprites/player.spt", m_resources.textures);
    std::vector<cro::Sprite> ironSprites =
    {
        spriteSheet.getSprite("female_iron"),
        spriteSheet.getSprite("female_iron"),
        spriteSheet.getSprite("male_iron"),
        spriteSheet.getSprite("male_iron"),
        spriteSheet.getSprite("female_iron_02"),
        spriteSheet.getSprite("female_iron_02"),
        spriteSheet.getSprite("male_iron_02"),
        spriteSheet.getSprite("male_iron_02")
    };

    std::vector<cro::Sprite> woodSprites =
    {
        spriteSheet.getSprite("female_wood"),
        spriteSheet.getSprite("female_wood"),
        spriteSheet.getSprite("male_wood"),
        spriteSheet.getSprite("male_wood"),
        spriteSheet.getSprite("female_wood_02"),
        spriteSheet.getSprite("female_wood_02"),
        spriteSheet.getSprite("male_wood_02"),
        spriteSheet.getSprite("male_wood_02")
    };

    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            auto skinID = std::min(m_sharedData.connectionData[i].playerData[j].skinID, std::uint8_t(PlayerAvatar::MaxSkins - 1));
            m_avatars[i][j].iron = ironSprites[skinID];
            m_avatars[i][j].wood = woodSprites[skinID];
            m_avatars[i][j].flipped = (skinID % 2);

            m_avatars[i][j].iron.setTexture(m_sharedData.avatarTextures[i][j], false);
            m_avatars[i][j].wood.setTexture(m_sharedData.avatarTextures[i][j], false);
        }
    }

    //ball resources - ball is rendered as a single point
    //at a distance, and as a model when closer
    glCheck(glPointSize(BallPointSize));

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    m_ballResources.materialID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_ballResources.ballMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));
    m_ballResources.shadowMeshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_POINTS));

    auto* meshData = &m_resources.meshes.getMesh(m_ballResources.ballMeshID);
    std::vector<float> verts =
    {
        0.f, 0.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f
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
        0.f, 0.f, 0.f,    0.5f, 0.5f, 0.5f, 1.f,
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


    //pre-process the crowd geometry
    cro::ModelDefinition billboardDef(m_resources);
    cro::SpriteSheet crowdSprites;
    crowdSprites.loadFromFile("assets/golf/sprites/crowd.spt", m_resources.textures);
    const auto& sprites = crowdSprites.getSprites();
    std::vector<cro::Billboard> billboards;
    for (const auto& [name, spr] : sprites)
    {
        billboards.push_back(spriteToBillboard(spr));
    }

    //used when parsing holes
    auto addCrowd = [&](HoleData& holeData, glm::vec3 position, float rotation)
    {
        //reload to ensure unique VBO
        if (!billboards.empty() &&
            billboardDef.loadFromFile("assets/golf/models/crowd.cmt"))
        {
            auto ent = m_gameScene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(position);
            ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
            billboardDef.createModel(ent);

            glm::vec3 bbPos(-8.f, 0.f, 0.f);
            for (auto i = 0; i < 16; ++i)
            {
                bbPos.x += 0.5f + (static_cast<float>(cro::Util::Random::value(5, 10)) / 10.f);
                bbPos.z = static_cast<float>(cro::Util::Random::value(-10, 10)) / 10.f;

                //images are a little oversized at 2.5m...
                auto bb = billboards[(i % billboards.size()) + cro::Util::Random::value(0, 2)];
                bb.size *= static_cast<float>(cro::Util::Random::value(65, 75)) / 100.f;
                bb.position = bbPos;
                ent.getComponent<cro::BillboardCollection>().addBillboard(bb);
            }

            ent.getComponent<cro::Model>().setHidden(true);

            holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            holeData.propEntities.push_back(ent);
        }
    };


    //load the map data
    bool error = false;
    auto mapDir = m_sharedData.mapDirectory.toAnsiString();
    auto mapPath = ConstVal::MapPath + mapDir + "/course.data";

    if (!cro::FileSystem::fileExists(mapPath))
    {
        LOG("Course file doesn't exist", cro::Logger::Type::Error);
        error = true;
    }

    cro::ConfigFile courseFile;
    if (!courseFile.loadFromFile(mapPath))
    {
        error = true;
    }

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
            m_gameScene.setCubemap(prop.getValue<std::string>());

            //disable smoothing for super-pixels
            glCheck(glBindTexture(GL_TEXTURE_CUBE_MAP, m_gameScene.getCubemap().textureID));
            glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            glCheck(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        }
    }

    if (holeStrings.empty())
    {
        LOG("No hole files in course data", cro::Logger::Type::Error);
        error = true;
    }

    cro::ConfigFile holeCfg;
    cro::ModelDefinition modelDef(m_resources);
    for (const auto& hole : holeStrings)
    {
        if (!cro::FileSystem::fileExists(hole))
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

        const auto& holeProps = holeCfg.getProperties();
        for (const auto& holeProp : holeProps)
        {
            const auto& name = holeProp.getName();
            if (name == "map")
            {
                if (!m_currentMap.loadFromFile(holeProp.getValue<std::string>()))
                {
                    error = true;
                }
                holeData.mapPath = holeProp.getValue<std::string>();
                propCount++;
            }
            else if (name == "pin")
            {
                //TODO not sure how we ensure these are sane values?
                auto pin = holeProp.getValue<glm::vec2>();
                holeData.pin = { pin.x, 0.f, -pin.y };
                propCount++;
            }
            else if (name == "tee")
            {
                auto tee = holeProp.getValue<glm::vec2>();
                holeData.tee = { tee.x, 0.f, -tee.y };
                propCount++;
            }
            else if (name == "target")
            {
                auto target = holeProp.getValue<glm::vec2>();
                holeData.target = { target.x, 0.f, -target.y };
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
                if (modelDef.loadFromFile(holeProp.getValue<std::string>()))
                {
                    auto material = m_resources.materials.get(m_materialIDs[MaterialID::Course]);
                    setTexture(modelDef, material);

                    holeData.modelEntity = m_gameScene.createEntity();
                    holeData.modelEntity.addComponent<cro::Transform>();
                    holeData.modelEntity.addComponent<cro::Callback>();
                    modelDef.createModel(holeData.modelEntity);
                    holeData.modelEntity.getComponent<cro::Model>().setHidden(true);
                    holeData.modelEntity.getComponent<cro::Model>().setMaterial(0, material);
                    propCount++;
                }
                else
                {
                    LOG("Failed loading model file", cro::Logger::Type::Error);
                    error = true;
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
                    std::string path;

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
                    }

                    if (!path.empty()
                        && cro::FileSystem::fileExists(path))
                    {
                        if (modelDef.loadFromFile(path))
                        {
                            auto ent = m_gameScene.createEntity();
                            ent.addComponent<cro::Transform>().setPosition(position);
                            ent.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation* cro::Util::Const::degToRad);
                            modelDef.createModel(ent);
                            if (modelDef.hasSkeleton())
                            {
                                ent.getComponent<cro::Skeleton>().play(0);

                                //TODO we need to specialise the material for skinned models.
                            }
                            else
                            {
                                auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                                setTexture(modelDef, texturedMat);
                                ent.getComponent<cro::Model>().setMaterial(0, texturedMat);
                            }
                            ent.getComponent<cro::Model>().setHidden(true);

                            holeData.modelEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                            holeData.propEntities.push_back(ent);
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

                    addCrowd(holeData, position, rotation);
                }
            }
        }
    }


    if (error)
    {
        m_sharedData.errorMessage = "Failed to load course data";
        requestStackPush(StateID::Error);
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

}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<ClientCollisionSystem>(mb, m_holeData);
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SkeletalAnimator>(mb);
    m_gameScene.addSystem<cro::BillboardSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    m_gameScene.addDirector<GolfParticleDirector>(m_resources.textures);
    m_gameScene.addDirector<GolfSoundDirector>(m_resources.audio);

    if (m_sharedData.tutorial)
    {
        m_gameScene.addDirector<TutorialDirector>(m_sharedData);
    }

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GolfState::buildScene()
{
    if (m_holeData.empty())
    {
        return;
    }


    //quality holing
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/cup.cmt");
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);

    auto holeEntity = entity;

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

    auto flagEntity = entity;

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
    material.blendMode = cro::Material::BlendMode::Alpha;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();

    std::vector<float> verts =
    {
        0.f, 0.05f, 0.f,    1.f, 0.97f, 0.88f, 1.f,
        5.f, 0.1f, 0.f,    1.f, 0.97f, 0.88f, 0.2f
    };
    std::vector<std::uint32_t> indices =
    {
        0,1
    };

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
    entity.getComponent<cro::Model>().setRenderFlags(~(RenderFlags::MiniGreen | RenderFlags::MiniMap));


    //draw the flag pole as a single line which can be
    //see from a distance - hole and model are also attached to this
    meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Hole;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    entity.getComponent<cro::Transform>().addChild(holeEntity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(flagEntity.getComponent<cro::Transform>());

    meshData = &entity.getComponent<cro::Model>().getMeshData();
    verts =
    {
        0.f, 2.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f, 0.f, 0.f,    LeaderboardTextLight.getRed(), LeaderboardTextLight.getGreen(), LeaderboardTextLight.getBlue(), 1.f,
        0.f,  0.001f,  0.f,    0.f, 0.f, 0.f, 0.5f,
        1.4f, 0.001f, 1.4f,    0.f, 0.f, 0.f, 0.01f,
    };
    indices =
    {
        0,1,2,3
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



    //attach a water plane to the camera
    //this is updated when positioning the camera
    meshID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(150.f, 30));
    auto waterEnt = m_gameScene.createEntity();
    waterEnt.addComponent<cro::Transform>().setPosition(m_holeData[0].pin);
    waterEnt.getComponent<cro::Transform>().move({ 0.f, 0.f, -30.f });
    waterEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 2.f);
    waterEnt.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(m_materialIDs[MaterialID::Water]));
    waterEnt.getComponent<cro::Model>().setRenderFlags(~RenderFlags::MiniMap);
    m_gameScene.setWaterLevel(WaterLevel);


    //tee marker
    md.loadFromFile("assets/golf/models/tee_balls.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].tee);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Tee;
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Cel]));
    
    auto targetDir = m_holeData[m_currentHole].target - m_holeData[0].tee;
    m_camRotation = std::atan2(-targetDir.z, targetDir.x);
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);

    auto teeEnt = entity;


    //player shadow
    static constexpr float ShadowScale = 20.f;
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_holeData[0].tee);
    entity.getComponent<cro::Transform>().setOrigin({0.f, 0.f, 0.04f});
    entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f, 1.f, ShadowScale));
    entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_camRotation);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::PlayerShadow;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 2.f));

        e.getComponent<cro::Transform>().setScale({ ShadowScale * cro::Util::Easing::easeOutBounce(currTime), 1.f, ShadowScale });

        if(currTime == 1)
        {
            currTime = 0.f;
            e.getComponent<cro::Callback>().active = false;
        }
    };
    m_modelDefs[ModelID::BallShadow]->createModel(entity);


    //carts
    md.loadFromFile("assets/golf/models/cart.cmt");
    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    setTexture(md, texturedMat);
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
            entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
            teeEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();
        m_gameSceneTexture.create(static_cast<std::uint32_t>(vpSize.x), static_cast<std::uint32_t>(vpSize.y));

        //the resize actually extends the target vertically so we need to maintain a
        //horizontal FOV, not the vertical one expected by default.
        cam.setPerspective(FOV * (vpSize.y / ViewportHeight), vpSize.x / vpSize.y, 0.1f, vpSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        //because we don't know in which order the cam callbacks are raised
        //we need to send the player repos command from here when we know the view is correct
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::PlayerSprite;
        cmd.action = [&](cro::Entity e, float)
        {
            const auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
            auto pos = camera.coordsToPixel(m_currentPlayer.position, m_gameSceneTexture.getSize());
            e.getComponent<cro::Transform>().setPosition(pos);
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    auto camEnt = m_gameScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);


    //used by transition callback to interp camera
    camEnt.addComponent<TargetInfo>().waterPlane = waterEnt;
    camEnt.getComponent<TargetInfo>().targetLookAt = m_holeData[0].target;
    cam.reflectionBuffer.create(1024, 1024);

    m_terrainBuilder.create(m_resources, m_gameScene);

    m_currentPlayer.position = m_holeData[m_currentHole].tee; //prevents the initial camera movement

    buildUI(); //put this here because we don't want to do this if the map data didn't load
    setCurrentHole(0);

    //careful with these values - they are fine tuned for shadowing of terrain
    auto sunEnt = m_gameScene.getSunlight();
    //sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, /*-0.967f*/-45.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, /*-1.5f*/-38.746f * cro::Util::Const::degToRad);
}

void GolfState::initAudio()
{
    //4 evenly spaced points with ambient audio
    auto envOffset = glm::vec2(MapSize) / 3.f;
    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/ambience.xas", m_resources.audio);

    std::array emitterNames =
    {
        std::string("01"),
        std::string("02"),
        std::string("03"),
        std::string("04"),
        std::string("05"),
        std::string("06"),
        std::string("05"),
        std::string("06"),
    };

    for (auto i = 0; i < 2; ++i)
    {
        for (auto j = 0; j < 2; ++j)
        {
            static constexpr float height = 4.f;
            glm::vec3 position(envOffset.x * (i + 1), height, -envOffset.y * (j + 1));
            
            auto idx = i * 2 + j;
            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(position);
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx + 4]);
            entity.getComponent<cro::AudioEmitter>().play();

            position = { i * MapSize.x, height, -static_cast<float>(MapSize.y) * j };
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(position);
            entity.addComponent<cro::AudioEmitter>() = as.getEmitter(emitterNames[idx]);
            entity.getComponent<cro::AudioEmitter>().play();
        }
    }

    //random plane audio
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("plane");
    auto plane01 = entity;

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("plane02");
    auto plane02 = entity;

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, float>>(0.f, static_cast<float>(cro::Util::Random::value(12, 64)));
    entity.getComponent<cro::Callback>().function =
        [plane01, plane02](cro::Entity e, float dt) mutable
    {
        auto& [currTime, timeOut] = e.getComponent<cro::Callback>().getUserData<std::pair<float, float>>();
        currTime += dt;

        if (currTime > timeOut)
        {
            currTime = 0.f;
            timeOut = static_cast<float>(cro::Util::Random::value(90, 170));

            auto ent = cro::Util::Random::value(0, 20) % 2 == 0 ? plane01 : plane02;
            if (ent.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
            {
                ent.getComponent<cro::AudioEmitter>().play();
            }
        }
    };


    //put the new hole music on the cam for accessabilty
    m_gameScene.getActiveCamera().addComponent<cro::AudioEmitter>() = as.getEmitter("music");
}

void GolfState::spawnBall(const ActorInfo& info)
{
    //render the ball as a point so no perspective is applied to the scale
    auto material = m_resources.materials.get(m_ballResources.materialID);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, -0.003f, 0.f }); //pushes the ent above the ground a bit to stop Z fighting
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Ball;
    entity.addComponent<InterpolationComponent>().setID(info.serverID);
    entity.addComponent<ClientCollider>().previousPosition = info.position;
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.ballMeshID), material);

    //ball shadow
    auto ballEnt = entity;
    material.blendMode = cro::Material::BlendMode::Multiply;

    //point shadow seen from distance
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(info.position);
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
            auto ballPos = ballEnt.getComponent<cro::Transform>().getPosition();
            ballPos.y = 0.003f; //just to prevent z-fighting
            e.getComponent<cro::Transform>().setPosition(ballPos);
        }
    };
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(m_ballResources.shadowMeshID), material);

    //large shadow seen close up
    auto shadowEnt = entity;
    entity = m_gameScene.createEntity();
    shadowEnt.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    m_modelDefs[ModelID::BallShadow]->createModel(entity);
    entity.getComponent<cro::Transform>().setScale(glm::vec3(1.3f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&,ballEnt](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };

    //adding a ball model means we see something a bit more reasonable when close up
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, ballEnt](cro::Entity e, float)
    {
        if (ballEnt.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);
        }
    };
    m_modelDefs[ModelID::Ball]->createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, m_resources.materials.get(m_materialIDs[MaterialID::Cel]));
    ballEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void GolfState::handleNetEvent(const cro::NetEvent& evt)
{
    switch (evt.type)
    {
    case cro::NetEvent::PacketReceived:
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::BallLanded:
        {
            auto terrain = evt.packet.as<std::uint8_t>();
            switch (terrain)
            {
            default: break;
            case TerrainID::Bunker:
                showMessageBoard(MessageBoardID::Bunker);
                break;
            case TerrainID::Scrub:
                showMessageBoard(MessageBoardID::Scrub);
                break;
            case TerrainID::Water:
                showMessageBoard(MessageBoardID::Water);
                break;
            case TerrainID::Hole:
                showMessageBoard(MessageBoardID::HoleScore);
                break;
            }
        }
            break;
        case PacketID::ClientDisconnected:
        {
            removeClient(evt.packet.as<std::uint8_t>());
        }
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
            auto animID = evt.packet.as<std::uint8_t>();
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::PlayerSprite;
            cmd.action = [animID](cro::Entity e, float)
            {
                e.getComponent<cro::SpriteAnimation>().play(animID);
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
            break;
        case PacketID::WindDirection:
            updateWindDisplay(cro::Util::Net::decompressVec3(evt.packet.as<std::array<std::int16_t, 3u>>()));
            break;
        case PacketID::SetHole:
            setCurrentHole(evt.packet.as<std::uint8_t>());
            break;
        case PacketID::ScoreUpdate:
        {
            auto su = evt.packet.as<ScoreUpdate>();
            auto& player = m_sharedData.connectionData[su.client].playerData[su.player];
            
            if (su.hole < player.holeScores.size())
            {
                player.score = su.score;
                player.holeScores[su.hole] = su.stroke;
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
                if (e.getComponent<InterpolationComponent>().getID() == idx)
                {
                    m_gameScene.destroyEntity(e);
                }
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        }
            break;
        }
        break;
    case cro::NetEvent::ClientDisconnect:
        m_sharedData.errorMessage = "Disconnected From Server (Host Quit)";
        requestStackPush(StateID::Error);
        break;
    default: break;
    }
}

void GolfState::removeClient(std::uint8_t clientID)
{
    cro::String str = m_sharedData.connectionData[clientID].playerData[0].name;
    for (auto i = 1u; i < m_sharedData.connectionData[clientID].playerCount; ++i)
    {
        str += ", " + m_sharedData.connectionData[clientID].playerData[i].name;
    }
    str += " left the game";

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, UIBarHeight * m_viewScale.y * 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::UI)).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(8u * static_cast<std::uint32_t>(m_viewScale.y));
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextLight);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(5.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        if (currTime < 0)
        {
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        }
    };

    m_sharedData.connectionData[clientID].playerCount = 0;
}

void GolfState::setCurrentHole(std::uint32_t hole)
{
    updateScoreboard();
    showScoreboard(!m_sharedData.tutorial);

    //CRO_ASSERT(hole < m_holeData.size(), "");
    if (hole >= m_holeData.size())
    {
        m_sharedData.errorMessage = "Server requested hole\nnot found";
        requestStackPush(StateID::Error);
        return;
    }

    m_terrainBuilder.update(hole);
    m_gameScene.getSystem<ClientCollisionSystem>().setMap(hole);

    //create hole model transition
    auto* propModels = &m_holeData[m_currentHole].propEntities;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().active = true;
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Callback>().function =
        [&, propModels](cro::Entity e, float dt)
    {
        auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
        progress = std::min(1.f, progress + dt);

        float scale = 1.f - progress;
        e.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });

        if (progress == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            e.getComponent<cro::Model>().setHidden(true);

            for (auto i = 0u; i < propModels->size(); ++i)
            {
                propModels->at(i).getComponent<cro::Model>().setHidden(true);
            }

            //index should be updated by now (as this is a callback)
            //so we're actually targetting the next hole entity
            auto entity = m_holeData[m_currentHole].modelEntity;
            entity.getComponent<cro::Model>().setHidden(false);
            entity.getComponent<cro::Transform>().setScale({ 0.f, 1.f, 0.f });
            entity.getComponent<cro::Callback>().setUserData<float>(0.f);
            entity.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity ent, float dt)
            {
                auto& progress = ent.getComponent<cro::Callback>().getUserData<float>();
                progress = std::min(1.f, progress + dt);

                float scale = progress;
                ent.getComponent<cro::Transform>().setScale({ scale, 1.f, scale });

                if (progress == 1)
                {
                    ent.getComponent<cro::Callback>().active = false;
                }
            };

            //unhide any prop models
            for (auto prop : m_holeData[m_currentHole].propEntities)
            {
                prop.getComponent<cro::Model>().setHidden(false);
            }
        }
    };

    m_currentHole = hole;

    //map collision data
    m_currentMap.loadFromFile(m_holeData[m_currentHole].mapPath);
    glm::vec2 size(m_currentMap.getSize());
    m_holeData[m_currentHole].modelEntity.getComponent<cro::Transform>().setPosition({ size.x / 2.f, 0.f, -size.y / 2.f });


    //make sure we have the correct target position
    m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetHeight = CameraStrokeHeight;
    m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetOffset = CameraStrokeOffset;
    m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetLookAt = m_holeData[m_currentHole].target;

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

        auto& targetInfo = m_gameScene.getActiveCamera().getComponent<TargetInfo>();

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
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


            auto teeMove = m_holeData[m_currentHole].tee - m_holeData[m_currentHole - 1].tee;
            auto teePos = m_holeData[m_currentHole - 1].tee + (teeMove * percent);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&,teePos](cro::Entity e, float)
            {
                e.getComponent<cro::Transform>().setPosition(teePos);

                auto pinDir = m_holeData[m_currentHole].target - teePos;
                auto rotation = std::atan2(-pinDir.z, pinDir.x);
                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            auto targetDir = m_holeData[m_currentHole].target - currPos;
            m_camRotation = std::atan2(-targetDir.z, targetDir.x);

            //randomise the cart positions a bit
            cmd.targetFlags = CommandID::Cart;
            cmd.action = [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
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

            //we're there
            setCameraPosition(m_holeData[m_currentHole].tee, targetInfo.targetHeight, targetInfo.targetOffset);

            //set tee / flag
            cro::Command cmd;
            cmd.targetFlags = CommandID::Hole;
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].pin);
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            cmd.targetFlags = CommandID::Tee;
            cmd.action = [&](cro::Entity en, float)
            {
                en.getComponent<cro::Transform>().setPosition(m_holeData[m_currentHole].tee);
            };
            m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

            //remove the transition ent
            e.getComponent<cro::Callback>().active = false;
            m_gameScene.destroyEntity(e);

            //play the music
            m_gameScene.getActiveCamera().getComponent<cro::AudioEmitter>().play();
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


    //this is called by setCurrentPlayer, but doing it here ensures that
    //each player starts a new hole on a driver/3 wood
    m_inputParser.setHoleDirection(m_holeData[m_currentHole].target - m_currentPlayer.position, true);
    m_currentPlayer.terrain = TerrainID::Fairway;

    //hide the slope indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [](cro::Entity e, float)
    {
        //e.getComponent<cro::Model>().setHidden(true);
        e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //update the UI
    cmd.targetFlags = CommandID::UI::HoleNumber;
    cmd.action =
        [&](cro::Entity e, float)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.string = "Hole: " + std::to_string(m_currentHole + 1);
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    m_terrainBuilder.setSlopePosition(m_holeData[m_currentHole].pin);

    //set green cam position
    auto holePos = m_holeData[m_currentHole].pin;
    m_greenCam.getComponent<cro::Transform>().setPosition({ holePos.x, 0.9f, holePos.z });
}

void GolfState::setCameraPosition(glm::vec3 position, float height, float viewOffset)
{
    static constexpr float MinDist = 6.f;
    static constexpr float MaxDist = 270.f;
    static constexpr float DistDiff = MaxDist - MinDist;
    float heightMultiplier = 1.f; //goes to -1.f at max dist

    auto camEnt = m_gameScene.getActiveCamera();
    auto& targetInfo = camEnt.getComponent<TargetInfo>();
    auto target = targetInfo.currentLookAt - position;

    auto dist = glm::length(target);
    float distNorm = std::min(1.f, (dist - MinDist) / DistDiff);
    heightMultiplier -= (2.f * distNorm);

    target *= 1.f - ((1.f - 0.08f) * distNorm);
    target += position;

    camEnt.getComponent<cro::Transform>().setPosition({ position.x, height, position.z });

    auto lookat = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), glm::vec3(target.x, height * heightMultiplier, target.z), cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(lookat));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * viewOffset);

    targetInfo.waterPlane.getComponent<cro::Transform>().setPosition({ target.x, WaterLevel, target.z });
}

void GolfState::setCurrentPlayer(const ActivePlayer& player)
{
    updateScoreboard();
    showScoreboard(false);

    auto localPlayer = (player.client == m_sharedData.clientConnection.connectionID);

    //self destructing ent to provide delay before popping up player name
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.5f);
    entity.getComponent<cro::Callback>().function =
        [&, localPlayer](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        if (currTime <= 0)
        {
            showMessageBoard(MessageBoardID::PlayerName);
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);

            m_inputParser.setActive(localPlayer);
        }
    };
    
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
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);    


    cmd.targetFlags = CommandID::UI::PinDistance;
    cmd.action =
        [&, player](cro::Entity e, float)
    {
        //if we're on the green convert to cm
        float ballDist = glm::length(player.position - m_holeData[m_currentHole].pin);
        std::int32_t distance = 0;

        if (ballDist > 5)
        {
            distance = static_cast<std::int32_t>(ballDist);
            e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "m");
        }
        else
        {
            distance = static_cast<std::int32_t>(ballDist * 100.f);
            e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "cm");
        }

        auto bounds = cro::Text::getLocalBounds(e);
        bounds.width = std::floor(bounds.width / 2.f);
        e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    cmd.targetFlags = CommandID::UI::MiniBall;
    cmd.action =
        [player](cro::Entity e, float)
    {
        auto pos = glm::vec3(player.position.x, -player.position.z, 0.1f);
        e.getComponent<cro::Transform>().setPosition(pos / 2.f);

        //play the callback animation
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //show ui if this is our client    
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [&,localPlayer](cro::Entity e, float)
    {
        float sizeX = static_cast<float>(cro::App::getWindow().getSize().x);
        sizeX /= m_viewScale.x;

        auto uiPos = localPlayer ? glm::vec2(sizeX / 2.f, UIBarHeight / 2.f) : UIHiddenPosition;
        e.getComponent<cro::Transform>().setPosition(uiPos);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


    //stroke indicator is in model scene...
    cmd.targetFlags = CommandID::StrokeIndicator;
    cmd.action = [localPlayer, player](cro::Entity e, float)
    {
        auto position = player.position;
        position.y = 0.01f;
        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Model>().setHidden(!localPlayer);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //if client is ours activate input/set initial stroke direction
    auto target = m_gameScene.getActiveCamera().getComponent<TargetInfo>().targetLookAt;
    m_inputParser.resetPower();
    m_inputParser.setHoleDirection(target - player.position, m_currentPlayer != player); // this also selects the nearest club

    //if the pause/options menu is open, don't take control
    //from the active input (this will be set when the state is closed)
    if (getStateCount() == 1)
    {
        m_sharedData.inputBinding.controllerID = m_sharedData.controllerIDs[player.player];
    }

    //this just makes sure to update the direction indicator
    //regardless of whether or not we actually switched clubs
    //it's a hack where above case tells the input parser not to update the club (because we're the same player)
    //but we've also landed on th green and therefor auto-switched to a putter
    auto* msg = getContext().appInstance.getMessageBus().post<GolfEvent>(MessageID::GolfMessage);
    msg->type = GolfEvent::ClubChanged;

    //apply the correct sprite to the player entity
    cmd.targetFlags = CommandID::UI::PlayerSprite;
    cmd.action = [&,player](cro::Entity e, float)
    {
        if (player.terrain != TerrainID::Green)
        {
            if (getClub() < ClubID::FiveIron)
            {
                e.getComponent<cro::Sprite>() = m_avatars[m_currentPlayer.client][m_currentPlayer.player].wood;
            }
            else
            {
                e.getComponent<cro::Sprite>() = m_avatars[m_currentPlayer.client][m_currentPlayer.player].iron;
            }
            e.getComponent<cro::Callback>().active = true;

            if (m_avatars[m_currentPlayer.client][m_currentPlayer.player].flipped)
            {
                e.getComponent<cro::Transform>().setScale({ -1.f, 0.f });
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale({ 1.f, 0.f });
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }


            const auto& camera = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
            auto pos = camera.coordsToPixel(player.position, m_gameSceneTexture.getSize());
            e.getComponent<cro::Transform>().setPosition(pos);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //show or hide the slope indicator depending if we're on the green
    cmd.targetFlags = CommandID::SlopeIndicator;
    cmd.action = [player](cro::Entity e, float)
    {
        bool hidden = (player.terrain != TerrainID::Green);

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
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //update player shadow position
    cmd.targetFlags = CommandID::PlayerShadow;
    cmd.action = [&,player](cro::Entity e, float)
    {
        float rotation = m_camRotation;
        if (m_avatars[m_currentPlayer.client][m_currentPlayer.player].flipped)
        {
            rotation += cro::Util::Const::PI;
        }

        e.getComponent<cro::Transform>().setPosition(player.position);
        e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);

        e.getComponent<cro::Model>().setHidden(player.terrain == TerrainID::Green);
        e.getComponent<cro::Callback>().active = true;
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //also check if we need to display mini map for green
    cmd.targetFlags = CommandID::UI::MiniGreen;
    cmd.action = [player](cro::Entity e, float)
    {
        bool hidden = (player.terrain != TerrainID::Green);

        if (!hidden)
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 0;
            e.getComponent<cro::Callback>().active = true;
        }
        else
        {
            e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
            e.getComponent<cro::Callback>().active = true;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    m_currentPlayer = player;
}

void GolfState::hitBall()
{
    auto pitch = Clubs[getClub()].angle;// cro::Util::Const::PI / 4.f;

    auto yaw = m_inputParser.getYaw();

    //add hook/slice to yaw
    yaw += MaxHook * m_inputParser.getHook();

    glm::vec3 impulse(1.f, 0.f, 0.f);
    auto rotation = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yaw, cro::Transform::Y_AXIS);
    rotation = glm::rotate(rotation, pitch, cro::Transform::Z_AXIS);
    impulse = glm::toMat3(rotation) * impulse;

    impulse *= Clubs[getClub()].power * m_inputParser.getPower();
    impulse *= Dampening[m_currentPlayer.terrain];


    InputUpdate update;
    update.clientID = m_sharedData.localConnectionData.connectionID;
    update.playerID = m_currentPlayer.player;
    update.impulse = impulse;

    m_sharedData.clientConnection.netClient.sendPacket(PacketID::InputUpdate, update, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_inputParser.setActive(false);


    //increase the local stroke count so that the UI is updated
    //the server will set the actual value
    m_sharedData.connectionData[m_currentPlayer.client].playerData[m_currentPlayer.player].holeScores[m_currentHole]++;

    //hide the indicator
    cro::Command cmd;
    cmd.targetFlags = CommandID::StrokeIndicator;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Model>().setHidden(true);
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
}

void GolfState::updateActor(const ActorInfo& update)
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Ball;
    cmd.action = [update](cro::Entity e, float)
    {
        if (e.isValid())
        {
            auto& interp = e.getComponent<InterpolationComponent>();
            if (interp.getID() == update.serverID)
            {
                interp.setTarget({ update.position, {1.f,0.f,0.f,0.f}, update.timestamp });
            }
        }
    };
    m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    if (update == m_currentPlayer)
    {
        //this is the active ball so update the UI
        cmd.targetFlags = CommandID::UI::PinDistance;
        cmd.action = [&, update](cro::Entity e, float)
        {
            //if we're on the green convert to cm
            float ballDist = glm::length(update.position - m_holeData[m_currentHole].pin);
            std::int32_t distance = 0;

            if (ballDist > 5)
            {
                distance = static_cast<std::int32_t>(ballDist);
                e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "m");
            }
            else
            {
                distance = static_cast<std::int32_t>(ballDist * 100.f);
                e.getComponent<cro::Text>().setString("Distance: " + std::to_string(distance) + "cm");
            }

            auto bounds = cro::Text::getLocalBounds(e);
            bounds.width = std::floor(bounds.width / 2.f);
            e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

        cmd.targetFlags = CommandID::UI::MiniBall;
        cmd.action =
            [update](cro::Entity e, float)
        {
            auto pos = glm::vec3(update.position.x, -update.position.z, 0.1f);
            e.getComponent<cro::Transform>().setPosition(pos / 2.f); //need to tie into the fact the mini map is 1/2 scale
            
            //set scale based on height
            static constexpr float MaxHeight = 40.f;
            float scale = 1.f + ((update.position.y / MaxHeight) * 2.f);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
}

void GolfState::createTransition(const ActivePlayer& playerData)
{
    //hide player sprite
    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::PlayerSprite;
    cmd.action = [](cro::Entity e, float)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, 0.f));
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    //hide hud
    cmd.targetFlags = CommandID::UI::Root;
    cmd.action = [](cro::Entity e, float) 
    {
        e.getComponent<cro::Transform>().setPosition(UIHiddenPosition);
    };
    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

    auto& targetInfo = m_gameScene.getActiveCamera().getComponent<TargetInfo>();
    if (playerData.terrain == TerrainID::Green)
    {
        targetInfo.targetHeight = CameraPuttHeight;
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
            if (targetDist < 5625) //remember this in len2
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
        auto& targetInfo = m_gameScene.getActiveCamera().getComponent<TargetInfo>();

        auto targetDir = targetInfo.currentLookAt - currPos;
        m_camRotation = std::atan2(-targetDir.z, targetDir.x);

        float minTravel = playerData.terrain == TerrainID::Green ? 0.000001f : 0.005f;
        if (glm::length2(travel) < minTravel)
        {
            //we're there
            targetInfo.prevLookAt = targetInfo.currentLookAt = targetInfo.targetLookAt;
            targetInfo.startHeight = targetInfo.targetHeight;
            targetInfo.startOffset = targetInfo.targetOffset;

            setCameraPosition(playerData.position, targetInfo.targetHeight, targetInfo.targetOffset);
            setCurrentPlayer(playerData);

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

std::int32_t GolfState::getClub() const
{
    switch (m_currentPlayer.terrain)
    {
    default: return m_inputParser.getClub();
    case TerrainID::Bunker: return ClubID::PitchWedge;
    case TerrainID::Green: return ClubID::Putter;
    }
}