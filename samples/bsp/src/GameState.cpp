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

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/CircleMeshBuilder.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

GameState::GameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool GameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void GameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GameState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void GameState::render()
{
    auto& rt = cro::App::getWindow();
    m_gameScene.render(rt);
    m_uiScene.render(rt);
}

//private
void GameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void GameState::loadAssets()
{

}

void GameState::createScene()
{
    auto gridID = m_resources.meshes.loadMesh(cro::CircleMeshBuilder(50.f, 30));
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);
    auto matID = m_resources.materials.add(m_resources.shaders.get(shaderID));
    
    m_tempTexture.loadFromFile("assets/images/batcat_diffuse.png");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -0.f, -0.f, -120.f });
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(gridID), m_resources.materials.get(matID));
    entity.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", cro::TextureID(m_tempTexture.getGLHandle()));

    auto camEnt = m_gameScene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&GameState::updateView, this, std::placeholders::_1);

    m_gameScene.enableSkybox();
}

void GameState::createUI()
{

}

void GameState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.projectionMatrix = glm::perspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;

    //update the UI camera to match the new screen size
    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport = cam3D.viewport;
}