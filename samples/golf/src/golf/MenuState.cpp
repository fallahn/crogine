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

#include "MenuState.hpp"
#include "MenuSoundDirector.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "Utility.hpp"
#include "CommandIDs.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "PoissonDisk.hpp"
#include "GolfCartSystem.hpp"
#include "MessageIDs.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/audio/AudioScape.hpp>
#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/String.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <cstring>

namespace
{
#include "TerrainShader.inl"
#include "PostProcess.inl"

    constexpr glm::vec3 CameraBasePosition(-22.f, 4.9f, 22.2f);
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_uiScene           (context.appInstance.getMessageBus(), 512),
    m_backgroundScene   (context.appInstance.getMessageBus()),
    m_playerAvatar      ("assets/golf/images/player.png"),
    m_avatarCallbacks   (std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()),
    m_currentMenu       (MenuID::Main),
    m_prevMenu          (MenuID::Main),
    m_viewScale         (2.f)
{
    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        loadAvatars();

        //add systems to scene
        addSystems();
        //load assets (textures, shaders, models etc)
        loadAssets();
        //create some entities
        createScene();
    });

    context.mainWindow.setMouseCaptured(false);
    
    sd.inputBinding.controllerID = 0;
    sd.baseState = StateID::Menu;

    sd.clientConnection.ready = false;
    std::fill(m_readyState.begin(), m_readyState.end(), false);

    //reset the state if we came from the tutorial (this is
    //also set if the player quit the game from the pause menu)
    if (sd.tutorial)
    {
        sd.serverInstance.stop();
        sd.hosting = false;

        sd.tutorial = false;
        sd.clientConnection.connected = false;
        sd.clientConnection.connectionID = 4;
        sd.clientConnection.ready = false;
        sd.clientConnection.netClient.disconnect();

        sd.mapDirectory = "course_01";
    }

    //we returned from a previous game
    if (sd.clientConnection.connected)
    {
        updateLobbyAvatars();

        //switch to lobby view - send as a command
        //to ensure it's delayed by a frame
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::RootNode;
        cmd.action = [&](cro::Entity e, float)
        {
            m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Lobby;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

        std::int32_t spriteID = 0;
        cro::String connectionString;

        if (m_sharedData.hosting)
        {
            spriteID = SpriteID::StartGame;
            connectionString = "Hosting on: localhost:" + std::to_string(ConstVal::GamePort);

            //auto ready up if host
            m_sharedData.clientConnection.netClient.sendPacket(
                PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                cro::NetFlag::Reliable, ConstVal::NetChannelReliable);


            //set the course selection menu
            addCourseSelectButtons();

            //send a UI refresh to correctly place buttons
            glm::vec2 size(cro::App::getWindow().getSize());
            cmd.targetFlags = CommandID::Menu::UIElement;
            cmd.action =
                [&, size](cro::Entity e, float)
            {
                const auto& element = e.getComponent<UIElement>();
                auto pos = element.absolutePosition;
                pos += element.relativePosition * size / m_viewScale;

                pos.x = std::floor(pos.x);
                pos.y = std::floor(pos.y);

                e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);


            //send the initially selected map/course
            m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
            auto data = serialiseString(m_sharedData.mapDirectory);
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);
        }
        else
        {
            spriteID = SpriteID::ReadyUp;
            connectionString = "Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort);
        }


        cmd.targetFlags = CommandID::Menu::ReadyButton;
        cmd.action = [&, spriteID](cro::Entity e, float)
        {
            e.getComponent<cro::Sprite>() = m_sprites[spriteID];
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

        cmd.targetFlags = CommandID::Menu::ServerInfo;
        cmd.action = [connectionString](cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(connectionString);
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
    else
    {
        //we ought to be resetting previous data here?
        for (auto& cd : m_sharedData.connectionData)
        {
            cd.playerCount = 0;
        }
        m_sharedData.hosting = false;
    }

#ifdef CRO_DEBUG_
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Debug"))
    //        {
    //            /*auto camPos = m_backgroundScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
    //            ImGui::Text("Cam Pos %3.3f, %3.3f, %3.3f", camPos.x, camPos.y, camPos.z);*/

    //            /*static float sunRot = 0.f;
    //            float prevRot = sunRot;
    //            if (ImGui::SliderFloat("Sun", &sunRot, -180.f, 180.f))
    //            {
    //                m_backgroundScene.getSunlight().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, sunRot * cro::Util::Const::degToRad);
    //            }*/

    //            auto group = m_uiScene.getSystem<cro::UISystem>().getActiveGroup();
    //            switch (group)
    //            {
    //            default:
    //                ImGui::Text("No Menu???");
    //                break;
    //            case MenuID::Avatar:
    //                ImGui::Text("Avatar");
    //                break;
    //            case MenuID::Dummy:
    //                ImGui::Text("Dummy");
    //                break;
    //            case MenuID::Join:
    //                ImGui::Text("Join");
    //                break;
    //            case MenuID::Lobby:
    //                ImGui::Text("Lobby");
    //                break;
    //            case MenuID::Main:
    //                ImGui::Text("Main");
    //                break;
    //            }

    //            if (group != m_currentMenu)
    //            {
    //                ImGui::Text("Group is %lu but menu is %lu", group, m_currentMenu);
    //            }
    //        }
    //        ImGui::End();
    //    
    //    });
#endif
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
#ifdef CRO_DEBUG_
        case SDLK_F2:
            showPlayerConfig(true, 0);
            break;
        case SDLK_F3:
            showPlayerConfig(false, 0);
            break;
        case SDLK_F4:
            requestStackClear();
            requestStackPush(StateID::SplashScreen);
            break;
        case SDLK_o:
            cro::GameController::rumbleStart(0, 1, 1000);
            LogI << cro::GameController::getName(0) << std::endl;
            break;
        case SDLK_p:
            cro::GameController::rumbleStart(1, 1, 1000);
            LogI << cro::GameController::getName(1) << std::endl;
            break;
#endif
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
            if (m_textEdit.string)
            {
                applyTextEdit();
            }
            break;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        handleTextEdit(evt);
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEdit(evt);
    }
    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        //controller IDs are automatically reassigned
        //so we just need to make sure no one is out of range
        for (auto& c : m_sharedData.controllerIDs)
        {
            //be careful with this cast because we might assign - 1 as an ID...
            //note that the controller count hasn't been updated yet...
            c = std::min(static_cast<std::int32_t>(cro::GameController::getControllerCount() - 2), c);
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
            //leave the current menu when B is pressed.
        case cro::GameController::ButtonB:
            switch (m_currentMenu)
            {
            default: break;
            case MenuID::Avatar:
                m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
                m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
                break;
            case MenuID::Join:
                applyTextEdit();
                m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
                break;
            case MenuID::Lobby:
                quitLobby();
                break;
            case MenuID::PlayerConfig:
                applyTextEdit();
                showPlayerConfig(false, m_playerAvatar.activePlayer);
                updateLocalAvatars(m_avatarCallbacks.first, m_avatarCallbacks.second);
                break;
            }
            break;
        }
    }

    m_uiScene.getSystem<cro::UISystem>().handleEvent(evt);

    m_uiScene.forwardEvent(evt);
    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        switch (data.type)
        {
        default: break;
        case SystemEvent::PostProcessToggled:
            m_sharedData.usePostProcess = !m_sharedData.usePostProcess;

            if (m_sharedData.usePostProcess)
            {
                auto windowSize = cro::App::getWindow().getSize();

                m_postBuffer.create(windowSize.x, windowSize.y, false);
                m_postQuad.setTexture(m_postBuffer.getTexture());
                auto shaderRes = glm::vec2(windowSize/* / 2u*/);
                glCheck(glUseProgram(m_postShader.getGLHandle()));
                glCheck(glUniform2f(m_postShader.getUniformID("u_resolution"), shaderRes.x, shaderRes.y));
            }

            break;
        }
    }

    m_backgroundScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            //handle events
            handleNetEvent(evt);
        }
    }

    m_backgroundScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void MenuState::render()
{
    //draw any renderable systems
    m_backgroundTexture.clear();
    m_backgroundScene.render(m_backgroundTexture);
    m_backgroundTexture.display();

    if (m_sharedData.usePostProcess)
    {
        m_postBuffer.clear();
        m_uiScene.render(m_postBuffer);
        m_postBuffer.display();

        m_postQuad.draw();
    }
    else
    {
        m_uiScene.render(cro::App::getWindow());
    }
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_backgroundScene.addSystem<GolfCartSystem>(mb);
    m_backgroundScene.addSystem<cro::CallbackSystem>(mb);
    m_backgroundScene.addSystem<cro::BillboardSystem>(mb);
    m_backgroundScene.addSystem<cro::CameraSystem>(mb);
    m_backgroundScene.addSystem<cro::ModelRenderer>(mb);
    m_backgroundScene.addSystem<cro::AudioSystem>(mb);

    m_backgroundScene.addDirector<MenuSoundDirector>(m_resources.audio, m_currentMenu);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void MenuState::loadAssets()
{
    m_postShader.loadFromString(PostVertex, PostFragment);
    m_postQuad.setShader(m_postShader);

    m_backgroundScene.setCubemap("assets/golf/images/skybox/spring/sky.ccm");

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n");
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define TEXTURED\n");

    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(m_resources.shaders.get(ShaderID::Cel));
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(m_resources.shaders.get(ShaderID::CelTextured));

    //load the billboard rects from a sprite sheet and convert to templates
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", m_resources.textures);
    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Pine] = spriteToBillboard(spriteSheet.getSprite("pine"));
    m_billboardTemplates[BillboardID::Willow] = spriteToBillboard(spriteSheet.getSprite("willow"));
    m_billboardTemplates[BillboardID::Birch] = spriteToBillboard(spriteSheet.getSprite("birch"));
}

