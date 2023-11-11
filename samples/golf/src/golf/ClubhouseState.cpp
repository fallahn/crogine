/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "ClubhouseState.hpp"
#include "SharedStateData.hpp"
#include "SharedProfileData.hpp"
#include "MenuConsts.hpp"
#include "PacketIDs.hpp"
#include "CommandIDs.hpp"
#include "Utility.hpp"
#include "PoissonDisk.hpp"
#include "GolfCartSystem.hpp"
#include "NameScrollSystem.hpp"
#include "MessageIDs.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

using namespace cl;

namespace
{
#include "shaders/CelShader.inl"
#include "shaders/BillboardShader.inl"
#include "shaders/ShaderIncludes.inl"
#include "shaders/TVShader.inl"

    struct TableCallbackData final
    {
        float progress = 0.f;
        std::int32_t direction = 1; //1 to show 0 to hide
    };

    //cro::Entity tableCamera;
    //cro::Entity tableEnt;
}

ClubhouseContext::ClubhouseContext(ClubhouseState* state)
{
    uiScene = &state->m_uiScene;
    currentMenu = &state->m_currentMenu;
    menuEntities = state->m_menuEntities.data();

    menuPositions = state->m_menuPositions.data();
    viewScale = &state->m_viewScale;
    sharedData = &state->m_sharedData;
}

constexpr std::array<glm::vec2, ClubhouseState::MenuID::Count> ClubhouseState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, 0.f)
};

ClubhouseState::ClubhouseState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, const SharedProfileData& sp, GolfGame& gg)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_profileData       (sp),
    m_golfGame          (gg),
    m_matchMaking       (ctx.appInstance.getMessageBus()),
    m_backgroundScene   (ctx.appInstance.getMessageBus()),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_tableScene        (ctx.appInstance.getMessageBus()),
    m_scaleBuffer       ("PixelScale"),
    m_resolutionBuffer  ("ScaledResolution"),
    m_windBuffer        ("WindValues"),
    m_tableIndex        (0),
    m_viewScale         (2.f),
    m_currentMenu       (MenuID::Main),
    m_prevMenu          (MenuID::Main),
    m_gameCreationIndex (0),
    m_arcadeIndexKey    (0),
    m_arcadeIndexJoy    (0)
{
    //if we were returning from arcade this tidies up, else does nothing
    gg.unloadPlugin();

    std::fill(m_readyState.begin(), m_readyState.end(), false);
    
    sd.baseState = StateID::Clubhouse;
    sd.mapDirectory = "pool";

    Achievements::refreshGlobalStats();
    Social::getMonthlyChallenge().refresh();

    ctx.mainWindow.loadResources([this]() {
        addSystems();
        loadResources();
        buildScene();

#ifdef USE_GNS
        Social::findLeaderboards(Social::BoardType::Courses);

        //cached menu states depend on steam stats being
        //up to date so this hacks in a delay and pumps the callback loop
        cro::Clock cl;
        while (cl.elapsed().asSeconds() < 3.f)
        {
            Achievements::update();
        }
#endif

        cacheState(StateID::Options);
        cacheState(StateID::Trophy);
        cacheState(StateID::Leaderboard);
        cacheState(StateID::Stats);
        cacheState(StateID::League);
        });

    ctx.mainWindow.setMouseCaptured(false);


    //this is actually set as a flag from the pause menu
    //to say we want to quit
    if (sd.tutorial)
    {
        sd.serverInstance.stop();
        sd.hosting = false;

        sd.tutorial = false;
        sd.clientConnection.connected = false;
        sd.clientConnection.connectionID = ConstVal::NullValue;
        sd.clientConnection.ready = false;
        sd.clientConnection.netClient.disconnect();
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
            if (m_sharedData.localConnectionData.playerCount == 1)
            {
                m_matchMaking.createLobby(2, Server::GameMode::Billiards);
            }

            spriteID = SpriteID::StartGame;
            connectionString = "Hosting on: localhost:" + std::to_string(ConstVal::GamePort);

            //auto ready up if host
            m_sharedData.clientConnection.netClient.sendPacket(
                PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                net::NetFlag::Reliable, ConstVal::NetChannelReliable);


            //set the course selection menu
            addTableSelectButtons();

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
            m_sharedData.mapDirectory = "pool"; //TODO read this from a list of parsed directories EG m_tables[m_sharedDirectory.courseIndex]
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
        for (auto& cd : sd.connectionData)
        {
            cd.playerCount = 0;
        }
        sd.hosting = false;

        //we might have switched here from an invite received while in the clubhouse
        if (m_sharedData.inviteID)
        {
            //do this via message handler to allow state to fully load
            auto* msg = postMessage<MatchMaking::Message>(MatchMaking::MessageID);
            msg->gameType = Server::GameMode::Billiards;
            msg->hostID = m_sharedData.inviteID;
            msg->type = MatchMaking::Message::LobbyInvite;

            sd.localConnectionData.playerCount = 1;
        }
        else
        {
            //assume two players for a local game
            sd.localConnectionData.playerCount = 2;
        }
    }
    m_sharedData.inviteID = 0;
    m_sharedData.lobbyID = 0;

    Achievements::setActive(true);
    Social::setStatus(Social::InfoID::Menu, { "Clubhouse" });
    Social::setGroup(0);
    Social::getRandomBest();

#ifdef CRO_DEBUG_
    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("Buns"))
    //        {
    //            auto size = glm::vec2(m_tvTopFive.profileIcons.getSize());
    //            ImGui::Image(m_tvTopFive.profileIcons, { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });
    //            //ImGui::Text("Index %u", m_arcadeIndexJoy);
    //            //float maxDepth = m_backgroundScene.getActiveCamera().getComponent<cro::Camera>().getMaxShadowDistance();
    //            //if (ImGui::SliderFloat("Depth", &maxDepth, 1.f, 30.f))
    //            //{
    //            //    m_backgroundScene.getActiveCamera().getComponent<cro::Camera>().setMaxShadowDistance(maxDepth);
    //            //}

    //            //float ext = m_backgroundScene.getActiveCamera().getComponent<cro::Camera>().getShadowExpansion();
    //            //if (ImGui::SliderFloat("ext", &ext, 0.f, 30.f))
    //            //{
    //            //    m_backgroundScene.getActiveCamera().getComponent<cro::Camera>().setShadowExpansion(ext);
    //            //}

    //            //auto rot = m_backgroundScene.getSunlight().getComponent<cro::Transform>().getRotation();
    //            //ImGui::Text("%3.3f, %3.3f, %3.3f, %3.3f", rot.x, rot.y, rot.z, rot.w);
    //        }
    //        ImGui::End();
    //    });
