/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#include <crogine/core/App.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/core/OSK.hpp>
#include <crogine/core/Utf.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>

#include <crogine/graphics/Colour.hpp>

using namespace cro;

namespace
{
    constexpr Colour BGColour = Colour(std::uint8_t(35), 38, 46);
    constexpr Colour ButtonColourNormal = Colour(std::uint8_t(14), 20, 27);
    constexpr Colour ButtonColourActive = Colour(std::uint8_t(255), 255, 255);
    constexpr Colour SpecialButtonNormal = Colour(std::uint8_t(0), 0, 0);
    constexpr Colour SpecialButtonActive = Colour(std::uint8_t(26), 159, 255);
    constexpr Colour TextColourNormal = ButtonColourActive;
    constexpr Colour TextColourActive = ButtonColourNormal;
    constexpr Colour TextColourShift = Colour(std::uint8_t(123), 126, 130); //shifted text icons EG number row when shift not active

    //we assign a scancode to each of the virtual keys
    //that way we automatically display the correct keys
    //based on the user's layout as well as render different
    //characters is chift is locked.
    struct KeyInfo final
    {
        constexpr KeyInfo() {};
        constexpr KeyInfo(SDL_Scancode c) : scancode(c) {}
        SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
        bool active = false;
    };

    constexpr std::array<std::array<KeyInfo, 14u>, 5u> LowerCaseKeys =
    {
        std::array<KeyInfo, 14u>{KeyInfo(SDL_SCANCODE_GRAVE),KeyInfo(SDL_SCANCODE_1),KeyInfo(SDL_SCANCODE_2),KeyInfo(SDL_SCANCODE_3),KeyInfo(SDL_SCANCODE_4),KeyInfo(SDL_SCANCODE_5),KeyInfo(SDL_SCANCODE_6),KeyInfo(SDL_SCANCODE_7),KeyInfo(SDL_SCANCODE_8),KeyInfo(SDL_SCANCODE_9),KeyInfo(SDL_SCANCODE_0),KeyInfo(SDL_SCANCODE_MINUS),KeyInfo(SDL_SCANCODE_EQUALS),KeyInfo(SDL_SCANCODE_BACKSPACE)},
        {KeyInfo(SDL_SCANCODE_TAB),KeyInfo(SDL_SCANCODE_Q),KeyInfo(SDL_SCANCODE_W),KeyInfo(SDL_SCANCODE_E),KeyInfo(SDL_SCANCODE_R),KeyInfo(SDL_SCANCODE_T),KeyInfo(SDL_SCANCODE_Y),KeyInfo(SDL_SCANCODE_U),KeyInfo(SDL_SCANCODE_I),KeyInfo(SDL_SCANCODE_O),KeyInfo(SDL_SCANCODE_P),KeyInfo(SDL_SCANCODE_LEFTBRACKET),KeyInfo(SDL_SCANCODE_RIGHTBRACKET),KeyInfo(SDL_SCANCODE_BACKSLASH)},
        {KeyInfo(SDL_SCANCODE_CAPSLOCK),KeyInfo(SDL_SCANCODE_A),KeyInfo(SDL_SCANCODE_S),KeyInfo(SDL_SCANCODE_D),KeyInfo(SDL_SCANCODE_F),KeyInfo(SDL_SCANCODE_G),KeyInfo(SDL_SCANCODE_H),KeyInfo(SDL_SCANCODE_J),KeyInfo(SDL_SCANCODE_K),KeyInfo(SDL_SCANCODE_L),KeyInfo(SDL_SCANCODE_SEMICOLON),KeyInfo(SDL_SCANCODE_APOSTROPHE),KeyInfo(SDL_SCANCODE_RETURN),KeyInfo()},
        {KeyInfo(SDL_SCANCODE_LSHIFT),KeyInfo(SDL_SCANCODE_Z),KeyInfo(SDL_SCANCODE_X),KeyInfo(SDL_SCANCODE_C),KeyInfo(SDL_SCANCODE_V),KeyInfo(SDL_SCANCODE_B),KeyInfo(SDL_SCANCODE_N),KeyInfo(SDL_SCANCODE_M),KeyInfo(SDL_SCANCODE_COMMA),KeyInfo(SDL_SCANCODE_PERIOD),KeyInfo(SDL_SCANCODE_SLASH),KeyInfo(SDL_SCANCODE_RSHIFT),KeyInfo(),KeyInfo()},
        {KeyInfo(SDL_SCANCODE_SPACE),KeyInfo(SDL_SCANCODE_LEFT),KeyInfo(SDL_SCANCODE_RIGHT),KeyInfo(/*would be switch to emoji*/),KeyInfo(/*would be Paste*/),KeyInfo(/*would be quit*/),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()}
    };

