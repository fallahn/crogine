/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

//const values used when building the menus
const cro::Colour textColourSelected(1.f, 0.77f, 0.f);
const cro::Colour textColourNormal = cro::Colour::White;
const float buttonIconOffset = 156.f;

const std::uint32_t TextXL = 80;
const std::uint32_t TextLarge = 60;
const std::uint32_t TextMedium = 42;
const std::uint32_t TextSmall = 32;

const cro::Colour stateBackgroundColour(0.f, 0.f, 0.f, 0.6f);

const std::string highscoreFile("high.scores");

static inline bool activated(const cro::ButtonEvent& evt)
{
    switch (evt.type)
    {
    default: return false;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
        return evt.button.button == SDL_BUTTON_LEFT;
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERBUTTONDOWN:
        return evt.cbutton.button == SDL_CONTROLLER_BUTTON_A;
    case SDL_FINGERUP:
    case SDL_FINGERDOWN:
        return true;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return (evt.key.keysym.sym == SDLK_SPACE || evt.key.keysym.sym == SDLK_RETURN);
    }
}