#endif

    cro::App::getInstance().resetFrameTime();
}

//public
bool ClubhouseState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }

    auto quitMenu = [&]()
    {
        switch (m_currentMenu)
        {
        default: break;
        case MenuID::PlayerSelect:
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
            m_currentMenu = MenuID::Dummy;
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            break;
        case MenuID::Join:
            applyTextEdit();
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::PlayerSelect;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
            m_currentMenu = MenuID::Dummy;
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            break;
        case MenuID::Lobby:
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            quitLobby();
            break;
        case MenuID::HallOfFame:
            m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<HOFCallbackData>().state = 2;
            m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;
            m_currentMenu = MenuID::Dummy;
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            break;
        }
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: 
            if (m_arcadeIndexKey < ArcadeKey.size())
            {
                if (evt.key.keysym.sym == ArcadeKey[m_arcadeIndexKey])
                {
                    m_arcadeIndexKey++;
                    if (m_arcadeIndexKey == ArcadeKey.size())
                    {
                        //MAGIC
                        if (m_arcadeEnt.isValid())
                        {
                            m_arcadeEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                            m_arcadeEnt.getComponent<cro::UIInput>().enabled = true;

                            m_audioEnts[AudioID::Win].getComponent<cro::AudioEmitter>().play();
                            Achievements::awardAchievement(AchievementStrings[AchievementID::Gamer]);
                        }
                    }
                }
                else
                {
                    m_arcadeIndexKey = 0;
                }
            }
            
            break;
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
            m_arcadeIndexKey = 0;
            applyTextEdit();
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            quitMenu();
            break;
#ifdef CRO_DEBUG_
        case SDLK_KP_9:
            requestStackPush(StateID::Leaderboard);
            break;
#endif
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        handleTextEdit(evt);
        switch (evt.key.keysym.sym)
        {
        default:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        case SDLK_F1:
        case SDLK_F2:
        case SDLK_F3:
        case SDLK_F4:
        case SDLK_F5:
        case SDLK_F6:
        case SDLK_F7:
        case SDLK_F8:
        case SDLK_F9:
        case SDLK_F10:
        case SDLK_F11:
        case SDLK_F12:
            break;
        }
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEdit(evt);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);
        switch (evt.cbutton.button)
        {
        default: 
            if (m_arcadeIndexJoy < ArcadeJoy.size())
            {
                if (evt.cbutton.button == ArcadeJoy[m_arcadeIndexJoy])
                {
                    m_arcadeIndexJoy++;
                    if (m_arcadeIndexJoy == ArcadeJoy.size())
                    {
                        //MAGIC
                        if (m_arcadeEnt.isValid())
                        {
                            m_arcadeEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                            m_arcadeEnt.getComponent<cro::UIInput>().enabled = true;

                            m_audioEnts[AudioID::Win].getComponent<cro::AudioEmitter>().play();

                            Achievements::awardAchievement(AchievementStrings[AchievementID::Gamer]);
                        }
                    }
                }
                else
                {
                    m_arcadeIndexJoy = 0;
                }
            }
            
            
            break;
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
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_backgroundScene.forwardEvent(evt);
    m_tableScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);

    return false;
}

void ClubhouseState::handleMessage(const cro::Message& msg)
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
        case MatchMaking::Message::LobbyListUpdated:
            updateLobbyList();
            break;
        case MatchMaking::Message::LobbyInvite:
            if (!m_sharedData.clientConnection.connected)
            {
                if (data.gameType == Server::GameMode::Billiards)
                {
                    m_matchMaking.joinGame(data.hostID);
                    m_sharedData.lobbyID = data.hostID;
                    m_sharedData.localConnectionData.playerCount = 1;
                }
                else
                {
                    m_sharedData.inviteID = data.hostID;
                    requestStackClear();
                    requestStackPush(StateID::Menu);
                }
            }            
            break;
        case MatchMaking::Message::GameCreateFailed:
            //local games still work
            [[fallthrough]];
        case MatchMaking::Message::GameCreated:
            finaliseGameCreate();
            break;
        case MatchMaking::Message::LobbyCreated:
            //broadcast the lobby ID to clients. This will also join ourselves.
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::NewLobbyReady, data.hostID, net::NetFlag::Reliable);
            break;
        case MatchMaking::Message::LobbyJoined:
            finaliseGameJoin(data);
            break;
        case MatchMaking::Message::LobbyJoinFailed:
            m_matchMaking.refreshLobbyList(Server::GameMode::Billiards);
            updateLobbyList();
            m_sharedData.errorMessage = "Join Failed:\n\nEither full\nor\nno longer exists.";
            requestStackPush(StateID::MessageOverlay);
            break;
        }
    }
    else if (msg.id == MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::ShadowQualityChanged)
        {
            auto shadowRes = m_sharedData.hqShadows ? 4096 : 2048;
            m_backgroundScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.create(shadowRes, shadowRes);
        }
    }
    else if (msg.id == Social::MessageID::SocialMessage)
    {
        const auto& data = msg.getData<Social::SocialEvent>();
        if (data.type == Social::SocialEvent::AvatarDownloaded)
        {
            //this must be our random top 5 entry
            m_tvTopFive.addProfile(data.playerID);
        }
    }

    m_backgroundScene.forwardMessage(msg);
    m_tableScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ClubhouseState::simulate(float dt)
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

