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
#include "Q3BspSystem.hpp"
#include "FpsCameraSystem.hpp"

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <crogine/core/App.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>

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
    context.mainWindow.setMouseCaptured(true);
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            getContext().mainWindow.setMouseCaptured(false);
            m_inputParser.setEnabled(false);
            break;
        }
    }
    else if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_TAB:
            getContext().mainWindow.setMouseCaptured(true);
            m_inputParser.setEnabled(true);
            break;
        }
    }

    m_inputParser.handleEvent(evt);
    m_scene.forwardEvent(evt);
	return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
{
    m_inputParser.update();
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

    m_scene.addSystem<FpsCameraSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<Q3BspSystem>(mb).loadMap("assets/maps/overkill.bsp");
}

void MenuState::loadAssets()
{

}

void MenuState::createScene()
{


    //add the resize callback to the camera so window change events
    //properly update the camera properties
    auto camEnt = m_scene.getActiveCamera();
    updateView(camEnt.getComponent<cro::Camera>());
    camEnt.getComponent<cro::Camera>().resizeCallback = std::bind(&MenuState::updateView, this, std::placeholders::_1);

    //add the first person controller
    camEnt.addComponent<FpsCamera>();
    m_inputParser.setEntity(camEnt);
}

void MenuState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    cam3D.projectionMatrix = glm::perspective(75.f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 4024.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;
}