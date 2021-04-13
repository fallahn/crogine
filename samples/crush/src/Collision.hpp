/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/graphics/Rectangle.hpp>

#include <array>
#include <cstdint>

struct CollisionID final
{
    //used as filters in the dynamic tree query
    //so for instance items on both layers could
    //be solid/water/crate
    enum
    {
        //layers must always come first!!
        LayerOne = 0x1,
        LayerTwo = 0x2
    };
};

struct CollisionMaterial final
{
    //used to decide how a collision should be resolved
    enum
    {
        None = -1,
        Solid,
        Water,
        Crate,
        Body,
        Foot
    };
};

struct CollisionRect final
{
    cro::FloatRect bounds;
    std::int32_t material = CollisionMaterial::None;
};

struct CollisionComponent final
{
    std::array<CollisionRect, 2u> rects;
    std::size_t rectCount = 0;

    //merged rectagles to give a global
    //bounds for broadphase testing
    cro::FloatRect sumRect =
    {
        std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()
    };
    void calcSum()
    {
        for (auto i = 0u; i < rectCount; ++i)
        {
            const auto& rect = rects[i];

            if (rect.bounds.left < sumRect.left) sumRect.left = rect.bounds.left;
            if (rect.bounds.bottom < sumRect.bottom) sumRect.bottom = rect.bounds.bottom;

            if (rect.bounds.width > sumRect.width) sumRect.width = rect.bounds.width;
            if (rect.bounds.height > sumRect.height) sumRect.height = rect.bounds.height;
        }
    }
};
