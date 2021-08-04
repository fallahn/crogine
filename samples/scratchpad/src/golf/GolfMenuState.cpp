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

#include "GolfMenuState.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "Utility.hpp"

#include <crogine/core/App.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/util/String.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <cstring>

namespace
{

}

GolfMenuState::GolfMenuState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_scene         (context.appInstance.getMessageBus()),
    m_viewScale     (2.f)
{
    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        //add systems to scene
        addSystems();
        //load assets (textures, shaders, models etc)
        loadAssets();
        //create some entities
        createScene();
    });

    context.mainWindow.setMouseCaptured(false);
    context.mainWindow.setTitle("Lotec Golf");
    //context.appInstance.setClearColour(cro::Colour(0.2f, 0.2f, 0.26f));

    sd.clientConnection.ready = false;
    std::fill(m_readyState.begin(), m_readyState.end(), false);
        
    //we returned from a previous game
    if (sd.clientConnection.connected)
    {
        updateLobbyAvatars();

        //switch to lobby view
        cro::Command cmd;
        cmd.targetFlags = MenuCommandID::RootNode;
        cmd.action = [&](cro::Entity e, float)
        {
            e.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Lobby]);
            m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Lobby);
        };
        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

        cro::String buttonString;
        cro::String connectionString;

        if (m_sharedData.hosting)
        {
            buttonString = "Start";
            connectionString = "Hosting on: localhost:" + std::to_string(ConstVal::GamePort);

            //auto ready up if host
            m_sharedData.clientConnection.netClient.sendPacket(
                PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
        }
        else
        {
            buttonString = "Ready";
            connectionString = "Connected to: " + m_sharedData.targetIP + ":" + std::to_string(ConstVal::GamePort);
        }


        cmd.targetFlags = MenuCommandID::ReadyButton;
        cmd.action = [buttonString](cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(buttonString);
        };
        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

        cmd.targetFlags = MenuCommandID::ServerInfo;
        cmd.action = [connectionString](cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(connectionString);
        };
        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
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
}

//public
bool GolfMenuState::handleEvent(const cro::Event& evt)
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
#endif
        case SDLK_1:

            break;
        case SDLK_2:
            
            break;
        case SDLK_3:
            
            break;
        case SDLK_4:

            break;
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
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            handleTextEdit(evt);
            break;
        }
    }
    else if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEdit(evt);
    }

    m_scene.getSystem<cro::UISystem>().handleEvent(evt);

    m_scene.forwardEvent(evt);
    return true;
}

void GolfMenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool GolfMenuState::simulate(float dt)
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

    m_scene.simulate(dt);
    return true;
}

void GolfMenuState::render()
{
    //draw any renderable systems
    m_scene.render(cro::App::getWindow());
}

//private
void GolfMenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
}

void GolfMenuState::loadAssets()
{
    m_font.loadFromFile("assets/golf/fonts/IBM_CGA.ttf");
}

void GolfMenuState::createScene()
{
#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            ImGui::SetNextWindowSize({ 400.f, 400.f });
            if (ImGui::Begin("Main Menu"))
            {
                if (m_scene.getSystem<cro::UISystem>().getActiveGroup() == GroupID::Main)
                {
                    if (ImGui::Button("Host"))
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.launch();

                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}

                            m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);

                            if (!m_sharedData.clientConnection.connected)
                            {
                                m_sharedData.serverInstance.stop();
                                cro::Logger::log("Failed to connect to local server", cro::Logger::Type::Error);
                            }
                        }
                    }

                    if (ImGui::Button("Join"))
                    {
                        if (!m_sharedData.clientConnection.connected
                            && !m_sharedData.serverInstance.running())
                        {
                            cro::Command cmd;
                            cmd.targetFlags = MenuCommandID::RootNode;
                            cmd.action = [&](cro::Entity e, float)
                            {
                                e.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Join]);
                                m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Join);
                            };
                            m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                        }
                    }
                }
                else if (m_scene.getSystem<cro::UISystem>().getActiveGroup() == GroupID::Join)
                {
                    static char buffer[20] = "127.0.0.1";
                    ImGui::InputText("Address", buffer, 20);
                    if (ImGui::Button("Connect"))
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect(buffer, ConstVal::GamePort);
                            if (!m_sharedData.clientConnection.connected)
                            {
                                cro::Logger::log("Could not connect to server", cro::Logger::Type::Error);
                            }
                        }
                    }

                    if (ImGui::Button("Back"))
                    {
                        m_sharedData.clientConnection.netClient.disconnect();
                        m_sharedData.serverInstance.stop();
                        m_sharedData.clientConnection.connected = false;
                        
                        cro::Command cmd;
                        cmd.targetFlags = MenuCommandID::RootNode;
                        cmd.action = [&](cro::Entity e, float)
                        {
                            e.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                            m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                    }
                }
                else
                {
                    //lobby
                    if (ImGui::Button("Start"))
                    {
                        if (m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }

                    if (ImGui::Button("Back"))
                    {
                        m_sharedData.clientConnection.netClient.disconnect();
                        m_sharedData.serverInstance.stop();
                        m_sharedData.clientConnection.connected = false;
                        
                        cro::Command cmd;
                        cmd.targetFlags = MenuCommandID::RootNode;
                        cmd.action = [&](cro::Entity e, float)
                        {
                            e.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Main]);
                            m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Main);
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                    }
                }

            }
            ImGui::End();        
        });
