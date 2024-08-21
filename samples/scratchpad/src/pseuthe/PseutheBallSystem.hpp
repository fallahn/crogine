#pragma once

#include <crogine/ecs/System.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

#include <set>

static constexpr float BallDensity = 2.5f;
static constexpr std::int32_t MaxVelocity = 100;

struct PseutheBall final
{
    glm::vec2 velocity = 
        glm::vec2
        (
        cro::Util::Random::value(-MaxVelocity, MaxVelocity),
        cro::Util::Random::value(-MaxVelocity, MaxVelocity)
        );
    float radius = 0.f;
    float radiusSqr = 0.f;
    float mass = 1.f;
    float inverseMass = 0.f;

    enum
    {
        Top, Bottom, Left, Right,
        Count
    };
    std::array<bool, Count> edges = { false, false, false, false };

    PseutheBall() = default;

    explicit PseutheBall(float r)
        : radius    (r), 
        radiusSqr   (r*r),
        mass        ((radius * radius) * cro::Util::Const::PI * BallDensity / 2.f), 
        inverseMass (1.f / mass) {}
};

class PseutheBallSystem final : public cro::System 
{
public:
    explicit PseutheBallSystem(cro::MessageBus&);

    void process(float) override;

private:

    struct Manifold final
    {
        glm::vec2 transferForce = glm::vec2(0.f);
        glm::vec3 normal = glm::vec3(0.f);
        float penetration = 0.f;
    };

    std::int32_t m_collisionCount;
    std::set<std::pair<cro::Entity, cro::Entity>> m_collisionPairs;
    bool testCollision(glm::vec3 position, float radius, cro::Entity other);
};