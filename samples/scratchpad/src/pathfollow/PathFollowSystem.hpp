/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <vector>

struct PathFollower final
{
    std::vector<glm::vec3> path;
    float moveSpeed = 1.f;

    //forward vector when we started slerp
    glm::quat startRotation = cro::Transform::QUAT_IDENTITY;
    glm::quat targetRotation = cro::Transform::QUAT_IDENTITY;

    float currentTurn = 1.f; //we slerp out rotation from start to target over turnSpeed
    float turnSpeed = 1.f;

    float minRadius = 2.5f; //look for next point when within this radius of current target

    std::size_t target = 1;
    enum
    {
        Loop, PingPong
    }motion = Loop;
};

class PathFollowSystem final : public cro::System
{
public:
    explicit PathFollowSystem(cro::MessageBus&);

    void process(float) override;

private:


    void onEntityAdded(cro::Entity) override;
};