#pragma once

#include "PseutheConsts.hpp"

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/ecs/System.hpp>

#include <cstdint>
#include <array>

struct PseuthePlayer final
{
    glm::ivec2 gridPosition = glm::ivec2(0);
    glm::ivec2 gridDirection = glm::ivec2(0);
    glm::ivec2 nextDirection = glm::ivec2(0);

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

    void setState(std::int32_t s)
    {
        state = s;
        if (nextPart.isValid())
        {
            auto& player = nextPart.getComponent<PseuthePlayer>();
            player.setState(s);
        }
    }

    static constexpr float RotationMultiplier = 8.f;

    cro::Entity nextPart;
    cro::Entity tail; //only used on head part
};


class PseuthePlayerSystem final : public cro::System
{
public:
    explicit PseuthePlayerSystem(cro::MessageBus&);

    void process(float) override;

private:
    //marks a cell as having a body part or not
    struct Cell final
    {
        glm::ivec2 exit = glm::ivec2(0);
        bool occupied = false;
    };
    std::array<Cell, CellCountX * CellCountY> m_cells = {};

    void updateActive(cro::Entity, float);
};