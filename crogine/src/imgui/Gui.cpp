/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/gui/Gui.hpp>

#include "imgui.h"

using namespace cro;

void ui::begin(const std::string& title, bool* b)
{
    ImGui::Begin(title.c_str(), b);
}

void ui::checkbox(const std::string& title, bool* value)
{
    ImGui::Checkbox(title.c_str(), value);
}

void ui::slider(const std::string& title, float& value, float min, float max)
{
    ImGui::SliderFloat(title.c_str(), &value, min, max);
}

void ui::end()
{
    ImGui::End();
}

bool ui::wantsMouse()
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool ui::wantsKeyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}