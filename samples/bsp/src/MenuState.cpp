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
#include <crogine/util/Random.hpp>

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

    registerWindow([&]()
        {
            if (ImGui::Begin("Window of Buns"))
            {
                ImGui::Image(m_reflectionMap.getTexture(), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
                ImGui::Image(m_refractionMap.getTexture(), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
            }
            ImGui::End();
        });
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    /*if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }*/

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
    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    auto oldVp = cam.viewport;
    cam.viewport = { 0.f,0.f,1.f,1.f };

    cam.setActivePass(cro::Camera::Pass::Reflection);
    m_reflectionMap.clear(cro::Colour::Red());
    m_scene.render(m_reflectionMap);
    m_reflectionMap.display();


    cam.setActivePass(cro::Camera::Pass::Refraction);
    m_refractionMap.clear(cro::Colour::Blue());
    m_scene.render(m_refractionMap);
    m_reflectionMap.display();


    //draw any renderable systems
    cam.setActivePass(cro::Camera::Pass::Final);
    cam.viewport = oldVp;
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
    const std::uint32_t MapSize = 512;
    m_reflectionMap.create(MapSize, MapSize);
    m_reflectionMap.setSmooth(true);

    m_refractionMap.create(MapSize, MapSize);
    m_refractionMap.setSmooth(true);
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

    const auto& spawnPoints = m_scene.getSystem<Q3BspSystem>().getSpawnPoints();

    if (spawnPoints.size() > 1)
    {
        const auto& spawn = spawnPoints[cro::Util::Random::value(0, spawnPoints.size() - 1)];
        camEnt.getComponent<cro::Transform>().setPosition(spawn.position);
        camEnt.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, -(cro::Util::Const::degToRad * spawn.rotation));
    }
    else
    {
        camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 256.f });
    }
}

void MenuState::updateView(cro::Camera& cam3D)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    //90 deg in x (glm expects fov in y)
    cam3D.projectionMatrix = glm::perspective(50.6f * cro::Util::Const::degToRad, 16.f / 9.f, 0.1f, 4024.f);
    cam3D.viewport.bottom = (1.f - size.y) / 2.f;
    cam3D.viewport.height = size.y;
}