/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/TextureResource.hpp>

struct SharedStateData;
class KeyboardState final : public cro::State, public cro::GuiClient
{
public:
    KeyboardState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Keyboard; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;
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

    struct InputBuffer final
    {
        cro::Entity background;
        cro::Entity string;
    }m_inputBuffer;

    std::uint8_t m_axisFlags;
    std::uint8_t m_prevAxisFlags;

    struct TouchpadContext final
    {
        cro::Entity pointerEnt;
        glm::vec2 targetBounds = glm::vec2(0.f);
        glm::vec2 lastPosition = glm::vec2(0.f);
    }m_touchpadContext;
    
    
    struct AudioID final
    {
        enum
        {
            Move, Select,
            Space,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    void buildScene();
    void initCodepointCallbacks();
    void initBufferCallbacks();
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