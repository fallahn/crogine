/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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

#include <crogine/detail/Types.hpp>
#include <crogine/gui/Gui.hpp>

#include <string>

struct SharedStateData;

struct HelpNav final
{
    std::int32_t chapterCount = 0;
    std::int32_t scrollIndex = 0;
    std::int32_t targetIndex = 0;

    std::int32_t selectedScroll = 0;

    bool wantsScroll = false;

    float manualScroll = 0.f;
    float currTime = 0.f;
    static constexpr float ScrollTime = 0.025f;
    static constexpr float ScrollAmount = 12.f;
};

static inline void showToolTip(const std::string& msg)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(msg.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

//called from main golf game handler to close active windows
bool handleTopLevelEvent(const cro::Event&, SharedStateData&, HelpNav&);