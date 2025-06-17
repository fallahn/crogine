/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include <crogine/core/String.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/util/Easings.hpp>

//used to animate strings with a text callback, ie player name
struct TextCallbackData final
{
    std::uint8_t client = 0;
    std::uint8_t player = 0;
    cro::String string;
    std::uint32_t currentChar = 0;
    float timer = 0.f;
    static constexpr float CharTime = 0.05f;
};

//prints text one character at a time (see name labels in game state)
struct TextAnimCallback final
{
    void operator () (cro::Entity e, float dt)
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<TextCallbackData>();
        data.timer += dt;
        if (data.timer > data.CharTime)
        {
            data.timer -= data.CharTime;
            data.currentChar++;

            if (data.currentChar <= data.string.size())
            {
                e.getComponent<cro::Text>().setString(data.string.substr(0, data.currentChar));

                auto bounds = cro::Text::getLocalBounds(e);
                bounds.width = std::floor(bounds.width / 2.f);
                e.getComponent<cro::Transform>().setOrigin({ bounds.width, 0.f });
            }
            else
            {
                data.timer = 0.f;
                data.currentChar = 0;
                e.getComponent<cro::Callback>().active = false;
            }
        }
    }
};

//used to animate text on menus (see paused/practice selection)
struct MenuTextCallback final
{
    const float baseScale = 1.f;
    explicit MenuTextCallback(float bs = 1.f) : baseScale(bs) {}

    void operator() (cro::Entity e, float dt)
    {
        data = std::min(1.f, data + (dt * 2.5f));

        const float scale = (1.f + (0.2f * (1.f - cro::Util::Easing::easeOutElastic(data)))) * baseScale;
        e.getComponent<cro::Transform>().setScale({ scale, scale });

        if (data == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            data = 0.f;
        }
    }

    float data = 0.f;
};