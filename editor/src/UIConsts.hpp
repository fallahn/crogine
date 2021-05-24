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

#pragma once

#include <crogine/gui/Gui.hpp>
#include <crogine/detail/glm/vec2.hpp>

#include <cstdint>

namespace uiConst
{
    static constexpr float TitleHeight = 22.f;
    static constexpr float InspectorWidth = 0.25f; //percent of window width
    static constexpr float BrowserHeight = 0.32f;
    static constexpr float ThumbnailHeight = 0.45f; //percent of browser height
    static const glm::vec2 FramePadding = glm::vec2(4.f, 9.f); //how much to expand frame padding of thumbnails
    static constexpr float MaterialPreviewWidth = 0.45f;
    static constexpr float TexturePreviewWidth = 0.65f;
    static constexpr float TextBoxWidth = 0.915f;
    static constexpr float MinMaterialSlotSize = 20.f;
    static constexpr float ViewManipSize = 96.f;
    static constexpr float InfoBarHeight = 26.f;

    static constexpr std::size_t MaxMaterials = 99;

    static const cro::Colour PreviewClearColour = cro::Colour(0.f, 0.f, 0.f, 0.2f);
    static constexpr std::uint32_t PreviewTextureSize = 512u;

    static inline void showTipMessage(const std::string& message)
    {
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(450.0f);
            ImGui::TextUnformatted(message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    static inline void showToolTip(const std::string message)
    {
        ImGui::TextDisabled("(?)");
        showTipMessage(message);
    }
}