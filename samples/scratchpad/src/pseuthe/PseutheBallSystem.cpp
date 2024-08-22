#include "PseutheBallSystem.hpp"
#include "PseutheConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr float Elasticity = 0.81f;
    const std::int32_t MaxCollisions = 160; //after this we reset the velocity of all balls at random
}

PseutheBallSystem::PseutheBallSystem(cro::MessageBus& mb)
    : cro::System       (mb, typeid(PseutheBallSystem)),
    m_collisionCount    (0)
{
    requireComponent<PseutheBall>();
    requireComponent<cro::Transform>();
}

//public
void PseutheBallSystem::process(float dt)
{

    //there should never be more than 19 balls so we're not bothering with quad trees etc
    auto& collisionEntities = getEntities(); 
    m_collisionPairs.clear();

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& tx = entity.getComponent<cro::Transform>();
        auto& ball = entity.getComponent<PseutheBall>();

        tx.move(ball.velocity * dt);

        //wrap around
        auto pos = tx.getPosition();       
        if (pos.x < -ball.radius)
        {
            pos.x += SceneSizeFloat.x;
        }
        else if (pos.x > SceneSizeFloat.x + ball.radius)
        {
            pos.x -= SceneSizeFloat.x;
        }

        if (pos.y < -ball.radius)
        {
            pos.y += SceneSizeFloat.y;
        }
        else if (pos.y > SceneSizeFloat.y + ball.radius)
        {
            pos.y -= SceneSizeFloat.y;
        }
        tx.setPosition(pos);


        //check for intersection with the edge of the screen and mark for double collision test
        ball.edges[PseutheBall::Left] = std::abs(pos.x) < ball.radius;
        ball.edges[PseutheBall::Right] = std::abs(SceneSizeFloat.x - pos.x) < ball.radius;

        ball.edges[PseutheBall::Bottom] = std::abs(pos.y) < ball.radius;
        ball.edges[PseutheBall::Top] = std::abs(SceneSizeFloat.y - pos.y) < ball.radius;

        for (auto other : collisionEntities)
        {
            if (other != entity)
            {
                if (testCollision(pos, ball.radius, other))
                {
                    m_collisionPairs.insert(std::minmax(entity, other,
                        [](const cro::Entity& a, const cro::Entity& b)
                        {
                            return a.getIndex() < b.getIndex();
                        }));
                }
            }
        }

        if (m_collisionCount > MaxCollisions)
        {
            ball.velocity.x = static_cast<float>(cro::Util::Random::value(-MaxVelocity, MaxVelocity));
            ball.velocity.y = static_cast<float>(cro::Util::Random::value(-MaxVelocity, MaxVelocity));
        }
    }

    if (m_collisionCount > MaxCollisions)
    {
        m_collisionCount = 0;
        //TODO raise a notification to play the sound
    }

    for (auto [e1, e2] : m_collisionPairs)
    {
        auto& tx1 = e1.getComponent<cro::Transform>();
        auto& tx2 = e2.getComponent<cro::Transform>();

        auto& b1 = e1.getComponent<PseutheBall>();
        auto& b2 = e2.getComponent<PseutheBall>();

        Manifold m;
        const auto collisionVec = tx2.getPosition() - tx1.getPosition();
        const auto collisionLen = glm::length(collisionVec);
        m.normal = collisionVec / collisionLen;
        CRO_ASSERT(!std::isnan(m.normal.x), "");
        CRO_ASSERT(!std::isnan(m.normal.y), "");
        CRO_ASSERT(!std::isnan(m.normal.z), "");
        
        
        //hmm there must be a bug here because occasionally collisionLen
        //is longer than the sum of the radii - which would mean there isn't a collision!
        m.penetration = ((b1.radius + b2.radius) - collisionLen);// / 2.f;

        CRO_ASSERT(!std::isnan(m.penetration), "");

        if (m.penetration > 0)
        {
            const float relForce = -glm::dot(glm::vec2(m.normal), b1.velocity - b2.velocity);
            const float impulse = (Elasticity * relForce) / (b1.inverseMass + b2.inverseMass);
            m.transferForce = m.normal * (impulse / b2.mass);

            b1.velocity += m.transferForce;
            tx1.move(-((b2.mass / (b2.mass + b1.mass) * m.penetration) * m.normal));

            m.normal = -m.normal;
            m.transferForce = m.normal * (impulse / b1.mass);

            b2.velocity += m.transferForce;
            tx2.move(-((b1.mass / (b1.mass + b2.mass) * m.penetration) * m.normal));

            m_collisionCount++;
        }
    }
}

//private
bool PseutheBallSystem::testCollision(glm::vec3 pos, float radius, cro::Entity other)
{
    const auto dir = other.getComponent<cro::Transform>().getPosition() - pos;
    const auto len2 = glm::length2(dir);
    const float totalRad = radius + other.getComponent<PseutheBall>().radius;

    return len2 < (totalRad * totalRad);
}