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

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

GameState::GameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });

    updateView();
    context.mainWindow.setMouseCaptured(true);
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

        }
    }
    else
    {
        //we've been disconnected somewhere - push error state
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


    //create a player entity and attach a camera
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 50.f });
    entity.addComponent<Player>();
    entity.addComponent<cro::Camera>();

    m_inputParser.setEntity(entity);

    m_gameScene.setActiveCamera(entity);
    updateView();
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