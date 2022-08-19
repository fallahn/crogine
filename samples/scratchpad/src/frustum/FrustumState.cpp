/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "FrustumState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

FrustumState::FrustumState(cro::StateStack& stack, cro::State::Context context)
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
bool FrustumState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void FrustumState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool FrustumState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void FrustumState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void FrustumState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void FrustumState::loadAssets()
{

}

void FrustumState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/cube.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -5.f });
        md.createModel(entity);
    }



    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        auto windowSize = size;
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        //90 deg in x (glm expects fov in y)
        cam.setPerspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 140.f);
        cam.viewport.bottom = (1.f - size.y) / 2.f;
        cam.viewport.height = size.y;
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(camEnt.getComponent<cro::Camera>());
}

void FrustumState::createUI()
{
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of happiness"))
            {

            }
            ImGui::End();        
        });

    auto updateView = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_uiScene.getActiveCamera();
    camEnt.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(camEnt.getComponent<cro::Camera>());
}