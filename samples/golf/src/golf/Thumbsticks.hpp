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

-----------------------------------------------------------------------*/

#pragma once

#include <crogine/core/GameController.hpp>

#include <cstdint>
#include <array>

struct Thumbsticks final
{
    void setValue(std::int32_t axis, std::int16_t value)
    {
        switch (axis)
        {
        default: break;
        case cro::GameController::AxisLeftX:
            thumbsticks[Thumbstick::Left].x = value;
            break;
        case cro::GameController::AxisLeftY:
            thumbsticks[Thumbstick::Left].y = value;
            break;
        case cro::GameController::AxisRightX:
            thumbsticks[Thumbstick::Right].x = value;
            break;
        case cro::GameController::AxisRightY:
            thumbsticks[Thumbstick::Right].y = value;
            break;
        }
    }

    std::int16_t getValue(std::int32_t axis) const
    {
        switch (axis)
        {
        default: return 0;
        case cro::GameController::AxisLeftX:
            return thumbsticks[Thumbstick::Left].x;
        case cro::GameController::AxisLeftY:
            return thumbsticks[Thumbstick::Left].y;
        case cro::GameController::AxisRightX:
            return thumbsticks[Thumbstick::Right].x;
        case cro::GameController::AxisRightY:
            return thumbsticks[Thumbstick::Right].y;
        }
    }

    void reset()
    {
        thumbsticks = {};
    }

private:
    struct Thumbstick final
    {
        enum
        {
            Left, Right,
            Count
        };

        std::int16_t x = 0;
        std::int16_t y = 0;
    };
    std::array<Thumbstick, Thumbstick::Count> thumbsticks = {};
};
