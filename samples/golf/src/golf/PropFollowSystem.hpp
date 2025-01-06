/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "Path.hpp"
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>

struct PropFollower final
{
    std::vector<glm::vec3> path;
    float moveSpeed = 1.f;

    //forward vector when we started slerp
    glm::quat startRotation = cro::Transform::QUAT_IDENTITY;
    glm::quat targetRotation = cro::Transform::QUAT_IDENTITY;

    float currentTurn = 1.f; //we slerp out rotation from start to target over turnSpeed
    float turnSpeed = 2.f;

    float minRadius = 3.5f; //look for next point when within this radius of current target

    std::size_t target = 1;

    bool loop = true;

    enum State
    {
        Idle, Follow
    }state = Idle;
    float idleTime = 0.f;
    float stateTimer = 0.f;

    static constexpr float WaitTime = 12.f;
    float waitTimeout = WaitTime;
};

class CollisionMesh;

class PropFollowSystem final : public cro::System
{
public:
    PropFollowSystem(cro::MessageBus&, const CollisionMesh&);

    void process(float) override;
    void setPlayerPosition(glm::vec3 pos) { m_playerPosition = pos; }

private:
    const CollisionMesh& m_collisionMesh;
    glm::vec3 m_playerPosition;

    void onEntityAdded(cro::Entity) override;
};