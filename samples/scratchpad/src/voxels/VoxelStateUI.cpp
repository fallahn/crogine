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

#include <crogine/gui/Gui.hpp>

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
            static bool showWater = false;
            if (ImGui::Checkbox("Show Water", &showWater))
            {

            }

            static bool showTerrain = false;
            if (ImGui::Checkbox("Show Terrain", &showTerrain))
            {

            }

            static bool showVoxels = false;
            if (ImGui::Checkbox("Show Voxels", &showVoxels))
            {

            }
        }
        ImGui::End();
    }
}