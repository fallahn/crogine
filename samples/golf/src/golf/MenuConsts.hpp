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

#include "ScoreType.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Vertex2D.hpp>
#include <crogine/ecs/Entity.hpp>
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

        OSK
    };
};

static const std::array<std::string, ScoreType::Count> ScoreTypes =
{
    "Stroke Play", 
    "Stableford", 
    "Stableford Pro",
    "Match Play",
    "Skins",
    "Multi-target",
    "Short Round",
    /*
    "Elimination",
    "Bingo Bango Bongo",
    "Nearest the Pin",
    "Longest Drive",
    */
};

static const std::array<std::string, GimmeSize::Count> GimmeString =
{
    "No Gimme",
    "Inside The Leather",
    "Inside The Putter"
};

static const std::array<std::string, 3u> CourseTypes =
{
    "Official Courses", "User Courses", "Workshop Courses"
};

static const std::array<std::string, ScoreType::Count> RuleDescriptions =
{
#ifdef USE_GNS
    "The player with the fewest total strokes wins.\nSolo or network play scores contribute to the\nmonthly and all time leaderboards.\nContributes to the Club League.\n\nRecommended for solo play or 2+ players.",
#else
    "The player with the fewest total strokes wins.\nContributes to the Club League.\n\nRecommended for solo play or 2+ players",
#endif
    "Stroke play rules, however par is scored at\n2 points, with one extra point awarded for every\nstroke under par. Max strokes are reached when\nthere are no more points available. The player with\nthe most points wins. Contributes to the Club League.\nRecommended for solo play or 2+ players.",
    "Stableford rules, however one point for every\nstroke over par is deducted instead of reaching\nthe stroke limit. The player with the most points wins.\nContributes to the Club League.\n\nRecommended for solo play or 2+ players.",
    "Holes are scored individually by fewest strokes\n and one point is awarded for each hole won.\nThe player with the most points wins.\n\n\nRecommended for 2-4 players.",
    "Holes are scored individually by fewest strokes.\nThe winner of the hole gets the skins pot,\nelse the pot rolls over to the next hole.\nTies are resolved with a sudden death round.\n\nRecommended for 2-4 players.",
    "Stroke play, but each player must hit the mid-point\ntarget before reaching the green. Not hitting the\ntarget forfeits the hole. Round times are usually\nlonger than average. Par is increased by one\non larger courses.\nRecommended for 2-4 players.",
#ifdef USE_GNS
    "As stroke play, however the number of holes is\nreduced by 33% - 18 holes become 12 and 9 holes\nbecome 6. Ideal for casual games. User courses\nremain unaffected, and scores are omitted from\nthe leaderboards. Contributes to the Club League.\nRecommended for solo play or 2+ players.",
#else
    "As stroke play, however the number of holes is\nreduced by 33% - 18 holes become 12 and 9 holes\nbecome 6. Ideal for casual games. User courses\nremain unaffected.Contributes to the Club League.\n\nRecommended for solo play or 2+ players.",
#endif

    /*
    "Elimination mode. The round is played as Stroke\nPlay, however after the first hole the last\nplayer to hole out each turn is eliminated.\nThe game is won by the final remaining player.\n\nMinimum 3 players, recommended for 4+ players."
    "The first player on the green scores Bingo, the\nplayer closest to the pin when all players are on\nthe green scores Bango and first to hole out wins\nBongo. Not recommended for putting courses.",
    "Each player has one stroke to get as near to the\npin as possible. The winner is the player with the\nshortest total distance.\nGreat for casual play.",
    "Each player has one stroke to make the longest\ndrive possible while staying on the fairway.\nThe winner is the player with the longest total\ndistance.",
    */
};

static constexpr std::array<glm::vec3, 8u> EmotePositions =
{
    glm::vec3(0.f, 34.f, 0.15f),
    glm::vec3(34.f, 0.f, 0.15f),
    glm::vec3(0.f, -34.f, 0.15f),
    glm::vec3(-34.f, 0.f, 0.15f),

    glm::vec3(24.f, 24.f, 0.15f),
    glm::vec3(-24.f, 24.f, 0.15f),
    glm::vec3(-24.f, -24.f, 0.15f),
    glm::vec3(24.f, -24.f, 0.15f)
};

static constexpr std::uint32_t LargeTextSize = 64;
static constexpr std::uint32_t MediumTextSize = 32;
static constexpr std::uint32_t SmallTextSize = 16;

static constexpr std::uint32_t UITextSize = 8;
static constexpr std::uint32_t InfoTextSize = 10;
static constexpr std::uint32_t LabelTextSize = 16;

static constexpr cro::Colour TextNormalColour(0xfff8e1ff);
static constexpr cro::Colour TextEditColour(0x6eb39dff);
static constexpr cro::Colour TextHighlightColour(0xb83530ff); //red
//static const cro::Colour TextHighlightColour(0x005ab5ff); //cb blue
//static const cro::Colour TextHighlightColour(0x006cd1ff); //cb blue 2
static constexpr cro::Colour TextGoldColour(0xf2cf5cff);
static constexpr cro::Colour TextOrangeColour(0xec773dff);
static constexpr cro::Colour TextGreenColour(0x6ebe70ff);
static constexpr cro::Colour LeaderboardTextDark(0x1a1e2dff);
static constexpr cro::Colour LeaderboardTextLight(0xfff8e1ff);

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
static constexpr glm::uvec2 BallPreviewSize = glm::uvec2(118u, 128u);
static constexpr glm::uvec2 AvatarPreviewSize(98, 128);

