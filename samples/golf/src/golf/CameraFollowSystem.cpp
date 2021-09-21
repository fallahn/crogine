/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "CameraFollowSystem.hpp"
#include "MessageIDs.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

CameraFollowSystem::CameraFollowSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(CameraFollowSystem)),
    m_closestCamera(0)
{
    requireComponent<cro::Transform>();
    requireComponent<cro::Camera>();
    requireComponent<CameraFollower>();
}

//public
void CameraFollowSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::GolfMessage)
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::BallLanded)
        {
            m_closestCamera = 0; //reset to player camera
        }
    }
}

void CameraFollowSystem::process(float dt)
{
    float currDist = std::numeric_limits<float>::max();
    auto lastCam = m_closestCamera;

    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& follower = entity.getComponent<CameraFollower>();

        switch (follower.state)
        {
        default: break;
        case CameraFollower::Track:
        {
            auto diff = follower.target - follower.currentTarget;
            follower.currentTarget += diff * (dt * 4.f);

            auto& tx = entity.getComponent<cro::Transform>();
            auto lookAt = glm::lookAt(tx.getPosition(), follower.currentTarget, cro::Transform::Y_AXIS);
            tx.setLocalTransform(glm::inverse(lookAt));

            //check the distance to the ball, and store it if closer than previous dist
            //and if we fall within the camera's radius
            //and if the play isn't standing too close
            auto dist = glm::length2(tx.getPosition() - follower.target);
            auto dist2 = glm::length2(tx.getPosition() - follower.playerPosition);
            if (dist < currDist
                && dist < follower.radius
                && dist2 > follower.radius)
            {
                currDist = dist;
                m_closestCamera = follower.id;
            }

            if (m_closestCamera != follower.id)
            {
                follower.state = CameraFollower::Reset;
            }
        }
            break;
        case CameraFollower::Zoom:
            if (follower.fov < follower.currentFov)
            {
                follower.currentFov -= dt;
                entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());
            }
            else
            {
                follower.state = CameraFollower::Track;
            }
            break;
        case CameraFollower::Reset:
            follower.currentFov = 1.f;
            follower.fov = 1.f;
            entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());

            follower.state = CameraFollower::Track;
            break;
        }
    }

    //send a message with desired camera ID
    if (m_closestCamera != lastCam)
    {
        auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
        msg->type = SceneEvent::RequestSwitchCamera;
        msg->data = m_closestCamera;
    }
}