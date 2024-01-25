/*-----------------------------------------------------------------------

Matt Marchant 2024
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

Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/graphics/Rectangle.hpp>
#include <crogine/detail/glm/vec2.hpp>

struct TrackSprite final
{
    float segmentInterp = 0.f; //used when interpolating position between two segments
    float scale = 1.f;
    float position = 0.f; //0 center +/- 1 road >1 <-1 off road
    glm::vec2 size = glm::vec2(0.f);
    cro::FloatRect uv;

    enum
    {
        Tree01,
        Tree02,
        Tree03,
        Bush01,
        Bush02,
        Bush03,

        Platform,

        Rock,
        Log,

        TallTree01,
        TallTree02,
        LampPost,
        PhoneBox,
        CartSide,
        CartFront,

        Ball,
        Flag,

        CartAway,

        Count
    };
    std::int32_t id = 0;
    std::size_t frameIndex = 0;
    float hitboxMultiplier = 1.f; //things like trees and vehicles have a smaller hitbox
    
    struct Animation final
    {
        enum
        {
            None, Rotate, Float,

            Count
        };
    };
    std::int32_t animation = 0;

    bool collisionActive = true;
};