#include "PseuthePlayerSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/util/Maths.hpp>

PseuthePlayerSystem::PseuthePlayerSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PseuthePlayerSystem))
{
    requireComponent<cro::Transform>();
    requireComponent<PseuthePlayer>();
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
    static constexpr float MinDistSqr = 3.f * 3.f;

    const auto target = getWorldPosition(player.gridPosition) + CellCentre;
    const auto dir = target - glm::vec2(tx.getPosition());

    //if close enough to grid centre step using current direction
    const auto len2 = glm::length2(dir);
    //TODO could add a dot prod to ensure target is still
    //in front in case we some how travel mode than 6 pixels

    bool collides = false;
    if (len2 < MinDistSqr)
    {
        collides = !player.step();
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
        player.state = PseuthePlayer::State::End;
    }


    //rotate the head towards the target
    const auto rotDir = (getWorldPosition(player.gridPosition + player.gridDirection) + CellCentre) - glm::vec2(tx.getPosition());
    const auto targetRot = std::atan2(rotDir.y, rotDir.x);
    const auto currRot = tx.getRotation2D();
    const auto rotation = cro::Util::Maths::shortestRotation(currRot, targetRot) * 6.f;
    tx.rotate(rotation * dt);
}