static constexpr float LobbyTextSpacing = 274.f;
static constexpr float MinLobbyCropWidth = 96.f;
static constexpr glm::vec2 LobbyBackgroundPosition(0.5f, 0.62f); //relative
static constexpr glm::vec2 CourseDescPosition(0.5f, 0.24f); //relative
static constexpr glm::vec2 ClubTextPosition(0.01f, 1.f); //relative
static constexpr glm::vec2 WindIndicatorPosition(48.f, 60.f); //absolute from edge of the screen

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

static inline bool deactivated(const cro::ButtonEvent& evt)
{
    switch (evt.type)
    {
    default: return false;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
        return evt.button.button == SDL_BUTTON_RIGHT;
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERBUTTONDOWN:
        return evt.cbutton.button == SDL_CONTROLLER_BUTTON_B;
    case SDL_FINGERUP:
    case SDL_FINGERDOWN:
        return false;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return (((evt.key.keysym.sym == SDLK_ESCAPE || evt.key.keysym.sym == SDLK_BACKSPACE)) && ((evt.key.keysym.mod & ~KMOD_NUM) == 0));
    }
}

static inline void centreText(cro::Entity entity)
{
    auto bounds = cro::Text::getLocalBounds(entity);
    bounds.width = std::floor(bounds.width / 2.f);
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
}

static inline std::vector<cro::Vertex2D> createMenuBackground(glm::vec2 size)
{
    //assumes GL_TRIANGLES
    //assumes origin bottom left
    static constexpr cro::Colour Background(0x0000003f);
    static constexpr cro::Colour Inner(0x64432fff);
    static constexpr cro::Colour Light(0x7e6d37ff);
    static constexpr cro::Colour Dark(0x50282fff);

    static constexpr float ShadowOffset = 6.f;
    return
    {
        //background
        cro::Vertex2D(glm::vec2(0.f, size.y ), Background),
        cro::Vertex2D(glm::vec2(0.f, -ShadowOffset), Background),
        cro::Vertex2D(glm::vec2(size.x + ShadowOffset, size.y), Background),

        cro::Vertex2D(glm::vec2(size.x + ShadowOffset, size.y), Background),
        cro::Vertex2D(glm::vec2(0.f, -ShadowOffset), Background),
        cro::Vertex2D(glm::vec2(size.x + ShadowOffset, - ShadowOffset), Background),

        //left
        cro::Vertex2D(glm::vec2(-2.f, size.y + 1.f), Light),
        cro::Vertex2D(glm::vec2(-2.f, -1.f), Light),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), Light),

        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), Light),
        cro::Vertex2D(glm::vec2(-2.f, -1.f), Light),
        cro::Vertex2D(glm::vec2(-1.f), Light),

        //right
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 1.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 2.f, size.y + 1.f), Dark),

        cro::Vertex2D(glm::vec2(size.x + 2.f, size.y + 1.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 2.f, -1.f), Dark),

        //top
        cro::Vertex2D(glm::vec2(-1.f, size.y + 2.f), Light),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), Light),
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 2.f), Light),

        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 2.f), Light),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), Light),
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 1.f), Light),

        //bottom
        cro::Vertex2D(glm::vec2(-1.f), Dark),
        cro::Vertex2D(glm::vec2(-1.f, -2.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), Dark),

        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), Dark),
        cro::Vertex2D(glm::vec2(-1.f, -2.f), Dark),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -2.f), Dark),

        //inner
        cro::Vertex2D(glm::vec2(-1.f, size.y +1.f), Inner),
        cro::Vertex2D(glm::vec2(-1.f), Inner),
        cro::Vertex2D(size + 1.f, Inner),

        cro::Vertex2D(size + 1.f, Inner),
        cro::Vertex2D(glm::vec2(-1.f), Inner),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), Inner),
    };
}

static inline std::vector<cro::Vertex2D> createMenuHighlight(glm::vec2 size, cro::Colour c)
{
    //assumes GL_TRIANGLES
    //assumes origin bottom left
    //assumes one pixel border
    //static constexpr cro::Colour c(cro::Detail::Transparent);
    return
    {
        //left
        cro::Vertex2D(glm::vec2(-2.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(-2.f, - 1.f), c),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), c),

        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(-2.f, -1.f), c),
        cro::Vertex2D(glm::vec2(-1.f), c),

        //right
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 2.f, size.y + 1.f), c),

        cro::Vertex2D(glm::vec2(size.x + 2.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 2.f, -1.f), c),

        //top
        cro::Vertex2D(glm::vec2(-1.f, size.y + 2.f), c),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 2.f), c),

        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 2.f), c),
        cro::Vertex2D(glm::vec2(-1.f, size.y + 1.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, size.y + 1.f), c),

        //bottom
        cro::Vertex2D(glm::vec2(-1.f), c),
        cro::Vertex2D(glm::vec2(-1.f, -2.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), c),

        cro::Vertex2D(glm::vec2(size.x + 1.f, -1.f), c),
        cro::Vertex2D(glm::vec2(-1.f, -2.f), c),
        cro::Vertex2D(glm::vec2(size.x + 1.f, -2.f), c),
    };
}

static constexpr float ProfileItemHeight = 14.f;
struct FlyoutMenu final
{
    std::uint32_t selectCallback = 0;
    std::uint32_t activateCallback = 0;
    std::vector<cro::Entity> items;

    cro::Entity background;
    cro::Entity detail; //could be text, could be colours
    cro::Entity highlight;
};

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