#ifdef CRO_DEBUG_
    
    //glm::vec3 movement(0.f);
    //float rotation = 0.f;
    //auto& tx = m_backgroundScene.getActiveCamera().getComponent<cro::Transform>();

    //if (cro::Keyboard::isKeyPressed(SDLK_d))
    //{
    //    movement += tx.getRightVector();
    //}
    //if (cro::Keyboard::isKeyPressed(SDLK_a))
    //{
    //    movement -= tx.getRightVector();
    //}

    //if (cro::Keyboard::isKeyPressed(SDLK_w))
    //{
    //    movement += tx.getForwardVector();
    //}
    //if (cro::Keyboard::isKeyPressed(SDLK_s))
    //{
    //    movement -= tx.getForwardVector();
    //}

    //if (cro::Keyboard::isKeyPressed(SDLK_q))
    //{
    //    rotation -= dt;
    //}
    //if (cro::Keyboard::isKeyPressed(SDLK_e))
    //{
    //    rotation += dt;
    //}

    //if (glm::length2(movement) != 0)
    //{
    //    movement = glm::normalize(movement);
    //    tx.move(movement * dt);
    //    LogI << tx.getPosition() << std::endl;
    //}

    //if (rotation != 0)
    //{
    //    tx.rotate(cro::Transform::Y_AXIS, rotation);
    //}

    //rotateEntity(m_backgroundScene.getSunlight(), KeysetID::Light, dt);

#endif

    static float accumTime = 0.f;
    accumTime += dt;

    WindData wind;
    wind.direction[0] = 0.5f;
    wind.direction[1] = 0.5f;
    wind.direction[2] = 0.5f;
    wind.elapsedTime = accumTime;
    m_windBuffer.setData(wind);

    m_arcadeVideo.update(dt);
    m_arcadeVideo2.update(dt);

    m_tvTopFive.update(dt);

    m_backgroundScene.simulate(dt);
    m_tableScene.simulate(dt);
    m_uiScene.simulate(dt);

    return false;
}

void ClubhouseState::render()
{
    m_scaleBuffer.bind(0);
    m_resolutionBuffer.bind(1);
    m_windBuffer.bind(2);

    m_backgroundTexture.clear(cro::Colour::CornflowerBlue);
    m_backgroundScene.render();
    m_backgroundTexture.display();

    m_pictureTexture.clear(cro::Colour::Magenta);
    m_pictureQuad.draw();
    m_tvTopFive.overlay.draw();
    m_tvTopFive.icon.draw();
    m_tvTopFive.name.draw();
    m_pictureTexture.display();

#ifndef CRO_DEBUG_
    if (m_currentMenu == MenuID::Dummy
        || m_currentMenu == MenuID::Lobby)
    {
#endif
        m_tableTexture.clear(cro::Colour::Transparent);
        m_tableScene.render();
        auto oldCam = m_tableScene.setActiveCamera(m_ballCam);
        m_tableScene.render();
        m_tableScene.setActiveCamera(oldCam);
        m_tableTexture.display();
#ifndef CRO_DEBUG_
    }
#endif

    m_uiScene.render();
}

//private
void ClubhouseState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_backgroundScene.addSystem<GolfCartSystem>(mb);
    m_backgroundScene.addSystem<cro::CallbackSystem>(mb);
    m_backgroundScene.addSystem<cro::SkeletalAnimator>(mb);
    m_backgroundScene.addSystem<cro::ModelRenderer>(mb);
    m_backgroundScene.addSystem<cro::BillboardSystem>(mb);
    m_backgroundScene.addSystem<cro::CameraSystem>(mb);
    m_backgroundScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_backgroundScene.addSystem<cro::AudioSystem>(mb);

    m_tableScene.addSystem<cro::CallbackSystem>(mb);
    m_tableScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_tableScene.addSystem<cro::ModelRenderer>(mb);
    m_tableScene.addSystem<cro::CameraSystem>(mb);

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<NameScrollSystem>(mb);
    m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioPlayerSystem>(mb);
}

