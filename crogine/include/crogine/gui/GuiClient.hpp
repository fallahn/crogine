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

#include <crogine/Config.hpp>

#include <functional>
#include <string>

namespace cro
{
    /*!
    \brief Allows registering GUI controls with the default output
    window, or registering custom windows.
    Classes which inherit this interface may register controls with
    the imgui renderer. This is usually used for debugging output or
    when creating tooling.
    */
    class CRO_EXPORT_API GuiClient
    {
    public:
        GuiClient() = default;
        virtual ~GuiClient();
        GuiClient(const GuiClient&) = default;
        GuiClient(GuiClient&&) = default;
        GuiClient& operator = (const GuiClient&) = default;
        GuiClient& operator = (GuiClient&&) = default;

        /*!
        \brief Registers one or more gui controls with the console window.
        The given function should include the ui/ImGui functions as they would
        appear between the Begin() and End() commands *without* Begin() and End()
        themselves. These controls will then appear in a new tab in the console window
        all the time the object which inherits this interface exists.
        \param name Title to give the new tab
        \param function Gui function to render the contents of the tab
        */
        void registerConsoleTab(const std::string& name, const std::function<void()>&) const;

        /*!
        \brief Registers a custom window with the ImGui renderer.
        The given function should include the Begin() and End() calls to create a
        fully stand-alone window with ImGui. The window will exist all the time
        the object which inherits this interface exists.
        \param windowFunc A std::function (or lambda) used to draw the ImGui window
        \param isDebug If true debug windows will only be drawn if r_drawDebugWindow is true
        */
        void registerWindow(const std::function<void()>& windowFunc, bool isDebug = false) const;

        /*!
        \brief Removes all ImGui windows currently registered by this state
        */
        void unregisterWindows() const;

        /*!
        \brief Removes all console tabs currently registered by this state
        */
        void unregisterConsoleTabs() const;

    private:
        mutable bool m_wantsWindowsRemoving = true;
        mutable bool m_wantsTabsRemoving = true;
    };
}