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
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

namespace
{
    constexpr glm::vec2 ViewSize(1920.f, 1080.f);
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
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_scene.getSystem<cro::UISystem>().handleEvent(evt);

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
    m_scene.render(cro::App::getWindow());
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
}

void MenuState::loadAssets()
{
    m_font.loadFromFile("assets/fonts/VeraMono.ttf");
}

void MenuState::createScene()
{
    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, -1.f));
    entity.addComponent<cro::Drawable2D>().getVertexData() = 
    {
        cro::Vertex2D(glm::vec2(0.f, ViewSize.y), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(ViewSize), cro::Colour::CornflowerBlue),
        cro::Vertex2D(glm::vec2(ViewSize.x, 0.f), cro::Colour::CornflowerBlue)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();



    //menu
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(200.f, 960.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Scratchpad");
    entity.getComponent<cro::Text>().setCharacterSize(80);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
    entity.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
    entity.getComponent<cro::Text>().setOutlineThickness(1.5f);

    auto& uiSystem = m_scene.getSystem<cro::UISystem>();
    auto selected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Red);
        });
    auto unselected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
        });

    glm::vec2 textPos(200.f, 800.f);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(textPos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Batcat");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
    entity.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
    entity.getComponent<cro::Text>().setOutlineThickness(1.f);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {
            
            
            });
    textPos.y -= 40.f;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(textPos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("BSP");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
    entity.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
    entity.getComponent<cro::Text>().setOutlineThickness(1.f);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {


            });
    textPos.y -= 40.f;


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(textPos);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_font).setString("Reflection / Refraction");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
    entity.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
    entity.getComponent<cro::Text>().setOutlineThickness(1.f);
    entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {


            });
    textPos.y -= 40.f;


    //camera
    auto updateCam = [&](cro::Camera& cam)
    {
        static constexpr float viewRatio = ViewSize.x / ViewSize.y;

        cam.setOrthographic(0.f, ViewSize.x, 0.f, ViewSize.y, -0.1f, 2.f);

        glm::vec2 size(cro::App::getWindow().getSize());
        auto sizeRatio = size.x / size.y;

        if (sizeRatio > viewRatio)
        {
            cam.viewport.width = viewRatio / sizeRatio;
            cam.viewport.left = (1.f - cam.viewport.width) / 2.f;

            cam.viewport.height = 1.f;
            cam.viewport.bottom = 0.f;
        }
        else
        {
            cam.viewport.width = 1.f;
            cam.viewport.left = 0.f;

            cam.viewport.height = sizeRatio / viewRatio;
            cam.viewport.bottom = (1.f - cam.viewport.height) / 2.f;
        }
    };

    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateCam;
    updateCam(cam);
}