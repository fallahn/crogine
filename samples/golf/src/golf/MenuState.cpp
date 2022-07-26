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

#include "MenuState.hpp"
#include "MenuSoundDirector.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "Utility.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "MenuCallbacks.hpp"
#include "PoissonDisk.hpp"
#include "GolfCartSystem.hpp"
#include "CloudSystem.hpp"
#include "MessageIDs.hpp"
#include "spooky2.hpp"
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

#include <crogine/ecs/InfoFlags.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem3D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <cstring>

namespace
{
#include "TerrainShader.inl"
#include "BillboardShader.inl"
#include "CloudShader.inl"

    //constexpr glm::vec3 CameraBasePosition(-22.f, 4.9f, 22.2f);
}

MainMenuContext::MainMenuContext(MenuState* state)
{
    uiScene = &state->m_uiScene;
    currentMenu = &state->m_currentMenu;
    menuEntities = state->m_menuEntities.data();

    menuPositions = state->m_menuPositions.data();
    viewScale = &state->m_viewScale;
    sharedData = &state->m_sharedData;
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_matchMaking       (context.appInstance.getMessageBus()),
    m_cursor            (/*"assets/images/cursor.png", 0, 0*/cro::SystemCursor::Hand),
    m_uiScene           (context.appInstance.getMessageBus(), 512),
    m_backgroundScene   (context.appInstance.getMessageBus()/*, 128, cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_avatarScene       (context.appInstance.getMessageBus()/*, 128, cro::INFO_FLAG_SYSTEMS_ACTIVE*/),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_windBuffer        ("WindValues"),
    m_avatarCallbacks   (std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()),
    m_currentMenu       (MenuID::Main),
    m_prevMenu          (MenuID::Main),
    m_viewScale         (2.f),
    m_activePlayerAvatar(0)
{
    std::fill(m_readyState.begin(), m_readyState.end(), false);
    std::fill(m_ballIndices.begin(), m_ballIndices.end(), 0);
    std::fill(m_avatarIndices.begin(), m_avatarIndices.end(), 0);    
    std::fill(m_hairIndices.begin(), m_hairIndices.end(), 0);    

    Achievements::setActive(true);
    
    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        //loadAvatars();

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
    sd.mapDirectory = "course_01";

    sd.clientConnection.ready = false;

    //remap the selected ball model indices - this is always applied
    //as the avatar IDs are loaded from the config, above
    for (auto i = 0u; i < ConnectionData::MaxPlayers; ++i)
    {
        auto idx = indexFromBallID(m_sharedData.localConnectionData.playerData[i].ballID);

        if (idx > -1)
        {
            m_ballIndices[i] = idx;
        }
        else
        {
            m_sharedData.localConnectionData.playerData[i].ballID = 0;
        }
        applyAvatarColours(i);
    }

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
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Lobby;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        std::int32_t spriteID = 0;
        cro::String connectionString;

        if (m_sharedData.hosting)
        {
            spriteID = SpriteID::StartGame;
            connectionString = "Hosting on: localhost:" + std::to_string(ConstVal::GamePort);

            //auto ready up if host
            m_sharedData.clientConnection.netClient.sendPacket(
                PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                net::NetFlag::Reliable, ConstVal::NetChannelReliable);


            //set the course selection menu
            addCourseSelectButtons();

            //send a UI refresh to correctly place buttons
            glm::vec2 size(GolfGame::getActiveTarget()->getSize());
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
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


            //send the initially selected map/course
            m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
            auto data = serialiseString(m_sharedData.mapDirectory);
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);
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
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cmd.targetFlags = CommandID::Menu::ServerInfo;
        cmd.action = [connectionString](cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(connectionString);
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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

    //for some reason this immediately unsets itself
    //cro::App::getWindow().setCursor(&m_cursor);

    registerCommand("tree_ed", [&](const std::string&) 
        {
            if (getStateCount() == 1)
            {
                requestStackPush(StateID::Bush);
            }
        });

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("Debug"))
            {
                /*ImGui::Image(m_sharedData.nameTextures[0].getTexture(), { 128, 64 }, { 0,1 }, { 1,0 });
                ImGui::SameLine();
                ImGui::Image(m_sharedData.nameTextures[1].getTexture(), { 128, 64 }, { 0,1 }, { 1,0 });
                ImGui::Image(m_sharedData.nameTextures[2].getTexture(), { 128, 64 }, { 0,1 }, { 1,0 });
                ImGui::SameLine();
                ImGui::Image(m_sharedData.nameTextures[3].getTexture(), { 128, 64 }, { 0,1 }, { 1,0 });*/
                /*float x = static_cast<float>(AvatarThumbSize.x);
                float y = static_cast<float>(AvatarThumbSize.y);
                ImGui::Image(m_avatarThumbs[0].getTexture(), {x,y}, {0,1}, {1,0});
                ImGui::SameLine();
                ImGui::Image(m_avatarThumbs[1].getTexture(), { x,y }, { 0,1 }, { 1,0 });
                ImGui::SameLine();
                ImGui::Image(m_avatarThumbs[2].getTexture(), { x,y }, { 0,1 }, { 1,0 });
                ImGui::SameLine();
                ImGui::Image(m_avatarThumbs[3].getTexture(), { x,y }, { 0,1 }, { 1,0 });*/
                //auto pos = m_avatarScene.getActiveCamera().getComponent<cro::Transform>().getPosition();
                //ImGui::Text("%3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);

                if (ImGui::InputInt("Tree Quality", &m_sharedData.treeQuality))
                {
                    m_sharedData.treeQuality = std::max(0, std::min(2, m_sharedData.treeQuality));
                }
            }
            ImGui::End();
        });
