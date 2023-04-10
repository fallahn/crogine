/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "SSAOState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

SSAOState::SSAOState(cro::StateStack& stack, cro::State::Context context)
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
bool SSAOState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void SSAOState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SSAOState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SSAOState::render()
{
    m_renderBuffer.clear(cro::Colour::Plum);
    m_gameScene.render();
    m_renderBuffer.display();


    m_colourQuad.draw();
    m_depthQuad.draw();

    m_uiScene.render();
}

//private
void SSAOState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void SSAOState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
}

void SSAOState::createScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/ssao/cart/cart.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt * 0.5f);
        };
    }

    if (md.loadFromFile("assets/ssao/background/background.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
    }

    auto resizeWindow = [&](cro::Camera& cam)
    {
        auto buffSize = cro::App::getWindow().getSize();
        auto size = glm::vec2(buffSize);

        //m_renderBuffer.create(buffSize.x / 2u, buffSize.y / 2u);
        //m_colourQuad.setTexture(m_renderBuffer.getTexture(0), m_renderBuffer.getSize());
        m_renderBuffer.create({ buffSize.x / 2u, buffSize.y / 2u, true, true, false, 4 });
        m_colourQuad.setTexture(m_renderBuffer.getTexture());
        m_colourQuad.setPosition({ 0.f, size.y / 2.f });

        m_depthQuad.setTexture(m_renderBuffer.getDepthTexture(), m_renderBuffer.getSize());

        cam.setPerspective(0.7f, size.x / size.y, 2.1f, 7.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = resizeWindow;
    camEnt.getComponent<cro::Camera>().shadowMapBuffer.create(2048, 2048);
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 2.f, 3.878f });
    camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, -0.254f);
    resizeWindow(camEnt.getComponent<cro::Camera>());

    auto sun = m_gameScene.getSunlight();
    sun.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -0.3f);
    sun.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.4f);
}

void SSAOState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Cart of buns"))
            {
                /*static glm::vec3 pos(0.f, 0.5f, 3.f);
                if (ImGui::SliderFloat3("Position", &pos[0], -10.f, 10.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition(pos);
                }

                static float rotation = 0.f;
                if (ImGui::SliderFloat("Rotation", &rotation, -2.f, 2.f))
                {
                    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setRotation(cro::Transform::X_AXIS, rotation);
                }*/
            }
            ImGui::End();        
        });
}