void ClubhouseState::loadResources()
{
    std::fill(m_materialIDs.begin(), m_materialIDs.end(), -1);

    for (const auto& [name, str] : IncludeMappings)
    {
        m_resources.shaders.addInclude(name, str);
    }

    m_resources.shaders.loadFromString(ShaderID::Course, CelVertexShader, CelFragmentShader, "#define TEXTURED\n#define RX_SHADOWS\n");
    auto* shader = &m_resources.shaders.get(ShaderID::Course);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Cel] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Leaderboard, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define TEXTURED\n#define RX_SHADOWS\n");
    shader = &m_resources.shaders.get(ShaderID::Leaderboard);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Shelf] = m_resources.materials.add(*shader);

    m_resources.shaders.loadFromString(ShaderID::Trophy, CelVertexShader, CelFragmentShader, "#define VERTEX_COLOURED\n#define RX_SHADOWS\n#define REFLECTIONS\n");
    shader = &m_resources.shaders.get(ShaderID::Trophy);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Trophy] = m_resources.materials.add(*shader);
    if (m_reflectionMap.loadFromFile("assets/golf/images/skybox/billiards/trophy.ccm"))
    {
        m_resources.materials.get(m_materialIDs[MaterialID::Trophy]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));
    }

    m_resources.shaders.loadFromString(ShaderID::Billboard, BillboardVertexShader, BillboardFragmentShader);
    shader = &m_resources.shaders.get(ShaderID::Billboard);
    m_materialIDs[MaterialID::Billboard] = m_resources.materials.add(*shader);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_windBuffer.addShader(*shader);

    m_resources.shaders.loadFromString(ShaderID::CelTexturedSkinned, CelVertexShader, CelFragmentShader, "#define SUBRECT\n#define TEXTURED\n#define SKINNED\n#define NOCHEX\n");
    shader = &m_resources.shaders.get(ShaderID::CelTexturedSkinned);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::CelSkinned] = m_resources.materials.add(*shader);

    //ball material in table preview
    m_resources.shaders.loadFromString(ShaderID::CelTextured, CelVertexShader, CelFragmentShader, "#define REFLECTIONS\n#define NOCHEX\n#define TEXTURED\n#define SUBRECT\n");
    shader = &m_resources.shaders.get(ShaderID::CelTextured);
    m_scaleBuffer.addShader(*shader);
    m_resolutionBuffer.addShader(*shader);
    m_materialIDs[MaterialID::Ball] = m_resources.materials.add(*shader);


    //TV shader
    m_resources.shaders.loadFromString(ShaderID::TV, CelVertexShader, TVFragment, "#define REFLECTIONS\n#define TEXTURED\n");
    shader = &m_resources.shaders.get(ShaderID::TV);
    m_materialIDs[MaterialID::TV] = m_resources.materials.add(*shader);
    m_windBuffer.addShader(*shader);

    if (m_tableCubemap.loadFromFile("assets/golf/images/skybox/billiards/sky.ccm"))
    {
        m_resources.materials.get(m_materialIDs[MaterialID::Ball]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));
        m_resources.materials.get(m_materialIDs[MaterialID::TV]).setProperty("u_reflectMap", cro::CubemapID(m_reflectionMap.getGLHandle()));
    }


    //load the billboard rects from a sprite sheet and convert to templates
    cro::SpriteSheet spriteSheet;
    if (m_sharedData.treeQuality == SharedStateData::Classic)
    {
        spriteSheet.loadFromFile("assets/golf/sprites/shrubbery_low.spt", m_resources.textures);
    }
    else
    {
        spriteSheet.loadFromFile("assets/golf/sprites/shrubbery.spt", m_resources.textures);
    }
    m_billboardTemplates[BillboardID::Grass01] = spriteToBillboard(spriteSheet.getSprite("grass01"));
    m_billboardTemplates[BillboardID::Grass02] = spriteToBillboard(spriteSheet.getSprite("grass02"));
    m_billboardTemplates[BillboardID::Flowers01] = spriteToBillboard(spriteSheet.getSprite("flowers01"));
    m_billboardTemplates[BillboardID::Flowers02] = spriteToBillboard(spriteSheet.getSprite("flowers02"));
    m_billboardTemplates[BillboardID::Flowers03] = spriteToBillboard(spriteSheet.getSprite("flowers03"));
    m_billboardTemplates[BillboardID::Tree01] = spriteToBillboard(spriteSheet.getSprite("tree01"));
    m_billboardTemplates[BillboardID::Tree02] = spriteToBillboard(spriteSheet.getSprite("tree02"));
    m_billboardTemplates[BillboardID::Tree03] = spriteToBillboard(spriteSheet.getSprite("tree03"));
    m_billboardTemplates[BillboardID::Tree04] = spriteToBillboard(spriteSheet.getSprite("tree04"));



    m_menuSounds.loadFromFile("assets/golf/sound/billiards/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
    m_audioEnts[AudioID::Win] = m_uiScene.createEntity();
    m_audioEnts[AudioID::Win].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("star");


    m_tvTopFive.name.setFont(m_sharedData.sharedResources->fonts.get(FontID::Label));
    m_tvTopFive.name.setCharacterSize(LabelTextSize * 2);
    m_tvTopFive.name.setFillColour(TextNormalColour);
    m_tvTopFive.name.setShadowColour(LeaderboardTextDark);
    m_tvTopFive.name.setShadowOffset({ 2.f, -2.f });

    m_tvTopFive.icon.setOrigin({ 92.f, 92.f });
    m_tvTopFive.icon.setPosition(TVPictureSize / 2u);

    m_tvTopFive.overlay.setTexture(m_resources.textures.get("assets/golf/images/tv_overlay.png"));

    validateTables();
}

