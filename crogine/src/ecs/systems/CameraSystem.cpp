/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>


using namespace cro;

CameraSystem::CameraSystem(cro::MessageBus& mb)
    : System(mb, typeid(CameraSystem))
{
    requireComponent<Camera>();
    requireComponent<Transform>();
}

//public
void CameraSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        //TODO could dirty flag optimise as updating the frustum
        //requires 6(!!) sqrts

        auto& camera = entity.getComponent<Camera>();
        if (camera.active)
        {
            const auto& tx = entity.getComponent<Transform>();

            camera.viewMatrix = glm::inverse(tx.getWorldTransform());
            camera.viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;

            updateFrustum(camera);

            camera.drawList.clear();
            getScene()->updateDrawLists(entity);
        }
    }
}

//private
void CameraSystem::updateFrustum(Camera& camera)
{
    const auto& viewProj = camera.viewProjectionMatrix;
    camera.m_frustum =
    {
        { Plane //left
        (
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        ),
        Plane //right
        (
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        ),
        Plane //bottom
        (
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        ),
        Plane //top
        (
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        ),
        Plane //near
        (
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        ),
        Plane //far
        (
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        ) }
    };

    //normalise the planes
    for (auto& p : camera.m_frustum)
    {
        const float factor = 1.f / std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        p.x *= factor;
        p.y *= factor;
        p.z *= factor;
        p.w *= factor;
    }
}