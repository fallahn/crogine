/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
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

#include "EditorApp.hpp"
#include "BushState.hpp"
#include "../StateIDs.hpp"
#include "../icon.hpp"

#include <crogine/core/Clock.hpp>

EditorApp::EditorApp()
    : m_stateStack({*this, getWindow()})
{
    setApplicationStrings("Trederia", "golf_editor");

    m_stateStack.registerState<BushState>(StateID::Bush, m_sharedData);
}

//public
void EditorApp::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_EVENT_KEY_UP)
    {
        switch (evt.key.key)
        {
        default: break;
#ifdef CRO_DEBUG_
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            App::quit();
#endif
            break;
        }
    }
    
    m_stateStack.handleEvent(evt);
}

void EditorApp::handleMessage(const cro::Message& msg)
{
    m_stateStack.handleMessage(msg);
}

void EditorApp::simulate(float dt)
{
    m_stateStack.simulate(dt);
}

void EditorApp::render()
{
    m_stateStack.render();
}

bool EditorApp::initialise()
{
    getWindow().setTitle("Super Video Golf - Editor");
    getWindow().setIcon(icon);

    cro::ConfigFile mntCfg;
    if (mntCfg.loadFromFile("assets/mount.cfg"))
    {
        for (const auto& obj : mntCfg.getObjects())
        {
            if (obj.getName() == "resource")
            {
                std::filesystem::path file;
                std::filesystem::path path;

                for (const auto& prop : obj.getProperties())
                {
                    const auto& name = prop.getName();
                    if (name == "file")
                    {
                        file = prop.getValue<std::u8string>();
                    }
                    else if (name == "mount_point")
                    {
                        path = prop.getValue<std::u8string>();
                    }
                }

                if (!file.empty())
                {
                    cro::IOResource::addPath(path / file, path);
                }
            }
        }
    }

    m_stateStack.pushState(StateID::Bush);

    return true;
}

void EditorApp::finalise()
{
    m_stateStack.clearStates();
    m_stateStack.simulate(0.f);
}