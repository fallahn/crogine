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

#include "GolfState.hpp"

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>


GolfState::GolfState(cro::StateStack& stack, cro::State::Context context)
    : cro::State(stack, context),
    m_gameScene(context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        loadAssets();
        addSystems();
        buildScene();
        });
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{

    m_gameScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
    m_gameScene.simulate(dt);

    return true;
}

void GolfState::render()
{
    m_renderTexture.clear();
    m_gameScene.render(m_renderTexture);
    m_renderTexture.display();

    m_quad.draw();

}

//private
void GolfState::loadAssets()
{
    m_gameScene.setCubemap("assets/images/skybox/sky.ccm");

    m_renderTexture.create(300, 200);
    m_quad.setTexture(m_renderTexture.getTexture());
    m_quad.setScale({ 3.f, 3.f });
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void GolfState::buildScene()
{
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();

    cro::ModelDefinition modelDef(m_resources);
    modelDef.loadFromFile("assets/models/golf_course.cmt");
    modelDef.createModel(entity);

    auto updateView = [](cro::Camera& cam)
    {
        cam.setPerspective(60.f * cro::Util::Const::degToRad, 300.f/200.f, 0.1f, 300.f);
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 2.f, 10.f });
    camEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -cro::Util::Const::PI / 8.f);

    auto& cam = camEnt.getComponent<cro::Camera>();
    updateView(cam);
    cam.resizeCallback = updateView;
}