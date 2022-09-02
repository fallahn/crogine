/*********************************************************************
(c) Matt Marchant 2022
http://trederia.blogspot.com

crogine - Zlib license.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.
*********************************************************************/

#include "MenuState.hpp"
#include "StateIDs.hpp"

#include <crogine/core/Log.hpp>
#include <crogine/gui/Gui.hpp>

MenuState::MenuState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx)
{
    if (m_font.loadFromFile("ProggyClean.ttf"))
    {
        m_text.setFont(m_font);
        m_text.setString("Plugin Works!!");
        m_text.setCharacterSize(64);
        m_text.setPosition({ 40.f, 140.f });
    }
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
	//prevents events being forwarded if the console wishes to consume them
	if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackClear(); //ALWAYS clear the stack first
            requestStackPush(0); //this should be whichever state ID you wish to return to in the application which loaded this plugin
            break;
        }
    }

    return true;
}

void MenuState::handleMessage(const cro::Message&)
{

}

bool MenuState::simulate(float)
{
    return true;
}

void MenuState::render()
{
    m_text.draw();
}

std::int32_t MenuState::getStateID() const
{
    return StateID::MainMenu;
}