#endif
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    const auto quitMenu = 
        [&]()
    {
        switch (m_currentMenu)
        {
        default: break;
        case MenuID::Avatar:
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
            break;
        case MenuID::Join:
            applyTextEdit();
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
            break;
        case MenuID::Lobby:
            enterConfirmCallback();
            break;
        case MenuID::PlayerConfig:
            applyTextEdit();
            showPlayerConfig(false, m_activePlayerAvatar);
            updateLocalAvatars(m_avatarCallbacks.first, m_avatarCallbacks.second);
            break;
        case MenuID::ConfirmQuit:
            quitConfirmCallback();
            break;
        }
    };

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
            requestStackPush(StateID::PuttingRange);
            break;
        case SDLK_F9:
            requestStackPush(StateID::Bush);
            break;
        case SDLK_KP_0:
            requestStackPush(StateID::News);
            //cro::GameController::rumbleStart(0, 65000, 65000, 1000);
            //LogI << cro::GameController::getName(0) << std::endl;
            break;
        case SDLK_KP_1:
            //cro::GameController::rumbleStart(1, 65000, 65000, 1000);
            //LogI << cro::GameController::getName(1) << std::endl;
            break;
        case SDLK_KP_2:
            //cro::GameController::rumbleStart(2, 65000, 65000, 1000);
            //LogI << cro::GameController::getName(2) << std::endl;
            break;
        case SDLK_KP_9:
        {
            requestStackPush(StateID::Trophy);
        }
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
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            quitMenu();
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
            quitMenu();
            break;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitMenu();
        }
        else if (evt.button.button == SDL_BUTTON_LEFT)
        {
            if (applyTextEdit())
            {
                //we applied a text edit so don't update the
                //UISystem
                return true;
            }
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_uiScene.forwardEvent(evt);
    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && data.id == StateID::Keyboard)
        {
            applyTextEdit();
        }
    }
    else if (msg.id == MatchMaking::MessageID)
    {
        const auto& data = msg.getData<MatchMaking::Message>();
        switch (data.type)
        {
        default:
        case MatchMaking::Message::Error:
            break;
        case MatchMaking::Message::GameCreated:
            finaliseGameCreate();
        break;
        case MatchMaking::Message::LobbyJoined:
            finaliseGameJoin();
            break;
        }
    }

    m_backgroundScene.forwardMessage(msg);
    m_avatarScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
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
    }

    static float accumTime = 0.f;
    accumTime += dt;

    WindData wind;
    wind.direction[0] = 0.5f;
    wind.direction[1] = 0.5f;
    wind.direction[2] = 0.5f;
    wind.elapsedTime = accumTime;
    m_windBuffer.setData(wind);

    m_backgroundScene.simulate(dt);
    //processing these with options open only slows things down.
