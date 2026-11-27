/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#pragma once

#include <crogine/gui/detail/imgui.h>

static constexpr std::size_t MaxProjectNameChars = 128; //maxmimum number of chars for the project name
static constexpr std::size_t MaxDescChar = 34; //maximum chars for title / desription strings
static constexpr float ToolBarSize = 20.f; //menu bar size at top of windows + 2px padding
static constexpr float ProjectPanelWidth = 300.f; //project properties panel
static constexpr std::size_t MaxAudioFileSize = 1000000;

//displays a tool tip when hovering an item
static inline void toolTip(const std::string& desc)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.f);
        ImGui::TextUnformatted(desc.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

//displays the tooltip but creates a ? icon to hover over
static inline void helpMarker(const std::string& desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    toolTip(desc);
}

//sets the fixed size for modal pop-ups
static inline void setModalSizeAndPos(glm::vec2 displaySize = glm::vec2(380.f, 170.f))
{
    const auto winSize = glm::vec2(cro::App::getWindow().getSize());
    const auto displayPos = (winSize - displaySize) / 2.f;

    ImGui::SetNextWindowSize({ displaySize.x, displaySize.y });
    ImGui::SetNextWindowPos({ displayPos.x, displayPos.y });
}