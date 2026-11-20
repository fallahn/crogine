/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "HoleState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

HoleState::HoleState(cro::StateStack& stack, cro::State::Context context, const SharedStateData&)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus())
{
    cro::App::getWindow().setTitle("Super Video Golf - Hole Editor");
    cro::App::getInstance().setClearColour(cro::Colour::CornflowerBlue);

    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool HoleState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    //if (evt.type == SDL_EVENT_KEY_DOWN)
    //{
    //    switch (evt.key.key)
    //    {
    //    default: break;
    //    case SDLK_BACKSPACE:
    //    case SDLK_ESCAPE:
    //        requestStackClear();
    //        requestStackPush(0);
    //        break;
    //    }
    //}

    m_gameScene.forwardEvent(evt);
    return true;
}

void HoleState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool HoleState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    return true;
}

void HoleState::preRender()
{
    m_gameScene.preRender();
}
void HoleState::render()
{
    m_gameScene.render();
}

//private
void HoleState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
}

void HoleState::loadAssets()
{
}

void HoleState::createScene()
{
    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.8f, 2.f });
}

void HoleState::createUI()
{
    registerWindow([this]() 
        {
            drawMenuBar();
        
        });
}

void HoleState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Quit"))
            {
                if (!cro::FileSystem::showMessageBox("Save Settings", "Do you want to save first?", cro::FileSystem::YesNo))
                {
                    cro::App::quit();
                }
                else
                {
                    //showPresetSaveDialogue(OpenDialogueState::Quit);
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Treeset Editor"))
            {
                //TODO show save dialogue first
                requestStackClear();
                requestStackPush(StateID::EditorBush);
            }

            if (ImGui::MenuItem("Course Editor"))
            {
                //TODO show save dialogue first
                requestStackClear();
                requestStackPush(StateID::EditorCourse);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About");

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    //handleFileDialogues();
}