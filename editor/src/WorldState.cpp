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

#include "WorldState.hpp"
#include "Messages.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include<array>

namespace
{
    enum WindowID
    {
        ModelBrowser,

        Count
    };

    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(), glm::vec2())
    };
}

WorldState::WorldState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_sharedData(sd),
    m_scene     (ctx.appInstance.getMessageBus())
{
    ctx.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        initUI();
        });
}

//public
bool WorldState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);
    return false;
}

void WorldState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
        }
    }
    else if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped)
        {
            if (data.id == States::ModelViewer)
            {
                initUI();
            }
        }
    }

    m_scene.forwardMessage(msg);
}

bool WorldState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void WorldState::render()
{
    auto& rw = getContext().mainWindow;
    m_scene.render(rw);
}

//private
void WorldState::loadAssets()
{

}

void WorldState::addSystems()
{

}

void WorldState::initUI()
{
    registerWindow(
        [&]()
        {
            //ImGui::ShowDemoWindow();
            if (ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open", nullptr, nullptr))
                    {
                        
                    }
                    if (ImGui::MenuItem("Save", nullptr, nullptr))
                    {
                        
                    }

                    if (ImGui::MenuItem("Save As...", nullptr, nullptr))
                    {
                        
                    }
                    if (ImGui::MenuItem("Import", nullptr, nullptr))
                    {
                        
                    }
                    if (ImGui::MenuItem("Quit", nullptr, nullptr))
                    {
                        cro::App::quit();
                    }
                    ImGui::EndMenu();
                }

                //view menu
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::MenuItem("Options", nullptr, nullptr))
                    {
                        
                    }
                    if (ImGui::MenuItem("Model Viewer/Importer", nullptr, nullptr))
                    {
                        requestStackPush(States::ModelViewer);
                        unregisterWindows();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }
        
            auto [pos, size] = WindowLayouts[WindowID::ModelBrowser];
            ImGui::SetNextWindowPos({ pos.x, pos.y });
            ImGui::SetNextWindowSize({ size.x, size.y });
            if (ImGui::Begin("ModelBrowser", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
            {

            }
            ImGui::End();
        });
    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void WorldState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::ModelBrowser] = std::make_pair(glm::vec2(0.f, ui::TitleHeight), glm::vec2(width * ui::InspectorWidth, height - ui::TitleHeight));
}