void MenuState::createScene()
{
    cro::AudioMixer::setPrefadeVolume(0.f, MixerChannel::Music);
    cro::AudioMixer::setPrefadeVolume(0.f, MixerChannel::Effects);

    //fade in entity
    auto audioEnt = m_backgroundScene.createEntity();
    audioEnt.addComponent<cro::Callback>().active = true;
    audioEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
    audioEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        static constexpr float MaxTime = 2.f;

        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(MaxTime, currTime + dt);
        
        float progress = currTime / MaxTime;
        cro::AudioMixer::setPrefadeVolume(progress, MixerChannel::Music);
        cro::AudioMixer::setPrefadeVolume(progress, MixerChannel::Effects);
        
        if(currTime == MaxTime)
        {
            e.getComponent<cro::Callback>().active = false;
            m_backgroundScene.destroyEntity(e);
        }
    };


    auto texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);

    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/golf/models/menu_pavilion.cmt");
    setTexture(md, texturedMat);

    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

    md.loadFromFile("assets/golf/models/menu_ground.cmt");
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);
    texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    setTexture(md, texturedMat);
    entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

    md.loadFromFile("assets/golf/models/phone_box.cmt");
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 8.2f, 0.f, 13.8f });
    md.createModel(entity);

    texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    setTexture(md, texturedMat);
    entity.getComponent<cro::Model>().setMaterial(0, texturedMat);


    //billboards
    md.loadFromFile("assets/golf/models/shrubbery.cmt");
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);

    if (entity.hasComponent<cro::BillboardCollection>())
    {
        std::array minBounds = { 30.f, 0.f };
        std::array maxBounds = { 80.f, 10.f };

        auto& collection = entity.getComponent<cro::BillboardCollection>();

        auto trees = pd::PoissonDiskSampling(4.8f, minBounds, maxBounds);
        for (auto [x, y] : trees)
        {
            float scale = static_cast<float>(cro::Util::Random::value(8, 12)) / 10.f;

            auto bb = m_billboardTemplates[cro::Util::Random::value(BillboardID::Pine, BillboardID::Willow)];
            bb.position = { x, 0.f, -y };
            bb.size *= scale;
            collection.addBillboard(bb);
        }

        //repeat for grass
        minBounds = { 10.5f, 12.f };
        maxBounds = { 26.f, 30.f };

        auto grass = pd::PoissonDiskSampling(1.f, minBounds, maxBounds);
        for (auto [x, y] : grass)
        {
            float scale = static_cast<float>(cro::Util::Random::value(8, 11)) / 10.f;

            auto bb = m_billboardTemplates[cro::Util::Random::value(BillboardID::Grass01, BillboardID::Flowers03)];
            bb.position = { -x, 0.1f, y };
            bb.size *= scale;
            collection.addBillboard(bb);
        }
    }

    cro::AudioScape as;
    as.loadFromFile("assets/golf/sound/menu.xas", m_resources.audio);

    //golf carts
    md.loadFromFile("assets/golf/models/cart.cmt");
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.f, 0.01f, 1.8f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 87.f * cro::Util::Const::degToRad);
    md.createModel(entity);

    texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
    setTexture(md, texturedMat);
    entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

    //these ones move :)
    for (auto i = 0u; i < 2u; ++i)
    {
        entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(-10000.f));
        entity.addComponent<GolfCart>();
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

        entity.addComponent<cro::AudioEmitter>() = as.getEmitter("cart");
        entity.getComponent<cro::AudioEmitter>().play();
    }

    //music
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("music");
    entity.getComponent<cro::AudioEmitter>().play();

    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();
        m_backgroundTexture.create(static_cast<std::uint32_t>(vpSize.x), static_cast<std::uint32_t>(vpSize.y));

        //the resize actually extends the target vertically so we need to maintain a
        //horizontal FOV, not the vertical one expected by default.
        cam.setPerspective(FOV * (vpSize.y / ViewportHeight), vpSize.x / vpSize.y, 0.1f, vpSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_backgroundScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    camEnt.getComponent<cro::Transform>().setPosition(CameraBasePosition);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -34.f * cro::Util::Const::degToRad);
    //camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, -0.84f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -8.f * cro::Util::Const::degToRad);

    //add the ambience to the cam cos why not
    camEnt.addComponent<cro::AudioEmitter>() = as.getEmitter("01");
    camEnt.getComponent<cro::AudioEmitter>().play();

    auto sunEnt = m_backgroundScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, /*-0.967f*/-40.56f * cro::Util::Const::degToRad);
    //sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -80.7f * cro::Util::Const::degToRad);

    createUI();
}