#ifdef CRO_DEBUG_
    if (getStateCount() == 1)
#endif
    {
        m_avatarScene.simulate(dt);
        m_uiScene.simulate(dt);
    }

    return true;
}

void MenuState::render()
{
    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_windBuffer.bind(2);

    //render ball preview first
    auto oldCam = m_backgroundScene.setActiveCamera(m_ballCam);
    m_ballTexture.clear(cro::Colour::Magenta);
    m_backgroundScene.render();
    m_ballTexture.display();

    //and avatar preview
    m_avatarTexture.clear(cro::Colour::Transparent);
    //m_avatarTexture.clear(cro::Colour::Magenta);
    m_avatarScene.render();
    m_avatarTexture.display();

    //then background scene
    m_backgroundScene.setActiveCamera(oldCam);
    m_backgroundTexture.clear();
    m_backgroundScene.render();
    m_backgroundTexture.display();

    m_uiScene.render();
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_backgroundScene.addSystem<GolfCartSystem>(mb);
    m_backgroundScene.addSystem<CloudSystem>(mb)->setWindVector(glm::vec3(0.25f));
    m_backgroundScene.addSystem<cro::CallbackSystem>(mb);
    m_backgroundScene.addSystem<cro::SkeletalAnimator>(mb);
    m_backgroundScene.addSystem<cro::SpriteSystem3D>(mb);
    m_backgroundScene.addSystem<cro::BillboardSystem>(mb);
    m_backgroundScene.addSystem<cro::CameraSystem>(mb);
    m_backgroundScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_backgroundScene.addSystem<cro::ModelRenderer>(mb);
    m_backgroundScene.addSystem<cro::ParticleSystem>(mb);
    m_backgroundScene.addSystem<cro::AudioSystem>(mb);

    m_backgroundScene.addDirector<MenuSoundDirector>(m_resources.audio, m_currentMenu);

    m_avatarScene.addSystem<cro::CallbackSystem>(mb);
    m_avatarScene.addSystem<cro::SkeletalAnimator>(mb);
    m_avatarScene.addSystem<cro::CameraSystem>(mb);
    m_avatarScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void MenuState::loadAssets()
{
    m_backgroundScene.setCubemap("assets/golf/images/skybox/spring/sky.ccm");

    std::string wobble;
    if (m_sharedData.vertexSnap)
    {
        wobble = "#define WOBBLE\n";
    }

    m_resources.shaders.loadFromString(ShaderID::Cel, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n" + wobble);
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define TEXTURED\n" + wobble);
    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n" + wobble);
    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define SUBRECT\n#define TEXTURED\n#define SKINNED\n#define NOCHEX\n");
    m_resources.shaders.loadFromString(ShaderID::Hair, CelVertexShader, CelFragmentShader, "#define USER_COLOUR\n#define NOCHEX");
    m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);

    auto* shader = &m_resources.shaders.get(ShaderID::Cel);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTextured] = m_resources.materials.add(*shader);

    shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ground] = m_resources.materials.add(*shader);

    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelTexturedSkinned] = m_resources.materials.add(*shader);

    shader = &m_resources.shaders.get(ShaderID::Hair);
    m_materialIDs[MaterialID::Hair] = m_resources.materials.add(*shader);
    m_resolutionBuffer.addShader(*shader);
    //fudge this for the previews
    m_resources.materials.get(m_materialIDs[MaterialID::Hair]).doubleSided = true;

    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    //load the billboard rects from a sprite sheet and convert to templates
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", m_resources.textures);
    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
    m_billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
    m_billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    for (auto& thumb : m_avatarThumbs)
    {
        thumb.create(AvatarThumbSize.x, AvatarThumbSize.y);
    }
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
    if (md.loadFromFile("assets/golf/models/menu_pavilion.cmt"))
    {
        applyMaterialData(md, texturedMat);

        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
    }

    if (md.loadFromFile("assets/golf/models/menu_ground.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::Ground]);
        applyMaterialData(md, texturedMat);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
    }

    if (md.loadFromFile("assets/golf/models/phone_box.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 8.2f, 0.f, 13.2f });
        entity.getComponent<cro::Transform>().setScale(glm::vec3(0.9f));
        md.createModel(entity);

        texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        applyMaterialData(md, texturedMat);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
    }

    if (md.loadFromFile("assets/golf/models/garden_bench.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 12.2f, 0.f, 13.6f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        applyMaterialData(md, texturedMat);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
    }

    if (md.loadFromFile("assets/golf/models/sign_post.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -10.f, 0.f, 12.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -150.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
        applyMaterialData(md, texturedMat);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
    }

    if (md.loadFromFile("assets/golf/models/bollard_day.cmt"))
    {
        constexpr std::array positions =
        {
            glm::vec3(7.2f, 0.f, 12.f),
            glm::vec3(7.2f, 0.f, 3.5f),
            //glm::vec3(-10.5f, 0.f, 12.5f),
            glm::vec3(-8.2f, 0.f, 3.5f)
        };

        for (auto pos : positions)
        {
            auto entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(pos);
            md.createModel(entity);

            texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
            applyMaterialData(md, texturedMat);
            entity.getComponent<cro::Model>().setMaterial(0, texturedMat);
        }
    }
    /*cro::EmitterSettings sprinkler;
    if (sprinkler.loadFromFile("assets/golf/particles/sprinkler.cps", m_resources.textures))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -11.f, 0.f, 13.8f });
        entity.addComponent<cro::ParticleEmitter>().settings = sprinkler;
        entity.getComponent<cro::ParticleEmitter>().start();
    }*/

    //billboards
    if (md.loadFromFile("assets/golf/models/shrubbery.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        auto billboardMat = m_resources.materials.get(m_materialIDs[MaterialID::Billboard]);
        applyMaterialData(md, billboardMat);
        entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

        if (entity.hasComponent<cro::BillboardCollection>())
        {
            std::array minBounds = { 30.f, 0.f };
            std::array maxBounds = { 80.f, 10.f };

            auto& collection = entity.getComponent<cro::BillboardCollection>();

            auto trees = pd::PoissonDiskSampling(2.8f, minBounds, maxBounds);
            for (auto [x, y] : trees)
            {
                float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;

                auto bb = m_billboardTemplates[cro::Util::Random::value(BillboardID::Tree01, BillboardID::Tree04)];
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
    }


    //golf carts
    if (md.loadFromFile("assets/golf/models/menu/cart.cmt"))
    {
        const std::array<std::string, 6u> passengerStrings =
        {
            "assets/golf/models/menu/driver01.cmt",
            "assets/golf/models/menu/passenger01.cmt",
            "assets/golf/models/menu/passenger02.cmt",
            "assets/golf/models/menu/driver02.cmt",
            "assets/golf/models/menu/passenger03.cmt",
            "assets/golf/models/menu/passenger04.cmt"
        };

        std::array<cro::Entity, 6u> passengers = {};

        cro::ModelDefinition passengerDef(m_resources);
        for (auto i = 0u; i < passengerStrings.size(); ++i)
        {
            if (passengerDef.loadFromFile(passengerStrings[i]))
            {
                passengers[i] = m_backgroundScene.createEntity();
                passengers[i].addComponent<cro::Transform>();
                passengerDef.createModel(passengers[i]);

                cro::Material::Data material;
                if (passengers[i].hasComponent<cro::Skeleton>())
                {
                    material = m_resources.materials.get(m_materialIDs[MaterialID::CelTexturedSkinned]);
#ifndef CRO_DEBUG_
                    passengers[i].getComponent<cro::Skeleton>().play(0);
#endif
                }
                else
                {
                    material = m_resources.materials.get(m_materialIDs[MaterialID::CelTextured]);
                }
                applyMaterialData(passengerDef, material);
                passengers[i].getComponent<cro::Model>().setMaterial(0, material);
            }
        }



        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 5.f, 0.01f, 1.8f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 87.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        texturedMat = m_resources.materials.get(m_materialIDs[MaterialID::Ground]);
        applyMaterialData(md, texturedMat);
        entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

        //these ones move :)
        for (auto i = 0u; i < 2u; ++i)
        {
            entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(-10000.f));
            entity.addComponent<GolfCart>();
            md.createModel(entity);
            entity.getComponent<cro::Model>().setMaterial(0, texturedMat);

            entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("cart");
            entity.getComponent<cro::AudioEmitter>().play();

            for (auto j = 0; j < 3; ++j)
            {
                auto index = (i * 3) + j;
                if (passengers[index].isValid())
                {
                    entity.getComponent<cro::Transform>().addChild(passengers[index].getComponent<cro::Transform>());
                }
            }
        }
    }

    createClouds();

    //music
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("music");
    entity.getComponent<cro::AudioEmitter>().play();
    entity.getComponent<cro::AudioEmitter>().setLooped(true);

    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto vpSize = calcVPSize();

        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = std::floor(winSize.y / vpSize.y);
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;

        auto invScale = (maxScale + 1) - scale;
        m_scaleBuffer.setData(invScale);

        ResolutionData d;
        d.resolution = texSize / invScale;
        m_resolutionBuffer.setData(d);

        m_backgroundTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y));

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, vpSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_backgroundScene.getActiveCamera();
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    cam.shadowMapBuffer.create(2048, 2048);
    updateView(cam);

    //camEnt.getComponent<cro::Transform>().setPosition({ -17.8273, 4.9, 25.0144 });
    camEnt.getComponent<cro::Transform>().setPosition({ -18.3f, 5.2f, 23.3144f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -31.f * cro::Util::Const::degToRad);
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -8.f * cro::Util::Const::degToRad);

    //add the ambience to the cam cos why not
    camEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("01");
    camEnt.getComponent<cro::AudioEmitter>().play();

    auto sunEnt = m_backgroundScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -40.56f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -39.f * cro::Util::Const::degToRad);

    //set up cam / models for ball preview
    createBallScene();    

    createUI();
}

void MenuState::createClouds()
{
    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/clouds.spt", m_resources.textures)
        && spriteSheet.getSprites().size() > 1)
    {
        const auto& sprites = spriteSheet.getSprites();
        std::vector<cro::Sprite> randSprites;
        for (auto [_, sprite] : sprites)
        {
            randSprites.push_back(sprite);
        }

        m_resources.shaders.loadFromString(ShaderID::Cloud, CloudVertex, CloudFragment, "#define MAX_RADIUS 86\n");
        auto& shader = m_resources.shaders.get(ShaderID::Cloud);
        m_scaleBuffer.addShader(shader);

        auto matID = m_resources.materials.add(shader);
        auto material = m_resources.materials.get(matID);
        material.blendMode = cro::Material::BlendMode::Alpha;
        material.setProperty("u_texture", *spriteSheet.getTexture());

        auto seed = static_cast<std::uint32_t>(std::time(nullptr));
        static constexpr std::array MinBounds = { 0.f, 0.f };
        static constexpr std::array MaxBounds = { 280.f, 280.f };
        auto positions = pd::PoissonDiskSampling(40.f, MinBounds, MaxBounds, 30u, seed);

        auto Offset = 140.f;

        std::vector<cro::Entity> delayedUpdates;

        for (const auto& position : positions)
        {
            float height = static_cast<float>(cro::Util::Random::value(20, 40));
            glm::vec3 cloudPos(position[0] - Offset, height, -position[1] + Offset);


            auto entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(cloudPos);
            entity.addComponent<Cloud>().speedMultiplier = static_cast<float>(cro::Util::Random::value(10, 22)) / 100.f;
            entity.addComponent<cro::Sprite>() = randSprites[cro::Util::Random::value(0u, randSprites.size() - 1)];
            entity.addComponent<cro::Model>();

            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            bounds.width /= PixelPerMetre;
            bounds.height /= PixelPerMetre;
            entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });

            float scale = static_cast<float>(cro::Util::Random::value(4, 10)) / 100.f;
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));
            entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);

            delayedUpdates.push_back(entity);
        }

        //this is a work around because changing sprite 3D materials
        //require at least once scene update to be run first.
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, material, delayedUpdates](cro::Entity e, float)
        {
            for (auto en : delayedUpdates)
            {
                en.getComponent<cro::Model>().setMaterial(0, material);
            }

            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);
        };
    }
}

