#pragma once

#include "PseutheConsts.hpp"

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/ecs/System.hpp>

#include <cstdint>

struct PseuthePlayer final
{
    glm::ivec2 gridPosition = glm::ivec2(0);
    glm::ivec2 gridDirection = glm::ivec2(0);

    //true if successful, false if collision
    bool step()
    {
        gridPosition += gridDirection;
        return gridPosition.x > -1 && gridPosition.x < CellCountX
            && gridPosition.y > -1 && gridPosition.y < CellCountY;
    }

    struct Direction final
    {
        static constexpr auto Left  = glm::ivec2(-1, 0);
        static constexpr auto Right = glm::ivec2( 1, 0);
        static constexpr auto Up    = glm::ivec2(0,  1);
        static constexpr auto Down  = glm::ivec2(0, -1);
    };

    struct State final
    {
        enum
        {
            Start, Active, End
        };
    };
    std::int32_t state = State::Start;
};


class PseuthePlayerSystem final : public cro::System
{
public:
    explicit PseuthePlayerSystem(cro::MessageBus&);

    void process(float) override;

private:
    void updateActive(cro::Entity, float);
};