void MenuState::handleNetEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Game)
            {
                requestStackClear();
                requestStackPush(StateID::Game);
            }
            break;
        case PacketID::ConnectionAccepted:
            {
                //update local player data
                m_sharedData.clientConnection.connectionID = evt.packet.as<std::uint8_t>();
                m_sharedData.localConnectionData.connectionID = evt.packet.as<std::uint8_t>();
                m_sharedData.connectionData[m_sharedData.clientConnection.connectionID] = m_sharedData.localConnectionData;

                //send player details to server (name, skin)
                auto buffer = m_sharedData.localConnectionData.serialise();
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::PlayerInfo, buffer.data(), buffer.size(), cro::NetFlag::Reliable, ConstVal::NetChannelStrings);

                //switch to lobby view
                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::RootNode;
                cmd.action = [&](cro::Entity e, float)
                {
                    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Dummy);
                    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Lobby;
                    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
                };
                m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                if (m_sharedData.serverInstance.running())
                {
                    //auto ready up if hosting
                    m_sharedData.clientConnection.netClient.sendPacket(
                        PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                        cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }

                LOG("Successfully connected to server", cro::Logger::Type::Info);
            }
            break;
        case PacketID::ConnectionRefused:
        {
            std::string err = "Connection refused: ";
            switch (evt.packet.as<std::uint8_t>())
            {
            default:
                err += "Unknown Error";
                break;
            case MessageType::ServerFull:
                err += "Server full";
                break;
            case MessageType::NotInLobby:
                err += "Game in progress";
                break;
            case MessageType::BadData:
                err += "Bad Data Received";
                break;
            }
            LogE << err << std::endl;
            m_sharedData.errorMessage = err;
            requestStackPush(StateID::Error);

            m_sharedData.clientConnection.netClient.disconnect();
            m_sharedData.clientConnection.connected = false;
        }
            break;
        case PacketID::LobbyUpdate:
            updateLobbyData(evt);
            break;
        case PacketID::ClientDisconnected:
        {
            auto client = evt.packet.as<std::uint8_t>();
            m_sharedData.connectionData[client].playerCount = 0;
            m_readyState[client] = false;
            updateLobbyAvatars();
        }
            break;
        case PacketID::LobbyReady:
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            m_readyState[((data & 0xff00) >> 8)] = (data & 0x00ff) ? true : false;
        }
            break;
        case PacketID::MapInfo:
        {
            //check we have the local data (or at least something with the same name)
            auto course = deserialiseString(evt.packet);

            //jump straight into the tutorial
            //if that's what's set
            if (course == "tutorial")
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            else
            {
                if (auto data = std::find_if(m_courseData.cbegin(), m_courseData.cend(),
                    [&course](const CourseData& cd)
                    {
                        return cd.directory == course;
                    }); data != m_courseData.cend())
                {
                    m_sharedData.mapDirectory = course;

                    //update UI
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::CourseTitle;
                    cmd.action = [data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(data->title);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseDesc;
                    cmd.action = [data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                        e.getComponent<cro::Text>().setString(data->description);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseHoles;
                    cmd.action = [data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(data->holeCount);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                }
                else
                {
                    //print to UI course is missing
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::CourseTitle;
                    cmd.action = [course](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(course);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseDesc;
                    cmd.action = [course](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                        e.getComponent<cro::Text>().setString("Course Data Not Found");
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseHoles;
                    cmd.action = [data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    };
                    m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                }
            }
        }
            break;
        break;
        }
    }
    else if (evt.type == cro::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Lost Connection To Host";
        requestStackPush(StateID::Error);
    }
}

void MenuState::beginTextEdit(cro::Entity stringEnt, cro::String* dst, std::size_t maxChars)
{
    stringEnt.getComponent<cro::Text>().setFillColour(TextEditColour);
    m_textEdit.string = dst;
    m_textEdit.entity = stringEnt;
    m_textEdit.maxLen = maxChars;

    //block input to menu
    m_prevMenu = m_currentMenu;
    m_currentMenu = MenuID::Dummy;
    m_uiScene.getSystem<cro::UISystem>().setActiveGroup(m_currentMenu);

    SDL_StartTextInput();
}

void MenuState::handleTextEdit(const cro::Event& evt)
{
    if (!m_textEdit.string)
    {
        return;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            if (!m_textEdit.string->empty())
            {
                m_textEdit.string->erase(m_textEdit.string->size() - 1);
            }
            break;
        //case SDLK_RETURN:
        //case SDLK_RETURN2:
            //applyTextEdit();
            //return;
        }
        
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        if (m_textEdit.string->size() < ConstVal::MaxStringChars
            && m_textEdit.string->size() < m_textEdit.maxLen)
        {
            auto codePoints = cro::Util::String::getCodepoints(evt.text.text);

            *m_textEdit.string += cro::String::fromUtf32(codePoints.begin(), codePoints.end());
        }
    }

    //update string origin
    //if (!m_textEdit.string->empty())
    //{
    //    //auto bounds = cro::Text::getLocalBounds(m_textEdit.entity);
    //    //m_textEdit.entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
    //    //TODO make this scroll when we hit the edge of the input
    //}
}

void MenuState::applyTextEdit()
{
    if (m_textEdit.string && m_textEdit.entity.isValid())
    {
        if (m_textEdit.string->empty())
        {
            *m_textEdit.string = "INVALID";
        }

        m_textEdit.entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        m_textEdit.entity.getComponent<cro::Text>().setString(*m_textEdit.string);
        m_textEdit.entity.getComponent<cro::Callback>().active = false;

        //send this as a command to delay it by a frame - doesn't matter who receives it :)
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::RootNode;
        cmd.action = [&](cro::Entity, float)
        {
            //commandception
            cro::Command cmd2;
            cmd2.targetFlags = CommandID::Menu::RootNode;
            cmd2.action = [&](cro::Entity, float)
            {
                m_currentMenu = m_prevMenu;
                m_uiScene.getSystem<cro::UISystem>().setActiveGroup(m_currentMenu);
            };
            m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd2);
        };
        m_uiScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
        SDL_StopTextInput();
    }
    m_textEdit = {};
}