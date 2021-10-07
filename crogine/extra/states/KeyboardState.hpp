/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/graphics/TextureResource.hpp>

/*
Creates an on-screen keyboard for text input with a game controller.

To allow text input with a game controller, first active text input with
SDL_StartTextInput() then push this state on to the top of the StateStack.
All game controller events are consumed by this state, and all other events
are forwarded to the states below.

Activating a keyboard input with the controller raises a new SDL_TextInput
event which can be handled in the exact same way as any text input,
making this state compatible with any existing text input code.

Pressing the Start button on the controller will quit this state, at which
point you should call SDL_StoTextInput(). The state will also be popped if
it registers a CONTROLLER_REMOVED event.

To use the state in your crogine project make sure your StateID enum contains
a Keyboard member (or adjust getStateID(), below, accordingly). The state will
also need to be registered with all the other States in the App::initialise()
function. You must also include the osk.png/osk.spt files in your project, which
are used to render the keyboard.

While the KeyboardState as-is may appear limited outside of the western alphabet
it theoretically supports all utf-8 characters supported by SDL. The keyboard 
may be extended by modifying the osk.png keyboard graphic, and registering new
character callbacks in initCallbacks(). For reference see 
https://www.charset.org/utf-8 for a list of utf-8 hex codes.

*/


class KeyboardState final : public cro::State
{
public:
    KeyboardState(cro::StateStack&, cro::State::Context);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Keyboard; }

private:

    cro::Scene m_scene;
    cro::TextureResource m_textures;
    cro::Entity m_keyboardEntity;
    cro::Entity m_highlightEntity;

    struct ButtonID final
    {
        enum
        {
            Backspace, Space, Shift,
            Count
        };
    };
    std::array<cro::Entity, ButtonID::Count> m_buttonEnts = {};

    struct KeyboardLayout final
    {
        cro::FloatRect bounds;
        std::array<std::function<void()>, 30> callbacks = {};

        enum
        {
            Lower, Upper, Symbol,
            Count
        };
    };
    std::array<KeyboardLayout, KeyboardLayout::Count> m_keyboardLayouts = {};
    std::size_t m_activeLayout;
    std::int32_t m_selectedIndex;

    void buildScene();
    void initCallbacks();
    void quitState();

    void setCursorPosition();
    void left();
    void right();
    void up();
    void down();
    void activate();

    void nextLayout();
    void sendKeystroke(std::int32_t);
    void sendBackspace(); //have to make this separate to be compatible with callbacks...
    void sendSpace();
};