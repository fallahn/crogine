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


struct PropFollower final
{
    Path path;
    
    struct Point final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 velocity = glm::vec3(0.f);
        std::int32_t target = 1;
    };
    std::array<Point, 2> axis = {};
    void initAxis(cro::Entity);

    float rotation = 0.f;
    float targetRotation = 0.f;
    float speed = 6.f;
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