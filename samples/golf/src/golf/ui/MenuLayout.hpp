/*-----------------------------------------------------------------------

Matt Marchant 2025
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
#include <crogine/ecs/Entity.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <array>
#include <functional>
#include <vector>

namespace UI
{
    static constexpr float TabBarHeight = 16.f;

    static constexpr float ItemHeight = TabBarHeight * 2.5f;
    static constexpr float ItemSpacing = 6.f;
    static constexpr glm::vec2 ItemImage = glm::vec2(ItemHeight - (ItemSpacing * 2.f), ItemHeight - (ItemSpacing * 2.f));

    static constexpr float InfoBarHeight = 24.f; //space at the bottom
}

struct TabBar final
{
    struct Item final
    {
        cro::Entity text;
        float displayWidth = 0.375f; //how much horizontal space items in this tab use

        enum
        {
            Left, Centre, Right
        }alignment = Left;

        cro::FloatRect hitbox; //in screen coords
        cro::Sprite sprite;
    };

    cro::Entity background;
    std::vector<Item> items;
    std::size_t activeIndex = 0;

    std::int32_t hoveredIndex = -1;

    cro::Entity navLeft;
    cro::Entity navRight;

    cro::Entity navLeftSprite;
    cro::Entity navRightSprite;

    std::array<cro::FloatRect, 2U> navLeftRects = {};
    std::array<cro::FloatRect, 2U> navRightRects = {};
};

struct Menu final
{
    struct Item final
    {
        //optional image to display colour selection
        //or achievement ID
        const cro::Texture* texture = nullptr;
        cro::FloatRect uv; //pixel coords for SimpleQuad
        cro::Colour previewColour = cro::Colour::White;

        //display type depending on data eg float/slider etc
        enum
        {
            Default, //left/right arrows
            Slider, //represents a sliding amount
            TextOnly, //displays the description on the item
            Heading //only displays the title, with half height background
        }displayType = Default;

        //float-rect in menu space to test click against
        cro::FloatRect hitbox;

        bool valueChangedOnActivate = false;
        bool alwaysActivate = false; //hack to always call activation callback regardless of input
        bool wrapValue = true; //value wraps back to the beginning instead of clamping
        std::int32_t selectedIndex = 0; //currently selected entry
        std::vector<cro::String> labels; //display text for each setting when cycled
        cro::String title; //main display title
        cro::String subTitle; //shown below title in TextOnly items
        cro::String description; //shown when hovered

        std::function<void(const Item&)> selected; //called when selected
        std::function<void(Item&)> activated; //called when activated
        bool activateLeft()
        {
            assert(!labels.empty());
            const auto count = static_cast<std::int32_t>(labels.size());

            if (count > 1)
            {
                selectedIndex = wrapValue ?
                    (selectedIndex + (count - 1)) % count
                    : std::max(selectedIndex - 1, 0);

                valueChangedOnActivate = true;
                activated(*this);
                return true;
            }
            return false;
        }

        bool activateRight()
        {
            assert(!labels.empty());
            const auto count = static_cast<std::int32_t>(labels.size());

            if (count > 1)
            {
                selectedIndex = wrapValue ?
                    (selectedIndex + 1) % count
                    : std::min(selectedIndex + 1, count - 1);

                valueChangedOnActivate = true;
                activated(*this);
                return true;
            }
            return false;
        }

        bool activate()
        {
            assert(!labels.empty());
            const auto count = static_cast<std::int32_t>(labels.size());

            if (count == 1
                || alwaysActivate)
            {
                valueChangedOnActivate = false;
                activated(*this);
                return true;
            }
            return false;
        }
    };
    std::vector<std::vector<Item>> items = {};

    cro::RenderTexture texture;
    cro::Entity sprite;

    std::uint32_t itemIndex = 0;
    std::int32_t hoveredIndex = -1;

    cro::FloatRect itemBox; //size is menu coords, position is updated during testing with current scroll position
};

void scrollToTarget(TabBar&, Menu&, float);