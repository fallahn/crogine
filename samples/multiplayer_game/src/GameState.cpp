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

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/gui/imgui.h>
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
    m_inputParser   (sd.clientConnection.netClient)
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
    }
}

void GameState::spawnPlayer(PlayerInfo info)
{
    if (info.playerID == m_sharedData.clientConnection.playerID
        && !m_inputParser.getEntity().isValid())
    {
        //this is us
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(info.spawnPosition);
        entity.getComponent<cro::Transform>().setRotation(Util::decompressQuat(info.rotation));

        entity.addComponent<Player>().id = info.playerID;
        entity.getComponent<Player>().spawnPosition = info.spawnPosition;
        entity.addComponent<cro::Camera>();
        playerEntity = entity;
        m_inputParser.setEntity(entity);

        m_gameScene.setActiveCamera(entity);
        updateView();
    }
    else
    {
        //spawn an avatar
    }
}