void ClubhouseState::validateTables()
{
    const std::string tablePath("assets/golf/tables/");

    auto fileList = cro::FileSystem::listFiles(cro::FileSystem::getResourcePath() + tablePath);
    for (const auto& file : fileList)
    {
        TableClientData data;
        if (data.loadFromFile(tablePath + file) //validates properties but not file existence
            && cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + data.viewModel))
        {
            m_tableData.push_back(data);
        }
    }

    //this assumes the file was previously saved in the correct order
    //but it won't be terrible if it's a bit out as it'll be overwritten again.
    cro::ConfigFile cfg;
    cfg.loadFromFile(cro::App::getPreferencePath() + "table_data.cfg", false);
    const auto& objs = cfg.getObjects();
    for (auto i = 0u; i < objs.size(); ++i)
    {
        const auto& objName = objs[i].getName();
        if (objName == "table")
        {
            const auto& objProps = objs[i].getProperties();
            for (const auto& p : objProps)
            {
                const auto propName = p.getName();
                if (propName == "ball")
                {
                    auto idx = p.getValue<std::int32_t>();
                    if (i < m_tableData.size()
                        && !m_tableData[i].ballSkins.empty())
                    {
                        m_tableData[i].ballSkinIndex = std::min(idx, static_cast<std::int32_t>(m_tableData[i].ballSkins.size()) - 1);
                    }
                }
                else if (propName == "table")
                {
                    auto idx = p.getValue<std::int32_t>();
                    if (i < m_tableData.size()
                        && !m_tableData[i].tableSkins.empty())
                    {
                        m_tableData[i].tableSkinIndex = std::min(idx, static_cast<std::int32_t>(m_tableData[i].tableSkins.size()) - 1);
                    }
                }
            }
        }
    }

    if (!m_tableData.empty())
    {
        m_sharedData.mapDirectory = m_tableData[0].name;
    }
}

