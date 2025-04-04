/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/detail/glm/vec3.hpp>

#include <btBulletCollisionCommon.h>

static constexpr glm::vec3 Gravity = glm::vec3(0.f, -0.98f, 0.f);
static const btVector3 RayLength = btVector3(0.f, 1.f, 0.f);
static constexpr glm::vec3 RollResetPosition(2.5f, 3.5f, 0.f);
static constexpr glm::vec3 RampResetPosition({ 13.f, 16.f, -25.f });

static inline btVector3 fromGLM(glm::vec3 v)
{
    return { v.x, v.y, v.z };
}

static inline glm::vec3 toGLM(btVector3 v)
{
    return { v.x(), v.y(), v.z() };
}

static inline btCollisionWorld::ClosestRayResultCallback collisionTest(btVector3 worldPos, const btCollisionWorld* collisionWorld)
{
    btCollisionWorld::ClosestRayResultCallback res(worldPos, worldPos + RayLength);
    collisionWorld->rayTest(worldPos , worldPos + RayLength, res);
    return res;
}