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
