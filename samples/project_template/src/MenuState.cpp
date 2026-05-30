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

#include <crogine/core/App.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

namespace
{

}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
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

    context.appInstance.setClearColour(cro::Colour(0.2f, 0.2f, 0.26f));
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
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

    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void MenuState::loadAssets()
{

}

void MenuState::createScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/models/bust.cmt"))
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float dt)
            {
                e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
                e.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, dt);
                e.getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, dt);
            };

        auto callback = [](cro::Camera& cam)
            {
                glm::vec2 size(cro::App::getWindow().getSize());
                cam.viewport = { 0.f, 0.f, 1.f, 1.f };
                cam.setPerspective(60.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 150.f);
            };
        m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
        m_scene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = callback;
        callback(m_scene.getActiveCamera().getComponent<cro::Camera>());
    }


    registerWindow(
        [this]()
        {
            if (ImGui::Begin("Info"))
            {
                int maj, min;
                SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &maj);
                SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &min);
                ImGui::Text("OpenGL Version: %d.%d", maj, min);
                ImGui::Text("Vendor: %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
                const auto size = cro::App::getWindow().getSize();
                ImGui::Text("Window size: %u, %u", size.x, size.y);
            }
            ImGui::End();
        });
}