void ClubhouseState::buildScene()
{
    m_backgroundScene.setCubemap("assets/golf/images/skybox/mountain_spring/sky.ccm");


    cro::ModelDefinition md(m_resources);

    auto applyMaterial = [&](cro::Entity entity, std::int32_t id, std::size_t idx = 0)
    {
        auto material = m_resources.materials.get(m_materialIDs[id]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(idx, material);
    };

    if (md.loadFromFile("assets/golf/models/clubhouse_menu.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
    }

    if (md.loadFromFile("assets/golf/models/menu_ground.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
    }

    if (md.loadFromFile("assets/golf/models/phone_box.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 8.2f, 0.f, 15.8f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
    }

    if (md.loadFromFile("assets/golf/models/trophies/cabinet.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({14.3f, 1.2f, -1.6f});
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/shelf.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.2f, -1.6f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Shelf);

        entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.8f, -1.6f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Shelf);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy05.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.2f, -0.9f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy03.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.2f, -1.2f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy04.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.2f, -2.3f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy07.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.8f, -1.3f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 91.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);

        entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.2f, -2.05f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 89.5f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy06.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.8f, -1.9f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy02.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.8f, -2.15f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/trophies/trophy01.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.25f, 1.8f, -1.f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy, 1);
    }

    if (md.loadFromFile("assets/golf/models/hole_19/snooker_model.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 17.1f, 0.5f, -1.8f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
    }

    if (md.loadFromFile("assets/golf/models/table_legs.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 17.1f, 0.f, -1.8f });
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
    }

    if (md.loadFromFile("assets/golf/models/hole_19/cue.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 17.1f, 0.5f, -1.8f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -87.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy);

        entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 18.4f, 1.5f, -0.01f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -87.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 180.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Trophy);
    }

    if (md.loadFromFile("assets/golf/models/arcade_machine_gvg.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.4f, 0.f, -3.f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().setScale({ 0.8f, 1.f, 1.f });
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("gvg");
        entity.getComponent<cro::AudioEmitter>().play();
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
        //applyMaterial(entity, MaterialID::TV, 1);

        m_arcadeVideo.loadFromFile("assets/golf/video/arcade.mpg");
        m_arcadeVideo.setLooped(true);
        m_arcadeVideo.play();

        entity.getComponent<cro::Model>().setMaterialProperty(1, "u_diffuseMap", cro::TextureID(m_arcadeVideo.getTexture().getGLHandle()));
    }

    if (md.loadFromFile("assets/golf/models/arcade_machine.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.4f, 0.f, -3.6f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 90.f * cro::Util::Const::degToRad);
        entity.getComponent<cro::Transform>().setScale({ 0.8f, 1.f, 1.f });
        md.createModel(entity);

        applyMaterial(entity, MaterialID::Cel);
        //applyMaterial(entity, MaterialID::TV, 1);

        m_arcadeVideo2.loadFromFile("assets/golf/video/arcade2.mpg");
        m_arcadeVideo2.setLooped(true);
        m_arcadeVideo2.play();

        entity.getComponent<cro::Model>().setMaterialProperty(1, "u_diffuseMap", cro::TextureID(m_arcadeVideo2.getTexture().getGLHandle()));
    }

    if (md.loadFromFile("assets/golf/models/pool_lights.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 17.1f, 2.7f, -1.8f });
        md.createModel(entity);
        applyMaterial(entity, MaterialID::Trophy);


        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float)
        {
            static constexpr float BaseX = 17.1f; //see position, above

            static const auto highFreq = cro::Util::Wavetable::sine(5.f, 0.003f);
            static std::size_t highFreqIndex = 0;

            static const auto lowFreq = cro::Util::Wavetable::sine(0.0001f);
            static std::size_t lowFreqIndex = 0;

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = BaseX + (highFreq[highFreqIndex] * cro::Util::Easing::easeInCubic((lowFreq[lowFreqIndex] + 1.f) / 2.f));
            e.getComponent<cro::Transform>().setPosition(pos);

            highFreqIndex = (highFreqIndex + 1) % highFreq.size();
            lowFreqIndex = (lowFreqIndex + 1) % lowFreq.size();
        };


        auto lights = entity;

        if (md.loadFromFile("assets/golf/models/pool_fan.cmt"))
        {
            entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>();
            md.createModel(entity);
            applyMaterial(entity, MaterialID::Trophy);
            lights.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -dt * 3.f);
            };
        }
    }

    m_pictureTexture.create(TVPictureSize.x, TVPictureSize.y, false);
    if (md.loadFromFile("assets/golf/models/picture_frame.cmt"))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.1f, 2.35f, -3.2f });
        entity.getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, -10.f * cro::Util::Const::degToRad);
        md.createModel(entity);


        applyMaterial(entity, MaterialID::Cel, 0);
        applyMaterial(entity, MaterialID::TV, 1);

        entity.getComponent<cro::Model>().setMaterialProperty(1, "u_diffuseMap", cro::TextureID(m_pictureTexture.getTexture().getGLHandle()));
    }

    m_pictureTexture.clear(cro::Colour::Magenta);
    m_pictureTexture.display();


    //billboards
    auto shrubPath = m_sharedData.treeQuality == SharedStateData::Classic ?
        "assets/golf/models/shrubbery_low.cmt" :
        "assets/golf/models/shrubbery.cmt";
    if (md.loadFromFile(shrubPath))
    {
        auto entity = m_backgroundScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        auto billboardMat = m_resources.materials.get(m_materialIDs[MaterialID::Billboard]);
        applyMaterialData(md, billboardMat);

        auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
        noiseTex.setSmooth(true);
        noiseTex.setRepeated(true);
        billboardMat.setProperty("u_noiseTexture", noiseTex);

        entity.getComponent<cro::Model>().setMaterial(0, billboardMat);

        if (entity.hasComponent<cro::BillboardCollection>())
        {
            constexpr std::array minBounds = { 0.f, 20.f };
            constexpr std::array maxBounds = { 60.f, 30.f };

            auto& collection = entity.getComponent<cro::BillboardCollection>();

            auto trees = pd::PoissonDiskSampling(2.8f, minBounds, maxBounds);
            for (auto [x, y] : trees)
            {
                float scale = static_cast<float>(cro::Util::Random::value(12, 22)) / 10.f;

                auto bb = m_billboardTemplates[cro::Util::Random::value(BillboardID::Tree01, BillboardID::Tree04)];
                bb.position = { x - (maxBounds[0] / 2.f), 0.f, y + 10.f };
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
                    material = m_resources.materials.get(m_materialIDs[MaterialID::CelSkinned]);
#ifndef CRO_DEBUG_
                    passengers[i].getComponent<cro::Skeleton>().play(0);
#endif
                }
                else
                {
                    material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
                }
                applyMaterialData(passengerDef, material);
                passengers[i].getComponent<cro::Model>().setMaterial(0, material);
            }
        }



        for (auto i = 0u; i < 2u; ++i)
        {
            auto entity = m_backgroundScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(-10000.f));
            entity.addComponent<GolfCart>();
            md.createModel(entity);
            applyMaterial(entity, MaterialID::Cel);

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



    //ambience 01
    auto entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 23.6f, 2.f, -1.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("01");
    entity.getComponent<cro::AudioEmitter>().play();
    entity.getComponent<cro::AudioEmitter>().setPitch(1.1f);

    //ambience 02
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 17.6f, 1.6f, -1.8f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("02");
    entity.getComponent<cro::AudioEmitter>().play();
    entity.getComponent<cro::AudioEmitter>().setPitch(0.91f);

    //music
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("music");
    entity.getComponent<cro::AudioEmitter>().play();

    auto targetVol = entity.getComponent<cro::AudioEmitter>().getVolume();
    entity.getComponent<cro::AudioEmitter>().setVolume(0.f);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, const float>>(0.f, targetVol);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [currTime, target] = e.getComponent<cro::Callback>().getUserData<std::pair<float, const float>>();
        currTime = std::min(1.f, currTime + (dt * 0.5f));

        e.getComponent<cro::AudioEmitter>().setVolume(cro::Util::Easing::easeOutQuint(currTime) * target);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };

    //unlock
    entity = m_backgroundScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 14.4f, 0.f, -3.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("unlock");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(45.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& f = e.getComponent<cro::Callback>().getUserData<float>();
        f -= dt;
        if (f < 0.f)
        {
            f += 65.f;
            e.getComponent<cro::AudioEmitter>().play();
        }
    };


    //update the 3D view
    auto updateView = [&](cro::Camera& cam)
    {
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float maxScale = getViewScale();
        float scale = m_sharedData.pixelScale ? maxScale : 1.f;
        auto texSize = winSize / scale;

        auto invScale = (maxScale + 1) - scale;
        m_scaleBuffer.setData(invScale);

        ResolutionData d;
        d.resolution = texSize / invScale;
        m_resolutionBuffer.setData(d);

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_sharedData.antialias =
            m_backgroundTexture.create(static_cast<std::uint32_t>(texSize.x), static_cast<std::uint32_t>(texSize.y), true, false, samples)
            && m_sharedData.multisamples != 0
            && !m_sharedData.pixelScale;

        m_pictureQuad.setTexture(m_backgroundTexture.getTexture());
        auto pictureScale = glm::vec2(m_pictureTexture.getSize()) / glm::vec2(m_backgroundTexture.getSize());
        m_pictureQuad.setScale(pictureScale);

        cam.setPerspective(m_sharedData.fov * cro::Util::Const::degToRad, texSize.x / texSize.y, 0.1f, 85.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_backgroundScene.getActiveCamera();
    //camEnt.getComponent<cro::Transform>().setPosition({ 19.187f, 1.54f, -4.37f });
    camEnt.getComponent<cro::Transform>().setPosition({ 18.9f, 1.54f, -4.58f });
    camEnt.getComponent<cro::Transform>().setRotation(glm::quat(-0.31f, 0.004f, -0.95f, 0.0057f));

    auto shadowRes = m_sharedData.hqShadows ? 4096 : 2048;
    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    cam.shadowMapBuffer.create(shadowRes, shadowRes);
    cam.setMaxShadowDistance(11.f);
    cam.setShadowExpansion(9.9f);
    updateView(cam);

    auto sunEnt = m_backgroundScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().setRotation({ -0.079f,0.886f,0.163f,0.427f });
    //sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 120.f * cro::Util::Const::degToRad);
    //sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -40.f * cro::Util::Const::degToRad);

    createTableScene();
    createUI();
}

void ClubhouseState::createTableScene()
{
    //set the camera / lighting first so we at least
    //have a valid render texture even if the table data
    //failed to load

    //camera
    static constexpr glm::uvec2 BaseSize(240, 140);
    static constexpr float Ratio = static_cast<float>(BaseSize.x) / BaseSize.y;
    auto updateView = [&](cro::Camera& cam)
    {
        auto texSize = BaseSize;

        //auto winSize = glm::vec2(cro::App::getWindow().getSize());
        float viewScale = getViewScale();

        if (!m_sharedData.pixelScale)
        {
            texSize *= viewScale;
        }

        std::uint32_t samples = m_sharedData.pixelScale ? 0 :
            m_sharedData.antialias ? m_sharedData.multisamples : 0;

        m_tableTexture.create(texSize.x, texSize.y, true, false, samples);
        
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::CourseDesc;
        cmd.action = [&,viewScale](cro::Entity e, float)
        {
            e.getComponent<cro::Sprite>().setTexture(m_tableTexture.getTexture(), true);
            
            float scale = 1.f;
            if (!m_sharedData.pixelScale)
            {
                scale = 1.f / viewScale;
            }
            e.getComponent<cro::Transform>().setScale({ scale, scale });
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        cam.setPerspective(55.f * cro::Util::Const::degToRad, Ratio, 0.1f, 15.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_tableScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -28.f * cro::Util::Const::degToRad);

    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    cam.shadowMapBuffer.create(1024, 1024);
    updateView(cam);

    auto sunEnt = m_tableScene.getSunlight();
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -130.56f * cro::Util::Const::degToRad);


    m_ballCam = m_tableScene.createEntity();
    m_ballCam.addComponent<cro::Transform>().setPosition({ 10.f, 0.f, 10.15f });
    m_ballCam.addComponent<cro::Camera>().isStatic = true;
    auto updateBallView = [](cro::Camera& cam)
    {
        cam.setPerspective(40.f * cro::Util::Const::degToRad, 1.f, 0.1f, 5.f);
        cam.viewport = { 0.1f, 0.f, 0.2f, 0.2f * Ratio};
    };
    m_ballCam.getComponent<cro::Camera>().resizeCallback = updateBallView;
    updateBallView(m_ballCam.getComponent<cro::Camera>());


    //quit if there's nothing to do
    if (m_tableData.empty())
    {
        return;
    }

    //animates table models
    auto callbackFunc = [&](cro::Entity e, float dt)
    {
        auto& [progress, direction] = e.getComponent<cro::Callback>().getUserData<TableCallbackData>();
        float scale = 0.f;
        if (direction == 0)
        {
            progress = std::max(0.f, progress - (dt * 5.f));

            scale = progress;

            if (progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;

                //show current selected table
                m_tableIndex = m_sharedData.courseIndex;
                m_tableData[m_sharedData.courseIndex].previewModel.getComponent<cro::Callback>().getUserData<TableCallbackData>().direction = 1;
                m_tableData[m_sharedData.courseIndex].previewModel.getComponent<cro::Callback>().active = true;
            }
        }
        else
        {
            progress = std::min(1.f, progress + (dt * 2.f));
            scale = cro::Util::Easing::easeOutBounce(progress);

            if (progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                updateBallTexture();
            }
        }
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };

    cro::ModelDefinition md(m_resources);
    //ball skin preview
    if (md.loadFromFile("assets/golf/models/hole_19/billiard_ball.cmt"))
    {
        auto entity = m_tableScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 9.973f, 0.f, 10.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);

        auto material = m_resources.materials.get(m_materialIDs[MaterialID::Ball]);
        applyMaterialData(md, material);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        glm::vec4 rect;
        rect.x = 0.f;
        rect.y = 0.125f;
        rect.z = 0.5f;
        rect.w = 0.125f;
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);
        m_previewBalls[0] = entity;

        entity = m_tableScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 10.027f, 0.f, 10.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -90.f * cro::Util::Const::degToRad);
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterial(0, material);

        rect.x = 0.5f;
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_subrect", rect);
        m_previewBalls[1] = entity;
    }

    //table models
    for (auto& td : m_tableData)
    {
        if (md.loadFromFile(td.viewModel))
        {
            auto entity = m_tableScene.createEntity();
            entity.addComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, 225.f * cro::Util::Const::degToRad);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            md.createModel(entity);

            auto material = m_resources.materials.get(m_materialIDs[MaterialID::Cel]);
            applyMaterialData(md, material);
            entity.getComponent<cro::Model>().setMaterial(0, material);
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", cro::TextureID(m_resources.textures.get(td.tableSkins[td.tableSkinIndex])));

            //not conducive to custom tables, but my game, my rules :P
            switch (td.rules)
            {
            default:
            case TableData::Eightball:
            case TableData::Nineball:
                entity.getComponent<cro::Transform>().setPosition({ -0.2f, -0.5f, -1.7f });
                break;
            case TableData::Snooker:
                entity.getComponent<cro::Transform>().setPosition({ -0.47f, -0.5f, -2.38f });
                break;
            }

            entity.addComponent<cro::Callback>().function = callbackFunc;
            entity.getComponent<cro::Callback>().setUserData<TableCallbackData>();

            td.previewModel = entity;
        }

        //pre-load the ball textures
        for (const auto& t : td.ballSkins)
        {
            m_resources.textures.get(t);
        }

        //and table textures
        for (const auto& t : td.tableSkins)
        {
            m_resources.textures.get(t);
        }
    }
    m_tableIndex = m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_tableData.size() - 1);
    m_tableData[m_sharedData.courseIndex].previewModel.getComponent<cro::Callback>().active = true;

    updateBallTexture();
}