void MenuState::handleNetEvent(const net::NetEvent& evt)
{
    if (evt.type == net::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::PingTime:
        {
            auto data = evt.packet.as<std::uint32_t>();
            auto pingTime = data & 0xffff;
            auto client = (data & 0xffff0000) >> 16;

            m_sharedData.connectionData[client].pingTime = pingTime;
        }
        break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Golf)
            {
                requestStackClear();
                requestStackPush(StateID::Golf);
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
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::PlayerInfo, buffer.data(), buffer.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                if (m_sharedData.tutorial)
                {
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }
                else
                {
                    //switch to lobby view
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::RootNode;
                    cmd.action = [&](cro::Entity e, float)
                    {
                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Lobby;
                        m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
                }

                if (m_sharedData.serverInstance.running())
                {
                    //auto ready up if hosting
                    m_sharedData.clientConnection.netClient.sendPacket(
                        PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                        net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
            case MessageType::VersionMismatch:
                err += "Client/Server Mismatch";
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
                //moved to connection accepted - must happen after sending player info
                //m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
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
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseDesc;
                    cmd.action = [data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                        e.getComponent<cro::Text>().setString(data->description);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseHoles;
                    cmd.action = [&,data](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(data->holeCount[m_sharedData.holeCount]);
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseDesc;
                    cmd.action = [course](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setFillColour(TextHighlightColour);
                        e.getComponent<cro::Text>().setString("Course Data Not Found");
                        centreText(e);
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    cmd.targetFlags = CommandID::Menu::CourseHoles;
                    cmd.action = [](cro::Entity e, float)
                    {
                        e.getComponent<cro::Text>().setString(" ");
                    };
                    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    //TODO should we un-ready the client to prevent the host launching
                    //if we don't have this course?
                }
            }
        }
            break;
        case PacketID::ScoreType:
            m_sharedData.scoreType = evt.packet.as<std::uint8_t>();
            {
                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::ScoreType;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(ScoreTypes[m_sharedData.scoreType]);
                    centreText(e);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                cmd.targetFlags = CommandID::Menu::ScoreDesc;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(ScoreDesc[m_sharedData.scoreType]);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
            break;
        case PacketID::GimmeRadius:
        {
            m_sharedData.gimmeRadius = evt.packet.as<std::uint8_t>();

            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::GimmeDesc;
            cmd.action = [&](cro::Entity e, float)
            {
                e.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
                centreText(e);
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        }
            break;
        case PacketID::HoleCount:
        {
            m_sharedData.holeCount = evt.packet.as<std::uint8_t>();
            //LogI << cro::FileSystem::getFileName(__FILE__) << ", " << __LINE__ << ": update UI" << std::endl;

            cro::Command cmd;
            cmd.targetFlags = CommandID::Menu::CourseHoles;
            if (auto data = std::find_if(m_courseData.cbegin(), m_courseData.cend(),
                [&](const CourseData& cd)
                {
                    return cd.directory == m_sharedData.mapDirectory;
                }); data != m_courseData.cend())
            {
                cmd.action = [&, data](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(data->holeCount[m_sharedData.holeCount]);
                    centreText(e);
                };
            }
            else
            {
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(" ");
                    centreText(e);
                };
            }
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
        case PacketID::ClientVersion:
            //server is requesting our client version, and stating current game mode
            if (evt.packet.as<std::uint16_t>() == static_cast<std::uint16_t>(Server::GameMode::Golf))
            {
                //reply if we're the right mode
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientVersion, CURRENT_VER, net::NetFlag::Reliable);
            }
            else
            {
                //else bail
                m_sharedData.errorMessage = "This is not a Golf lobby";
                requestStackPush(StateID::Error);
            }
            break;
        break;
        }
    }
    else if (evt.type == net::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Lost Connection To Host";
        requestStackPush(StateID::Error);
    }
}

void MenuState::finaliseGameCreate()
{
#ifdef USE_GNS
    //gns only supports 127.0.0.1 for loopback, but to report correct local IP with enet we need 255.255.255.255
    m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("127.0.0.1", ConstVal::GamePort);
#else
    m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);
#endif
    if (!m_sharedData.clientConnection.connected)
    {
        m_sharedData.serverInstance.stop();
        m_sharedData.errorMessage = "Failed to connect to local server.\nPlease make sure port "
            + std::to_string(ConstVal::GamePort)
            + " is allowed through\nany firewalls or NAT";
        requestStackPush(StateID::Error);
    }
    else
    {
        //make sure the server knows we're the host
        m_sharedData.serverInstance.setHostID(m_sharedData.clientConnection.netClient.getPeer().getID());

        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::ReadyButton;
        cmd.action = [&](cro::Entity e, float)
        {
            e.getComponent<cro::Sprite>() = m_sprites[SpriteID::StartGame];
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cmd.targetFlags = CommandID::Menu::ServerInfo;
        cmd.action = [&](cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(
                "Hosting on: " + m_sharedData.clientConnection.netClient.getPeer().getAddress() + ":"
                + std::to_string(ConstVal::GamePort));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //enable the course selection in the lobby
        addCourseSelectButtons();

        //send a UI refresh to correctly place buttons
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());
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
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


        //send the initially selected map/course
        m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
        auto data = serialiseString(m_sharedData.mapDirectory);
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

        //and scoring type
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ScoreType, m_sharedData.scoreType, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    }
}

void MenuState::finaliseGameJoin()
{
    m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(m_sharedData.targetIP.toAnsiString(), ConstVal::GamePort);
    if (!m_sharedData.clientConnection.connected)
    {
        m_sharedData.errorMessage = "Could not connect to server";
        requestStackPush(StateID::Error);
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::ReadyButton;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Sprite>() = m_sprites[SpriteID::ReadyUp];
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::Menu::ServerInfo;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString("Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort));
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //disable the course selection in the lobby
    cmd.targetFlags = CommandID::Menu::CourseSelect;
    cmd.action = [&](cro::Entity e, float)
    {
        m_uiScene.destroyEntity(e);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
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
    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(m_currentMenu);

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

bool MenuState::applyTextEdit()
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
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(m_currentMenu);
            };
            m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd2);
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        SDL_StopTextInput();
        m_textEdit = {};
        return true;
    }
    m_textEdit = {};
    return false;
}