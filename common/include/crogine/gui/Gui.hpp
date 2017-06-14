/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_GUI_HPP_
#define CRO_GUI_HPP_

#include <crogine/Config.hpp>

#include <string>

namespace cro
{
    /*!
    \brief Exposes a selection of ImGui functions to the public API.
    These can be used to create stand-alone windows or to add useful
    information to the status window via App::registerStatusOutput().
    Note these functions are only effective if crogine is built with
    USE_IMGUI defined.
    */
    namespace Nim
    {
        /*!
        \see ImGui::Begin()
        */
        CRO_EXPORT_API void begin(const std::string& title, bool* open = nullptr);

        /*!
        \see ImGui::CheckBox()
        */
        CRO_EXPORT_API void checkbox(const std::string& title, bool* value);

        /*!
        \see ImGui::End()
        */
        CRO_EXPORT_API void end();
    }
}

#endif //CRO_GUI_HPP_