#endif //CRO_DEBUG_

    auto mouseEnterCallback = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextHighlightColour);        
        });
    auto mouseExitCallback = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition( {0.f, 0.f, -10.f} );
    entity.addComponent<cro::Sprite>(m_textureResource.get("assets/golf/images/menu_background.png"));
    entity.addComponent<cro::Drawable2D>();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = MenuCommandID::RootNode;

    createMainMenu(entity, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(entity, mouseEnterCallback, mouseExitCallback);
    createJoinMenu(entity, mouseEnterCallback, mouseExitCallback);
    createLobbyMenu(entity, mouseEnterCallback, mouseExitCallback);
    createOptionsMenu(entity, mouseEnterCallback, mouseExitCallback);
    createPlayerConfigMenu(mouseEnterCallback, mouseExitCallback);

    //set a custom camera so the scene doesn't overwrite the viewport
    //with the default view when resizing the window
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = std::bind(&GolfMenuState::updateView, this, std::placeholders::_1);
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void GolfMenuState::handleNetEvent(const cro::NetEvent& evt)
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
                requestStackPush(States::Golf::Game);
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
                cmd.targetFlags = MenuCommandID::RootNode;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Transform>().setPosition(m_menuPositions[MenuID::Lobby]);
                    m_scene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Lobby);
                };
                m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                if (m_sharedData.serverInstance.running())
                {
                    //auto ready up if host
                    m_sharedData.clientConnection.netClient.sendPacket(
                        PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | std::uint8_t(1)),
                        cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
                }

                LOG("Successfully connected to server", cro::Logger::Type::Info);
            }
            break;
        case PacketID::ConnectionRefused:
        {
            std::string err = evt.packet.as<std::uint8_t>() == 0 ? "Server full" : "Game in progress";
            cro::Logger::log("Connection refused: " + err, cro::Logger::Type::Error);

            m_sharedData.clientConnection.netClient.disconnect();
            m_sharedData.clientConnection.connected = false;
        }
            break;
        case PacketID::LobbyUpdate:
            updateLobbyData(evt);
            break;
        case PacketID::ClientDisconnected:
            m_sharedData.connectionData[evt.packet.as<std::uint8_t>()].playerCount = 0;
            updateLobbyAvatars();
            break;
        case PacketID::LobbyReady:
        {
            std::uint16_t data = evt.packet.as<std::uint16_t>();
            m_readyState[((data & 0xff00) >> 8)] = (data & 0x00ff) ? true : false;
        }
            break;
        case PacketID::MapInfo:
            m_sharedData.mapDirectory = deserialiseString(evt.packet);
            break;
        break;
        }
    }
    else if (evt.type == cro::NetEvent::ClientDisconnect)
    {
        m_sharedData.errorMessage = "Lost Connection To Host";
        requestStackPush(States::Golf::Error);
    }
}

void GolfMenuState::updateView(cro::Camera& cam)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    auto windowSize = size;
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    cam.setOrthographic(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -20.f, 10.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;

    auto vpSize = calcVPSize();

    m_viewScale = glm::vec2(std::floor(windowSize.y / vpSize.y));
}

void GolfMenuState::handleTextEdit(const cro::Event& evt)
{
    if (!m_textEdit.string)
    {
        return;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        //assuming we're only handling backspace...
        if (!m_textEdit.string->empty())
        {
            m_textEdit.string->erase(m_textEdit.string->size() - 1);
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
    if (!m_textEdit.string->empty())
    {
        auto bounds = cro::Text::getLocalBounds(m_textEdit.entity);
       // m_textEdit.entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
        //TODO make this scroll when we hist the edge of the input
    }
}

void GolfMenuState::applyTextEdit()
{
    if (m_textEdit.string && m_textEdit.entity.isValid())
    {
        if (m_textEdit.string->empty())
        {
            *m_textEdit.string = "INVALID";
        }

        m_textEdit.entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        m_textEdit.entity.getComponent<cro::Text>().setString(*m_textEdit.string);
        //auto bounds = cro::Text::getLocalBounds(m_textEdit.entity);
        //m_textEdit.entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, -bounds.height / 2.f });
        m_textEdit.entity.getComponent<cro::Callback>().active = false;
        SDL_StopTextInput();
    }
    m_textEdit = {};
}