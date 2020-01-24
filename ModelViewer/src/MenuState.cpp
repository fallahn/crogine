/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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
#include <crogine/gui/imgui.h>

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
        //windowing
        buildUI();
    });

    context.appInstance.resetFrameTime();

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

bool MenuState::simulate(cro::Time dt)
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


}

void MenuState::loadAssets()
{

}

void MenuState::createScene()
{

}

void MenuState::buildUI()
{
    registerWindow([]()
        {
            if(ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File"))
                {
                    ImGui::MenuItem("Open Model", nullptr, nullptr);
                    ImGui::MenuItem("Import Model", nullptr, nullptr);
                    ImGui::MenuItem("Export Model", nullptr, nullptr);
                    ImGui::MenuItem("Quit", nullptr, nullptr);
                    ImGui::EndMenu();
                }

                //view menu
                if (ImGui::BeginMenu("View"))
                {
                    ImGui::MenuItem("Options", nullptr, nullptr);
                    ImGui::MenuItem("Animation Data", nullptr, nullptr);
                    ImGui::MenuItem("Material Data", nullptr, nullptr);
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }
        });
}