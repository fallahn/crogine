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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

#include <array>
#include <functional>

namespace cro
{
    /*!
    \brief On-screen keyboard class
    Allows text input from controller or mouse events
    */
    class CRO_EXPORT_API OSK final
    {
    public:
        OSK();
        OSK(const OSK&) = delete;
        OSK(OSK&&) = delete;
        const OSK& operator = (const OSK&) = delete;
        OSK& operator = (OSK&&) = delete;


        /*!
        \brief Sets the on-screen keyboard visible, ready for input
        \param callback This callback is executed when the keyboard is
        closed. The callback parameters passed in indicate whether the
        input text was submitted (eg false if the input was cancelled)
        and a pointer to the utf8 encoded input buffer (null terminated)
        TODO this would be ideal for C++20s Ranges
        */
        static void show(const std::function<void(bool, const char*)>&);

    private:

        static constexpr std::uint32_t MaxChars = 2048;
        std::array<std::uint32_t, MaxChars> m_textBuffer = {}; //unicode codepoints
        std::size_t m_bufferIndex;

        std::uint32_t m_rowIndex;
        std::uint32_t m_colIndex;

        SDL_Keymod m_keymod; //toggled to switch between shifted and non-shifted layouts

        std::uint8_t m_controllerMask;
        std::uint8_t m_prevControllerMask;
        enum ControllerBits
        {
            Up = 0x1, Down = 0x2, Left = 0x4, Right = 0x8,
            L2 = 0x10, R2 = 0x20
        };


        bool m_isActive;
        std::function<void(bool, const char*)> m_callback;



        SimpleVertexArray m_keyboardArray;
        SimpleVertexArray m_textArray; //icons
        SimpleText m_previewText;
        Font m_previewFont;

        void close(bool isSubmitted);
        void updateVertices();
        bool keypress(SDL_Scancode);

        void moveLeft();
        void moveRight();
        void moveUp();
        void moveDown();
        void mouseClick(glm::vec2);

        friend class App;
        //returns true if the keyboard should consume the event
        bool handleEvent(const cro::Event&);
        void render();
    };
}