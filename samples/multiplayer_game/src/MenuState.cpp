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
    m_scene         (context.appInstance.getMessageBus())
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
            if (!m_sharedData.clientConnection.connected)
            {
                m_sharedData.serverInstance.launch();
                m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("127.0.0.1", ConstVal::GamePort);

                if (!m_sharedData.clientConnection.connected)
                {
                    m_sharedData.serverInstance.stop();
                    cro::Logger::log("Failed to connect to local server", cro::Logger::Type::Error);
                }
            }
            break;
        case SDLK_3:
            if (m_sharedData.clientConnection.connected
                && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
            {
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            }
            break;
        }
    }

    m_scene.forwardEvent(evt);
	return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
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
    m_scene.render();
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<SliderSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::TextRenderer>(mb);

}

void MenuState::loadAssets()
{
    m_font.loadFromFile("assets/fonts/VeraMono.ttf");
}

void MenuState::createScene()
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::RootNode;
    entity.addComponent<Slider>();
    auto& rootTx = entity.getComponent<cro::Transform>();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 1000.f, 0.f });
    entity.addComponent<cro::Text>(m_font);
    entity.getComponent<cro::Text>().setString("1. Host\n2. Join");
    entity.getComponent<cro::Text>().setCharSize(40);
    entity.getComponent<cro::Text>().setColour(cro::Colour::White());
    rootTx.addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f + cro::DefaultSceneSize.x, 1000.f, 0.f });
    entity.addComponent<cro::Text>(m_font);
    entity.getComponent<cro::Text>().setString("3. Start\n4. Quit");
    entity.getComponent<cro::Text>().setCharSize(40);
    entity.getComponent<cro::Text>().setColour(cro::Colour::White());
    rootTx.addChild(entity.getComponent<cro::Transform>());

    m_scene.getActiveCamera().getComponent<cro::Camera>().projectionMatrix = 
        glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
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

                //switch to lobby view
                cro::Command cmd;
                cmd.targetFlags = CommandID::RootNode;
                cmd.action = [](cro::Entity e, float)
                {
                    e.getComponent<Slider>().destination = { -(float)cro::DefaultSceneSize.x, 0.f, 0.f };
                    e.getComponent<Slider>().active = true;
                };
                m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);

                LOG("Successfully connected to server", cro::Logger::Type::Info);
            }
            break;
        }
    }
}