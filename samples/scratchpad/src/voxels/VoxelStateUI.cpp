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

#include "VoxelState.hpp"

#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    void showTip(const char* msg)
    {
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(450.0f);
            ImGui::TextUnformatted(msg);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}

void VoxelState::drawMenuBar()
{
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

            if (ImGui::MenuItem("Quit", nullptr, nullptr))
            {
                requestStackClear();
                requestStackPush(States::ScratchPad::MainMenu);
                saveSettings();
            }
            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Options", nullptr, nullptr))
            {

            }
            if (ImGui::MenuItem("Layers", nullptr, &m_showLayerWindow))
            {

            }
            ImGui::MenuItem("Brush", nullptr, &m_showBrushWindow);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void VoxelState::drawLayerWindow()
{
    if (m_showLayerWindow)
    {
        if (ImGui::Begin("Layers", &m_showLayerWindow))
        {
            if (ImGui::Checkbox("Show Water", &m_showLayer[Layer::Water]))
            {
                m_layers[Layer::Water].getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Water]);
            }

            if (ImGui::Checkbox("Show Terrain", &m_showLayer[Layer::Terrain]))
            {
                m_layers[Layer::Terrain].getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Terrain]);
            }

            if (ImGui::Checkbox("Show Voxels", &m_showLayer[Layer::Voxel]))
            {
                m_layers[Layer::Voxel].getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Voxel]);
            }

            const char* Labels[] =
            {
                "Water", "Terrain", "Course"
            };

            if (ImGui::BeginCombo("Active Layer", Labels[m_activeLayer]))
            {
                for (int n = 0; n < Layer::Count; n++)
                {
                    bool is_selected = (n == m_activeLayer);
                    if (ImGui::Selectable(Labels[n], is_selected))
                    {
                        m_activeLayer = n;
                        m_cursor.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", Voxel::LayerColours[n]);
                        m_cursor.getComponent<cro::Model>().setHidden(n == Layer::Water);
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
        ImGui::End();
    }
}

void VoxelState::drawBrushWindow()
{
    if (m_showBrushWindow)
    {
        if (ImGui::Begin("Brush Settings", &m_showBrushWindow))
        {
            ImGui::SliderFloat("Strength", &m_brush.strength, Voxel::BrushMinStrength, Voxel::BrushMaxStrength);
            ImGui::SliderFloat("Feather", &m_brush.feather, Voxel::BrushMinFeather, Voxel::BrushMaxFeather);
            showTip("Alt+[ or Alt+]");
            //ImGui::SliderFloat("Radius", ); //TODO move this here
            
            bool modeAdd = m_brush.editMode == Brush::EditMode::Add;
            bool modeSubtract = !modeAdd;

            if (ImGui::Checkbox("Add", &modeAdd))
            {
                modeSubtract = !modeAdd;
                m_brush.editMode = modeAdd ? Brush::EditMode::Add : Brush::EditMode::Subtract;
            }
            showTip("Keypad Plus");

            if (ImGui::Checkbox("Subtract", &modeSubtract))
            {
                modeAdd = !modeSubtract;
                m_brush.editMode = modeAdd ? Brush::EditMode::Add : Brush::EditMode::Subtract;
            }
            showTip("Keypad Minus");
        }
        ImGui::End();
    }
}

void VoxelState::handleKeyboardShortcut(const SDL_KeyboardEvent& evt)
{
    switch (evt.keysym.sym)
    {
    default: break;
    case SDLK_KP_PLUS:
        m_brush.editMode = Brush::EditMode::Add;
        break;
    case SDLK_KP_MINUS:
        m_brush.editMode = Brush::EditMode::Subtract;
        break;
    case SDLK_LEFTBRACKET:
        if (evt.keysym.mod & KMOD_ALT)
        {
            m_brush.feather = std::max(0.f, m_brush.feather - 0.1f);
        }
        else
        {
            float scale = m_cursor.getComponent<cro::Transform>().getScale().x;
            scale = std::max(Voxel::MinCursorScale, scale - Voxel::CursorScaleStep);
            m_cursor.getComponent<cro::Transform>().setScale(glm::vec3(scale));
        }
        break;
    case SDLK_RIGHTBRACKET:
        if (evt.keysym.mod & KMOD_ALT)
        {
            m_brush.feather = std::min(Voxel::BrushMaxFeather, m_brush.feather + 0.1f);
        }
        else
        {
            float scale = m_cursor.getComponent<cro::Transform>().getScale().x;
            scale = std::min(Voxel::MaxCursorScale, scale + Voxel::CursorScaleStep);
            m_cursor.getComponent<cro::Transform>().setScale(glm::vec3(scale));

        }
        break;
    }
}