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

#include "GameState.hpp"
#include "SharedStateData.hpp"
#include "PlayerSystem.hpp"
#include "PacketIDs.hpp"
#include "ActorIDs.hpp"
#include "ClientCommandIDs.hpp"
#include "InterpolationSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    //for debug output
    cro::Entity playerEntity;

}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_inputParser   (sd.clientConnection.netClient),
    m_cameraPosIndex(0)
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    updateView();
    context.mainWindow.setMouseCaptured(true);
    sd.clientConnection.ready = false;

    //debug output
    registerWindow([&]()
        {
            if (playerEntity.isValid())
            {
                ImGui::SetNextWindowSize({ 300.f, 120.f });
                ImGui::Begin("Info");

                ImGui::Text("Player ID: %d", m_sharedData.clientConnection.playerID);

                auto pos = playerEntity.getComponent<cro::Transform>().getPosition();
                auto rotation = playerEntity.getComponent<cro::Transform>().getRotation() * cro::Util::Const::radToDeg;
                ImGui::Text("Position: %3.3f, %3.3f, %3.3f", pos.x, pos.y, pos.z);
                ImGui::Text("Rotation: %3.3f, %3.3f, %3.3f", rotation.x, rotation.y, rotation.z);

                auto mouse = playerEntity.getComponent<Player>().inputStack[playerEntity.getComponent<Player>().lastUpdatedInput];
                ImGui::Text("Mouse Movement: %d, %d", mouse.xMove, mouse.yMove);

                ImGui::End();
            }
        });
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() 
        || cro::ui::wantsKeyboard()
        || cro::Console::isVisible())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_F3:
            updateCameraPosition();
            break;
        }
    }

    m_inputParser.handleEvent(evt);

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
    else if (msg.id == cro::Message::ConsoleMessage)
    {
        const auto& data = msg.getData<cro::Message::ConsoleEvent>();
        getContext().mainWindow.setMouseCaptured(data.type == cro::Message::ConsoleEvent::Closed);
    }
}

bool GameState::simulate(float dt)
{
    if (m_sharedData.clientConnection.connected)
    {
        cro::NetEvent evt;
        while (m_sharedData.clientConnection.netClient.pollEvent(evt))
        {
            if (evt.type == cro::NetEvent::PacketReceived)
            {
                handlePacket(evt.packet);
            }
        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
    }

    //if we haven't had the server reply yet, tell it we're ready
    if (!m_sharedData.clientConnection.ready)
    {
        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClientReady, m_sharedData.clientConnection.playerID, cro::NetFlag::Unreliable);
    }


    m_inputParser.update();

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CommandSystem>(mb);
    m_gameScene.addSystem<InterpolationSystem>(mb);
    m_gameScene.addSystem<PlayerSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void GameState::loadAssets()
{

}

void GameState::createScene()
{
    //create ground plane for testing
    cro::ModelDefinition modelDef;
    modelDef.loadFromFile("assets/models/ground_plane.cmt", m_resources);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setRotation({ -90.f * cro::Util::Const::degToRad, 0.f, 0.f });
    modelDef.createModel(entity, m_resources);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -7.f });
    modelDef.createModel(entity, m_resources);
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::Green());
}

void GameState::createUI()
{

}

void GameState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam3D = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam3D.projectionMatrix = glm::perspective(35.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 280.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}

void GameState::handlePacket(const cro::NetEvent::Packet& packet)
{
    switch (packet.getID())
    {
    default: break;
    case PacketID::PlayerSpawn:
        //TODO we want to flag all these + world data
        //so we know to stop requesting world data
        m_sharedData.clientConnection.ready = true;
        spawnPlayer(packet.as<PlayerInfo>());
        break;
    case PacketID::PlayerUpdate:
        //we assume we're only receiving our own
        m_gameScene.getSystem<PlayerSystem>().reconcile(m_inputParser.getEntity(), packet.as<PlayerUpdate>());
        break;
    case PacketID::ActorUpdate:
    {
        auto update = packet.as<ActorUpdate>();
        cro::Command cmd;
        cmd.targetFlags = Client::CommandID::Interpolated;
        cmd.action = [update](cro::Entity e, float)
        {
            if (e.isValid() &&
                e.getComponent<Actor>().serverEntityId == update.serverID)
            {
                auto& interp = e.getComponent<InterpolationComponent>();
                interp.setTarget({ update.position, Util::decompressQuat(update.rotation), update.timestamp });
            }
        };
        m_gameScene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    }
        break;
    }
}

void GameState::spawnPlayer(PlayerInfo info)
{
    //TODO move code up from below to share between conditions and make
    //sure this function is not called twice on the same ID

    if (info.playerID == m_sharedData.clientConnection.playerID)
    {
        if (!m_inputParser.getEntity().isValid())
        {
            //this is us


            //TODO do we want to cache this model def?
            cro::ModelDefinition modelDef;
            modelDef.loadFromFile("assets/models/head.cmt", m_resources);

            auto entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
            entity.getComponent<cro::Transform>().setRotation(Util::decompressQuat(info.rotation));
            modelDef.createModel(entity, m_resources);
            
            entity.addComponent<Actor>().id = info.playerID;
            entity.getComponent<Actor>().serverEntityId = info.serverID;

            entity.addComponent<Player>().id = info.playerID;
            entity.getComponent<Player>().spawnPosition = info.spawnPosition;
            playerEntity = entity;
            m_inputParser.setEntity(entity);

            auto& tx = entity.getComponent<cro::Transform>();

            //add the camera as a child so we can change
            //the perspective as necessary
            entity = m_gameScene.createEntity();
            entity.addComponent<cro::Transform>();
            tx.addChild(entity.getComponent<cro::Transform>());

            entity.addComponent<cro::Camera>();
            m_gameScene.setActiveCamera(entity);
            updateView();
        }
    }
    else
    {
        //spawn an avatar
        //TODO check this avatar doesn't already exist
        //TODO interpolation component
        //TODO put some of this shared code in own function with above
        cro::ModelDefinition modelDef;
        modelDef.loadFromFile("assets/models/head.cmt", m_resources);

        auto entity = m_gameScene.createEntity();
        auto rotation = Util::decompressQuat(info.rotation);
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(rotation);
        modelDef.createModel(entity, m_resources);

        entity.addComponent<Actor>().id = info.playerID;
        entity.getComponent<Actor>().serverEntityId = info.serverID;

        entity.addComponent<cro::CommandTarget>().ID = Client::CommandID::Interpolated;
        entity.addComponent<InterpolationComponent>(InterpolationPoint(info.spawnPosition, rotation, info.timestamp));
    }
}

void GameState::updateCameraPosition()
{
    m_cameraPosIndex = (m_cameraPosIndex + 1) % 3;
    auto& tx = m_gameScene.getActiveCamera().getComponent<cro::Transform>();
    switch (m_cameraPosIndex)
    {
    default: break;
    case 0:
        tx.setRotation(glm::quat(1.f, 0.f, 0.f, 0.f));
        tx.setOrigin(glm::vec3(0.f));
        break;
    case 1:
        tx.setOrigin(glm::vec3(0.f, 0.f, -10.f)); //TODO update this once we settle on a scale (need smaller heads!)
        break;
    case 2:
        tx.rotate(glm::vec3(0.f, 1.f, 0.f), cro::Util::Const::PI);
        break;
    }
}