#include "PseuthePlayerSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/util/Maths.hpp>

PseuthePlayerSystem::PseuthePlayerSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PseuthePlayerSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<PseuthePlayer>();

    //std::fill(m_cells.begin(), m_cells.end(), false);
}

//public
void PseuthePlayerSystem::process(float dt)
{
    for (auto entity : getEntities())
    {
        auto& player = entity.getComponent<PseuthePlayer>();
        switch (player.state)
        {
        default: break;
        case PseuthePlayer::State::Active:
            updateActive(entity, dt);
            break;
        }
    }
}

//private
void PseuthePlayerSystem::updateActive(cro::Entity entity, float dt)
{
    //move towards grid position
    auto& tx = entity.getComponent<cro::Transform>();
    auto& player = entity.getComponent<PseuthePlayer>();

    static constexpr float Speed = 200.f;
    static constexpr float MinDistSqr = 5.f * 5.f;

    const auto target = getWorldPosition(player.gridPosition) + CellCentre;
    const auto dir = target - glm::vec2(tx.getPosition());

    //if close enough to grid centre step using current direction
    const auto len2 = glm::length2(dir);
    //TODO could add a dot prod to ensure target is still
    //in front in case we some how travel mode than 6 pixels

    bool collides = false;
    if (len2 < MinDistSqr)
    {
        //mark the cell we just left as empty in the cell grid
        m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].occupied = false;

        //call recursive func on nextPart if tail is valid, as this will be the head part
        if (player.tail.isValid())
        {
            //player.tail.getComponent<PseuthePlayer>().updateNext();
            player.gridDirection = player.nextDirection;
            m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].exit = player.gridDirection;

            //step will have moved us on a cell so here we're checking if the new cell is occupied
            collides = !player.step() || m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].occupied;
        }
        else
        {
            //this is a regular body part which steps, but doesn't collision check
            m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].exit = player.gridDirection;
            player.step();
            player.gridDirection = m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].exit;
        }
    }

    if (!collides)
    {
        if (len2 != 0)
        {
            tx.move((dir / std::sqrt(len2)) * Speed * dt);
        }
    }
    else
    {
        //as the head is the only part with collision
        //detection, this should only ever be called on the head part
        player.setState(PseuthePlayer::State::End);

        //TODO raise a message for sound/particles/game rules


        return; //we changed state so the rest of this update function is inapplicable
    }


    //rotate the head towards the target
    const auto targetRot = std::atan2(dir.y, dir.x);
    const auto currRot = tx.getRotation2D();
    const auto rotation = cro::Util::Maths::shortestRotation(currRot, targetRot) * PseuthePlayer::RotationMultiplier;
    tx.rotate(rotation * dt);


    //mark the cell in the grid array as occupied so we can test body collision
    m_cells[player.gridPosition.y * CellCountX + player.gridPosition.x].occupied = true;
}