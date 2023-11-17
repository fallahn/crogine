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

#include <crogine/gui/GuiClient.hpp>
#include <crogine/core/App.hpp>

using namespace cro;

GuiClient::~GuiClient()
{
    if (m_wantsTabsRemoving)
    {
        App::removeConsoleTab(this);
    }

    if (m_wantsWindowsRemoving)
    {
        App::removeWindows(this);
    }
}

//public
void GuiClient::registerConsoleTab(const std::string& name, const std::function<void()>& f) const
{
    App::addConsoleTab(name, f, this);
    m_wantsTabsRemoving = true;
}

void GuiClient::registerWindow(const std::function<void()>& f, bool isDebug) const
{
    App::addWindow(f, this, isDebug);
    m_wantsWindowsRemoving = true;

    if (isDebug && !Console::getConvarValue<bool>("drawDebugWindows"))
    {
        LogW << "Registered window with isDebug flag set to true, but r_drawDebugWindows is currently false." << std::endl;
    }
}

void GuiClient::unregisterWindows() const
{
    App::removeWindows(this);
    m_wantsWindowsRemoving = false;
}

void GuiClient::unregisterConsoleTabs() const
{
    App::removeConsoleTab(this);
    m_wantsTabsRemoving = false;
}