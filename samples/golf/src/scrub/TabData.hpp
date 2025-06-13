/*-----------------------------------------------------------------------

Matt Marchant 2024 - 2025
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
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

//this is actually shared with both Scrub and Sports Ball

constexpr float TabDisplayTime = 6.f;

struct TabData final
{
    enum
    {
        In, Hold, Out
    };
    std::int32_t state = Hold;

    std::function<void(cro::Entity)> onStartIn;
    std::function<void(cro::Entity)> onEndIn;
    std::function<void(cro::Entity)> onStartOut;
    std::function<void(cro::Entity)> onEndOut;

    //all sprites are parented to this so they can be scaled to fit the screen
    cro::Entity spriteNode;

    void show(cro::Entity e)
    {
        state = In;
        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

        const auto size = glm::vec2(cro::App::getWindow().getSize());
        e.getComponent<cro::Transform>().setPosition({ size.x, 0.f });

        if (onStartIn)
        {
            onStartIn(e);
        }
    }

    void hide(cro::Entity e)
    {
        state = Out;
        if (onStartOut)
        {
            onStartOut(e);
        }
    }
};

static inline void processTab(cro::Entity e, float dt)
{
    const auto size = glm::vec2(cro::App::getWindow().getSize());
    auto& data = e.getComponent<cro::Callback>().getUserData<TabData>();
    const auto Speed = dt * size.x * 8.f;

    switch (data.state)
    {
    default:
        break;
    case TabData::In:
    {
        //Y should always be set, but if for some reason the res
        //changes mid-transition we update that too
        //the UIElement property always expects elements to be relative to 0,0
        //so tabs move from the bottom left of the screen
        const auto target = glm::vec2(0.f);
        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.x = std::max(pos.x - Speed, target.x);
        pos.y = std::max(pos.y - Speed, target.y);
        e.getComponent<cro::Transform>().setPosition(pos);

        if (pos.x == target.x && pos.y == target.y)
        {
            data.state = TabData::Hold;
            if (data.onEndIn)
            {
                data.onEndIn(e);
            }
        }
    }
    break;
    case TabData::Out:
    {
        const auto target = glm::vec2(-size.x, 0.f);
        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.x = std::max(pos.x - Speed, target.x);
        pos.y = std::max(pos.y - Speed, target.y);
        e.getComponent<cro::Transform>().setPosition(pos);

        if (pos.x == target.x && pos.y == target.y)
        {
            data.state = TabData::Hold;
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            if (data.onEndOut)
            {
                data.onEndOut(e);
            }
        }
    }
    break;
    }
}