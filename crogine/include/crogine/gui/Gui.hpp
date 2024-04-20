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

#pragma once

#include "detail/imgui.h"
#include "detail/imgui_stdlib.h"
#include "detail/ImGuizmo.h"
#include "detail/imfilebrowser.h"

#include <crogine/Config.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <string>

namespace cro
{
    /*!
    \brief Exposes a selection of ImGui functions to the public API.
    These can be used to create stand-alone windows or to add useful
    information to the status window via Console::registerConsoleTab().

    Honestly though I don't know why I thought this was a good idea.
    Just use DearImgui directly.
    */
    namespace ui
    {
        /*!
        \see ImGui::Begin()
        */
        [[deprecated("Use ImGui directly")]]
        CRO_EXPORT_API void begin(const std::string& title, bool* open = nullptr);

        /*!
        \see ImGui::CheckBox()
        */
        [[deprecated("Use ImGui directly")]]
        CRO_EXPORT_API void checkbox(const std::string& title, bool* value);

        /*!
        \see ImGui::FloatSlider()
        */
        [[deprecated("Use ImGui directly")]]
        CRO_EXPORT_API void slider(const std::string& title, float& value, float min, float max);

        /*!
        \see ImGui::End()
        */
        [[deprecated("Use ImGui directly")]]
        CRO_EXPORT_API void end();

        /*!
        \brief Returns true if the ui wants to capture mouse input
        */
        CRO_EXPORT_API bool wantsMouse();

        /*!
        \brief Returns true if the ui wants to capture keyboard input
        */
        CRO_EXPORT_API bool wantsKeyboard();
    }
}

#include <crogine/graphics/Texture.hpp>
namespace ImGui
{
    static inline void Image(const cro::Texture& texture, const ImVec2& size, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f })
    {
        ImGui::Image((void*)(std::size_t)texture.getGLHandle(), size, uv0, uv1);
    }

    static inline void Image(const cro::TextureID texture, const ImVec2& size, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f })
    {
        ImGui::Image((void*)(std::size_t)texture.textureID, size, uv0, uv1);
    }

    static inline bool ImageButton(const cro::Texture& texture, const ImVec2& size, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f })
    {
        return ImGui::ImageButton((void*)(std::size_t)texture.getGLHandle(), size, uv0, uv1);
    }

    static inline bool ImageButton(const cro::TextureID texture, const ImVec2& size, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f })
    {
        return ImGui::ImageButton((void*)(std::size_t)texture.textureID, size, uv0, uv1);
    }

    static inline bool ImageButton(std::uint32_t texture, const ImVec2& size, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f })
    {
        return ImGui::ImageButton((void*)(std::size_t)texture, size, uv0, uv1);
    }
}