    //TODO do we need these or can the output of scancodes be inferred by shift status?
    //constexpr std::array<std::array<KeyInfo, 14u>, 5u> UpperCaseKeys =
    //{
    //    std::array<KeyInfo, 14u>{KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()},
    //    {KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo(),KeyInfo()}
    //};


    constexpr std::uint32_t BasePreviewTextSize = 12; //gets scaled based on screen size
}

//public
OSK::OSK()
    : m_bufferIndex (0),
    m_isActive      (false),
    m_keymod        (0)
{
    m_keyboardArray.setPrimitiveType(GL_TRIANGLES);
    m_textArray.setPrimitiveType(GL_TRIANGLES);

    if (m_previewFont.loadFromFile("assets/fonts/VeraMono.ttf"))
    {
        //TODO append other fonts such as CJK and emoji



        m_previewText.setFont(m_previewFont);
    }
}

void OSK::show(const std::function<void(bool, const char*)>& callback)
{
    auto& instance = App::getInstance().m_osk;

    if (!instance->m_isActive)
    {
        instance->m_isActive = true;
        instance->m_callback = callback;
        instance->m_bufferIndex = 0;

        instance->updateVertices();
    }
}


//private
void OSK::close(bool isSubmitted)
{
    if (m_isActive)
    {
        if (m_callback)
        {
            std::basic_string<char> output;
            output.reserve(m_textBuffer.size());
            Utf32::toUtf8(m_textBuffer.begin(), m_textBuffer.end(), std::back_inserter(output));

            m_callback(isSubmitted, output.data());
        }

        //reset the buffer for next input
        //if (isSubmitted)
        {
            std::fill(std::begin(m_textBuffer), std::end(m_textBuffer), 0);
            m_bufferIndex = 0;

            m_previewText.setString(" ");
        }

        m_isActive = false;
    }
}

void OSK::updateVertices()
{
    const auto WindowSize = glm::vec2(App::getWindow().getSize());
    std::vector<Vertex2D> verts;

    //background
    const float BGHeight = std::floor(WindowSize.y / 3.f);
    verts.emplace_back(glm::vec2(0.f, BGHeight), BGColour);
    verts.emplace_back(glm::vec2(0.f), BGColour);
    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight), BGColour);

    verts.emplace_back(glm::vec2(WindowSize.x, BGHeight), BGColour);
    verts.emplace_back(glm::vec2(0.f), BGColour);
    verts.emplace_back(glm::vec2(WindowSize.x, 0.f), BGColour);


    const auto Scale = UIElementSystem::getViewScale();
    //TODO calc padding and char size based on scale

    //TODO total text area width should be 4:3 ratio of the current window height

    m_keyboardArray.setVertexData(verts);


    m_previewText.setCharacterSize(BasePreviewTextSize * static_cast<std::uint32_t>(Scale));
}

bool OSK::keypress(SDL_Scancode code)
{
    const auto k = SDL_GetKeyFromScancode(code, m_keymod, false);
    if ((k & (SDLK_EXTENDED_MASK | SDLK_SCANCODE_MASK)) == 0)
    {
        switch (k)
        {
        default:
            if (m_bufferIndex < MaxChars)
            {
                m_textBuffer[m_bufferIndex++] = k;
                m_previewText.setString(String::fromUtf32(m_textBuffer.begin(), m_textBuffer.begin() + m_bufferIndex));

                return true;
            }
            return false;
        case SDLK_BACKSPACE:
            if (m_bufferIndex > 0)
            {
                m_textBuffer[--m_bufferIndex] = 0;
                m_previewText.setString(String::fromUtf32(m_textBuffer.begin(), m_textBuffer.begin() + m_bufferIndex));
                return true;
            }
            return false;

        case SDLK_RETURN:
        case SDLK_RETURN2:
            close(true);
            return false;
        }
    }
    return false;
}

bool OSK::handleEvent(const Event& evt)
{
    if (!m_isActive)
    {
        return false;
    }

    switch (evt.type)
    {
    default: return false;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:

        return m_isActive;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        switch (evt.gbutton.button)
        {
        default: break;
        case GameController::ButtonB:
            close(false);
            break;
        }
        return m_isActive;


    case SDL_EVENT_KEY_UP:
        switch (evt.key.key)
        {
        default: break;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            m_keymod = SDL_KMOD_NONE;
            break;
        case SDLK_ESCAPE:
            close(false);
            break;
        }
        return m_isActive;
    case SDL_EVENT_KEY_DOWN:
    {
        switch (evt.key.key)
        {
        default: break;;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            m_keymod = SDL_KMOD_SHIFT;
            updateVertices();
            return true;
        }
        keypress(evt.key.scancode);
    }
    return true;


    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            close(false);
        }
        return m_isActive;



    case SDL_EVENT_WINDOW_RESIZED:
        updateVertices();
        return false;
    }
}

void OSK::render()
{
    if (m_isActive)
    {
        m_keyboardArray.draw();
        m_textArray.draw();

        m_previewText.draw();
    }
}