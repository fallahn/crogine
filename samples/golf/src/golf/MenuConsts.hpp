/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include <crogine/core/Clock.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/detail/glm/vec2.hpp>

#ifdef USE_GNS
//#define IS_PS(x) Input::isPSController(x)
#define IS_PS(x) cro::GameController::hasPSLayout(x)
#else
#define IS_PS(x) cro::GameController::hasPSLayout(x)
#endif

struct FontID final
{
    enum
    {
        UI,
        Info,
        Label,

        Count
    };
};

static const std::array<std::string, 3u> ScoreTypes =
{
    "Stroke Play", "Match Play", "Skins"
};

static const std::array<std::string, 3u> ScoreDesc =
{
    "Player with the fewest total\nstrokes wins",
    "Holes are scored individually.\nPlayer with the most holes\nwins",
    "Holes are scored individually.\nWinner of the hole gets the\nskins pot, else the pot\nrolls over to the next hole."
};

static const std::array<std::string, 3u> GimmeString =
{
    "None",
    "Inside the leather",
    "Inside the putter"
};

static constexpr std::array<glm::vec3, 4u> EmotePositions =
{
    glm::vec3(0.f, 34.f, 0.15f),
    glm::vec3(34.f, 0.f, 0.15f),
    glm::vec3(0.f, -34.f, 0.15f),
    glm::vec3(-34.f, 0.f, 0.15f)
};

static constexpr std::uint32_t LargeTextSize = 64;
static constexpr std::uint32_t MediumTextSize = 32;
static constexpr std::uint32_t SmallTextSize = 16;

static constexpr std::uint32_t UITextSize = 8;
static constexpr std::uint32_t InfoTextSize = 10;
static constexpr std::uint32_t LabelTextSize = 16;

static const cro::Colour TextNormalColour(0xfff8e1ff);
static const cro::Colour TextEditColour(0x6eb39dff);
static const cro::Colour TextHighlightColour(0xb83530ff); //red
//static const cro::Colour TextHighlightColour(0x005ab5ff); //cb blue
//static const cro::Colour TextHighlightColour(0x006cd1ff); //cb blue 2
static const cro::Colour TextGoldColour(0xf2cf5cff);
static const cro::Colour TextOrangeColour(0xec773dff);
static const cro::Colour TextGreenColour(0x6ebe70ff);
static const cro::Colour LeaderboardTextDark(0x1a1e2dff);
static const cro::Colour LeaderboardTextLight(0xfff8e1ff);

static constexpr float LeaderboardTextSpacing = 6.f;
static constexpr float BackgroundAlpha = 0.7f;
static constexpr glm::vec2 LobbyTextRootPosition(8.f, 172.f);

static constexpr float UIBarHeight = 16.f;
static constexpr float UITextPosV = 12.f;
static constexpr glm::vec3 CursorOffset(-20.f, 4.f, 0.f);

//ui components are laid out as a normalised value
//relative to the window size.
struct UIElement final
{
    glm::vec2 absolutePosition = glm::vec2(0.f); //absolute in units offset from relative position
    glm::vec2 relativePosition = glm::vec2(0.f); //normalised relative to screen size
    float depth = 0.f; //z depth
    std::function<void(cro::Entity)> resizeCallback;
};
static constexpr glm::vec2 UIHiddenPosition(-10000.f, -10000.f);

//spacing of each menu relative to root node
//see GolfMenuState::m_menuPositions/MenuCreation.cpp
static constexpr glm::vec2 MenuSpacing(1920.f, 1080.f);
static constexpr float MenuBottomBorder = 15.f;
static constexpr float BannerPosition = MenuBottomBorder - 3.f;

static constexpr float NameWidth = 96.f;
static constexpr std::uint32_t BallPreviewSize = 64u;
static constexpr std::uint32_t BallThumbSize = 40u;
static constexpr glm::uvec2 AvatarPreviewSize(70, 90);
static constexpr glm::uvec2 AvatarThumbSize(60, 77);

static constexpr float LobbyTextSpacing = 274.f;
static constexpr float MinLobbyCropWidth = 96.f;
static constexpr glm::vec2 LobbyBackgroundPosition(0.5f, 0.62f); //relative
static constexpr glm::vec2 CourseDescPosition(0.5f, 0.24f); //relative
static constexpr glm::vec2 ClubTextPosition(0.01f, 1.f); //relative
static constexpr glm::vec2 WindIndicatorPosition(48.f, 56.f); //absolute from edge of the screen

static const cro::Time MouseHideTime = cro::seconds(3.f);

static const std::array<std::string, 5u> ShaderNames =
{
    "Terminal Display",
    "Terminal (Extreme)",
    "Black and White",
    "CRT Effect",
    "Cinematic"
};

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
        return ((evt.key.keysym.sym == SDLK_KP_ENTER || evt.key.keysym.sym == SDLK_RETURN) && ((evt.key.keysym.mod & KMOD_ALT) == 0));
    }
}

static inline void centreText(cro::Entity entity)
{
    auto bounds = cro::Text::getLocalBounds(entity);
    bounds.width = std::floor(bounds.width / 2.f);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
}

struct HighlightAnimationCallback final
{
    float currTime = 1.f;

    void operator() (cro::Entity e, float dt)
    {
        float scale = cro::Util::Easing::easeInBounce(currTime) * 0.2f;
        scale += 1.f;
        e.getComponent<cro::Transform>().setScale({ scale, scale });

        currTime = std::max(0.f, currTime - (dt * 2.f));
        if (currTime == 0)
        {
            currTime = 1.f;
            e.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
            e.getComponent<cro::Callback>().active = false;
        }
    }
};