void ClubhouseState::handleNetEvent(const net::NetEvent& evt)
{
    if (evt.type == net::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::ClientPlayerCount:
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientPlayerCount, m_sharedData.localConnectionData.playerCount, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
            break;
        case PacketID::NewLobbyReady:
            m_matchMaking.joinLobby(evt.packet.as<std::uint64_t>());
            break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == sv::StateID::Billiards)
            {
                //leave the lobby
                m_matchMaking.leaveGame();

                m_sharedData.ballSkinIndex = m_tableData[m_tableIndex].ballSkinIndex;
                m_sharedData.tableSkinIndex = m_tableData[m_tableIndex].tableSkinIndex;

                //save these for later
                cro::ConfigFile cfg("table_skins");
                for (const auto& table : m_tableData)
                {
                    auto* obj = cfg.addObject("table");
                    obj->addProperty("ball").setValue(table.ballSkinIndex);
                    obj->addProperty("table").setValue(table.tableSkinIndex);
                }
                cfg.save(cro::App::getPreferencePath() + "table_data.cfg");

                requestStackClear();
                requestStackPush(StateID::Billiards);
            }
            break;
        case PacketID::ConnectionAccepted:
        {
            //update local player data
            m_sharedData.clientConnection.connectionID = evt.packet.as<std::uint8_t>();
            m_sharedData.localConnectionData.connectionID = evt.packet.as<std::uint8_t>();
            m_sharedData.localConnectionData.peerID = m_sharedData.clientConnection.netClient.getPeer().getID();
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
            auto table = deserialiseString(evt.packet);

            if (auto data = std::find_if(m_tableData.cbegin(), m_tableData.cend(),
                [&table](const TableData& td)
                {
                    return td.name == table;
                }); data != m_tableData.cend())
            {
                //hide current preview model (callback automatically shows new target)
                m_tableData[m_tableIndex].previewModel.getComponent<cro::Callback>().getUserData<TableCallbackData>().direction = 0;
                m_tableData[m_tableIndex].previewModel.getComponent<cro::Callback>().active = true;

                m_sharedData.mapDirectory = table;
                m_sharedData.courseIndex = std::distance(m_tableData.cbegin(), data);

                //update UI
                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::CourseTitle;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(TableStrings[m_tableData[m_sharedData.courseIndex].rules]);
                    centreText(e);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //show/hide the ball skin selection arrow
                cmd.targetFlags = CommandID::Menu::CourseHoles;
                cmd.action = [&](cro::Entity e, float)
                {
                    bool show = m_tableData[m_sharedData.courseIndex].ballSkins.size() > 1;
                    if (show)
                    {
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
                        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    }
                    else
                    {
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Inactive);
                        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                //and table skin selection
                cmd.targetFlags = CommandID::Menu::ScoreType;
                cmd.action = [&](cro::Entity e, float)
                {
                    bool show = m_tableData[m_sharedData.courseIndex].tableSkins.size() > 1;
                    if (show)
                    {
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
                        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    }
                    else
                    {
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Inactive);
                        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                if (m_sharedData.hosting)
                {
                    m_matchMaking.setGameTitle(TableStrings[m_tableData[m_sharedData.courseIndex].rules]);
                }
            }
            else
            {
                //print to UI course is missing
                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::CourseTitle;
                cmd.action = [table](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(table + ": not found.");
                    centreText(e);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
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
            if (evt.packet.as<std::uint16_t>() == static_cast<std::uint16_t>(Server::GameMode::Billiards))
            {
                //reply if we're the right mode
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientVersion, CURRENT_VER, net::NetFlag::Reliable);
            }
            else
            {
                //else bail
                m_sharedData.errorMessage = "This is not a Billiards lobby";
                requestStackPush(StateID::Error);
            }
            break;
        }
    }
    else if (evt.type == net::NetEvent::ClientDisconnect)
    {
        m_matchMaking.leaveGame();
        m_sharedData.errorMessage = "Lost Connection To Host";
        requestStackPush(StateID::Error);
    }
}

void ClubhouseState::finaliseGameCreate()
{
#ifdef USE_GNS
    m_sharedData.clientConnection.connected = m_sharedData.serverInstance.addLocalConnection(m_sharedData.clientConnection.netClient);
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
    
        //enable the game type selection in the lobby
        if (m_tableData.size() > 1)
        {
            addTableSelectButtons();
        }
    
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
    
    
        //send the initially selected table - this triggers the menu to move to the next stage.
        auto data = serialiseString(m_sharedData.mapDirectory);
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);
    }

    refreshLobbyButtons();
}

void ClubhouseState::finaliseGameJoin(const MatchMaking::Message& data)
{
#ifdef USE_GNS
    m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(CSteamID(uint64(data.hostID)));
#else
    m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(m_sharedData.targetIP.toAnsiString(), ConstVal::GamePort);
#endif
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

    refreshLobbyButtons();
}
