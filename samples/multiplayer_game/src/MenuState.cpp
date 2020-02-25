/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "Slider.hpp"

#include <crogine/core/App.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/detail/GlobalConsts.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>


namespace
{
    enum CommandID
    {
        RootNode = 0x1
    };
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
	: cro::State    (stack, context),
    m_sharedData    (sd),
    m_scene         (context.appInstance.getMessageBus()),
    m_hosting       (false)
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
    context.appInstance.setClearColour(cro::Colour(0.2f, 0.2f, 0.26f));

    sd.clientConnection.ready = false;
    //TODO we need to check the connection state if we returned here after a game completed
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
        case SDLK_1:

            break;
        case SDLK_2:
            
            break;
        case SDLK_3:
            
            break;
        case SDLK_4:

            break;
        }
    }

    m_scene.getSystem<cro::UISystem>().handleEvent(evt);

    m_scene.forwardEvent(evt);
	return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }

    m_scene.forwardMessage(msg);
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

    m_scene.simulate(dt);
	return true;
}

void MenuState::render()
{
	//draw any renderable systems
    m_scene.render(cro::App::getWindow());
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<SliderSystem>(mb);
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::TextRenderer>(mb);
    m_scene.addSystem<cro::SpriteRenderer>(mb);
}

void MenuState::loadAssets()
{
    m_font.loadFromFile("assets/fonts/VeraMono.ttf");
}

void MenuState::createScene()
{
#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            ImGui::SetNextWindowSize({ 400.f, 400.f });
            if (ImGui::Begin("Main Menu"))
            {
                if (m_currentMenu == Main)
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
                            //switch to connection view
                            cro::Command cmd;
                            cmd.targetFlags = CommandID::RootNode;
                            cmd.action = [](cro::Entity e, float)
                            {
                                e.getComponent<Slider>().destination = { (float)cro::DefaultSceneSize.x, 0.f, 0.f };
                                e.getComponent<Slider>().active = true;
                            };
                            m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
                            m_currentMenu = Join;
                        }
                    }
                }
                else if (m_currentMenu == Join)
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
                        cro::Command cmd;
                        cmd.targetFlags = CommandID::RootNode;
                        cmd.action = [](cro::Entity e, float)
                        {
                            e.getComponent<Slider>().destination = glm::vec3(0.f);
                            e.getComponent<Slider>().active = true;
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                        m_sharedData.clientConnection.netClient.disconnect();
                        m_sharedData.serverInstance.stop();
                        m_sharedData.clientConnection.connected = false;
                        m_currentMenu = Main;
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
                        cro::Command cmd;
                        cmd.targetFlags = CommandID::RootNode;
                        cmd.action = [](cro::Entity e, float)
                        {
                            e.getComponent<Slider>().destination = glm::vec3(0.f);
                            e.getComponent<Slider>().active = true;
                        };
                        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                        m_sharedData.clientConnection.netClient.disconnect();
                        m_sharedData.serverInstance.stop();
                        m_sharedData.clientConnection.connected = false;
                        m_currentMenu = Main;
                    }
                }

            }
            ImGui::End();        
        });
#endif //CRO_DEBUG_

    auto mouseEnterCallback = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e, glm::vec2)
        {
            e.getComponent<cro::Text>().setColour(cro::Colour::Red());        
        });
    auto mouseExitCallback = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e, glm::vec2) 
        {
            e.getComponent<cro::Text>().setColour(cro::Colour::White());
        });

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::RootNode;

    createMainMenu(entity, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(entity, mouseEnterCallback, mouseExitCallback);
    createJoinMenu(entity, mouseEnterCallback, mouseExitCallback);
    createLobbyMenu(entity, mouseEnterCallback, mouseExitCallback);
    createOptionsMenu(entity, mouseEnterCallback, mouseExitCallback);

    //entity.addComponent<Slider>();
    //auto& rootTx = entity.getComponent<cro::Transform>();

    //entity = m_scene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ 10.f, 1000.f, 0.f });
    //entity.addComponent<cro::Text>(m_font);
    //entity.getComponent<cro::Text>().setString("Main Menu");
    //entity.getComponent<cro::Text>().setCharSize(40);
    //entity.getComponent<cro::Text>().setColour(cro::Colour::White());
    //rootTx.addChild(entity.getComponent<cro::Transform>());


    //entity = m_scene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ 10.f + cro::DefaultSceneSize.x, 1000.f, 0.f });
    //entity.addComponent<cro::Text>(m_font);
    //entity.getComponent<cro::Text>().setString("Lobby");
    //entity.getComponent<cro::Text>().setCharSize(40);
    //entity.getComponent<cro::Text>().setColour(cro::Colour::White());
    //rootTx.addChild(entity.getComponent<cro::Transform>());

    //entity = m_scene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ -(cro::DefaultSceneSize.x - 10.f), 1000.f, 0.f });
    //entity.addComponent<cro::Text>(m_font);
    //entity.getComponent<cro::Text>().setString("Join");
    //entity.getComponent<cro::Text>().setCharSize(40);
    //entity.getComponent<cro::Text>().setColour(cro::Colour::White());
    //rootTx.addChild(entity.getComponent<cro::Transform>());


    //set a custom camera so the scene doesn't overwrite the viewport
    //with the default view when resizing the window
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>();
    m_scene.setActiveCamera(entity);
    updateView();
}

void MenuState::handleNetEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::StateChange:
            if (evt.packet.as<std::uint8_t>() == Sv::StateID::Game)
            {
                requestStackClear();
                requestStackPush(States::Game);
            }
            break;
        case PacketID::ConnectionAccepted:
            {
                m_sharedData.clientConnection.playerID = evt.packet.as<std::uint8_t>();

                m_currentMenu = Lobby;

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
        }
    }
}

void MenuState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam.projectionMatrix = glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;
}