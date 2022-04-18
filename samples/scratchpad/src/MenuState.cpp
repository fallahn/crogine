/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2022
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

#include <fstream>
#include <iomanip>

using namespace sp;

namespace
{
    constexpr glm::vec2 ViewSize(1920.f, 1080.f);
    constexpr float MenuSpacing = 40.f;

    bool activated(const cro::ButtonEvent& evt)
    {
        switch (evt.type)
        {
        default: return false;
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            return evt.button.button == SDL_BUTTON_LEFT;
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERBUTTONDOWN:
            return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
        case SDL_FINGERUP:
        case SDL_FINGERDOWN:
            return true;
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            return (evt.key.keysym.sym == SDLK_SPACE || evt.key.keysym.sym == SDLK_RETURN);
        }
    }
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

#ifdef CRO_DEBUG_
    registerWindow([&]() 
        {
            if (ImGui::Begin("Window of Woo"))
            {
                static std::string label("No file open");
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Open video"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "mpg");
                    if (!path.empty())
                    {
                        if (!m_video.loadFromFile(path))
                        {
                            cro::FileSystem::showMessageBox("Error", "Could not open file");
                            label = "No file open";
                        }
                        else
                        {
                            label = cro::FileSystem::getFileName(path);
                        }
                    }
                }

                ImGui::Image(m_video.getTexture(), { 352.f, 288.f }, { 0.f, 0.f }, { 1.f, 1.f });

                if (ImGui::Button("Play"))
                {
                    m_video.play();
                }
                ImGui::SameLine();
                if (ImGui::Button("Pause"))
                {
                    m_video.pause();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop"))
                {
                    m_video.stop();
                }
                ImGui::SameLine();
                auto looped = m_video.getLooped();
                if (ImGui::Checkbox("Loop", &looped))
                {
                    m_video.setLooped(looped);
                }

                ImGui::Text("%3.3f / %3.3f", m_video.getPosition(), m_video.getDuration());

                ImGui::SameLine();
                if (ImGui::Button("Jump"))
                {
                    m_video.seek(100.f);
                }
            }
            ImGui::End();
        });
#endif
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);

    m_scene.forwardEvent(evt);
    return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(float dt)
{
    m_video.update(dt);

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

    auto* uiSystem = m_scene.getSystem<cro::UISystem>();
    auto selected = uiSystem->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Red);
        });
    auto unselected = uiSystem->addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
        });


    auto createButton = [&](const std::string label, glm::vec2 position)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(position);
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(m_font).setString(label);
        e.getComponent<cro::Text>().setFillColour(cro::Colour::Plum);
        e.getComponent<cro::Text>().setOutlineColour(cro::Colour::Teal);
        e.getComponent<cro::Text>().setOutlineThickness(1.f);
        e.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;

        return e;
    };

    //batcat button
    glm::vec2 textPos(200.f, 800.f);
    entity = createButton("Batcat", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::BatCat);
                }
            });

    //mesh collision button
    textPos.y -= MenuSpacing;
    entity = createButton("Mesh Collision", textPos);    
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::MeshCollision);
                }
            });

    //BSP button
    textPos.y -= MenuSpacing;
    entity = createButton("BSP", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::BSP);
                }
            });

    //voxels button
    textPos.y -= MenuSpacing;
    entity = createButton("Voxels", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Voxels);
                }
            });

    //billiards button
    textPos.y -= MenuSpacing;
    entity = createButton("Billiards", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(States::ScratchPad::Billiards);
                }
            });


    //quit button
    textPos.y -= MenuSpacing * 2.f;
    entity = createButton("Quit", textPos);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });




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