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

#include <crogine/ecs/Entity.hpp>
#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    class Scene;
}

struct SharedStateData;

//inherit this so a specific menu state
//can supply member data to the menu callback
struct MenuContext
{
    cro::Scene* uiScene = nullptr;
    std::size_t* currentMenu = nullptr;
    cro::Entity* menuEntities = nullptr;

    const glm::vec2* menuPositions = nullptr;
    const glm::vec2* viewScale = nullptr;
    const SharedStateData* sharedData = nullptr;
};

struct MenuCallback final
{
    MenuContext menuContext;
    explicit MenuCallback(const MenuContext& m) : menuContext(m) {}

    void operator()(cro::Entity, float);
};

struct TitleTextCallback final
{
    void operator() (cro::Entity, float);
};

//used in animation callback
struct MenuData final
{
    enum
    {
        In, Out
    }direction = In;
    float currentTime = 0.f;

    std::int32_t targetMenu = 1;
};

struct SpriteID final
{
    enum
    {
        CPUHighlight,

        ButtonBanner,
        Cursor,
        Flag,
        PrevMenu,
        NextMenu,
        AddPlayer,
        RemovePlayer,
        ReadyUp,
        ReadyStatus,
        NetStrength,
        StartGame,
        Connect,
        PrevCourse,
        PrevCourseHighlight,
        NextCourse,
        NextCourseHighlight,
        LobbyCheckbox,
        LobbyCheckboxHighlight,
        LobbyRuleButton,
        LobbyRuleButtonHighlight,
        LobbyLeft,
        LobbyRight,
        WeatherHighlight,
        Envelope,
        LevelBadge,
        InviteHighlight,

        Count
    };
};