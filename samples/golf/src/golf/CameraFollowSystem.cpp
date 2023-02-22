/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "CameraFollowSystem.hpp"
#include "MessageIDs.hpp"
#include "Terrain.hpp"
#include "ClientCollisionSystem.hpp"
#include "BallSystem.hpp"
#include "InterpolationSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>
#include <crogine/detail/glm/common.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Maths.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    const std::array<std::string, CameraID::Count> CamNames =
    {
        "Player", "Bystander", "Sky", "Green", "Transition"
    };

    const std::array<std::string, 3u> StateNames =
    {
        "Follow", "Zoom", "Reset"
    };

    //follows just above the target
    constexpr glm::vec3 TargetOffset(0.f, 0.2f, 0.f);

    constexpr float CameraTrackTime = 0.25f; //approx delay in tracking interpolation
}

CameraFollowSystem::CameraFollowSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(CameraFollowSystem)),
    m_closestCamera (CameraID::Player)
{
    requireComponent<cro::Transform>();
    requireComponent<cro::Camera>();
    requireComponent<CameraFollower>();

#ifdef CRO_DEBUG_
    registerWindow([&]()
        {
            if (ImGui::Begin("Cameras"))
            {
                ImGui::Text("Closest %s", CamNames[m_closestCamera].c_str());
                ImGui::Text("Active %s, %3.3f", CamNames[m_currentCamera.getComponent<CameraFollower>().id].c_str(), m_currentCamera.getComponent<CameraFollower>().currentFollowTime);
                //ImGui::Text("Tracking %s", m_currentCamera.getComponent<CameraFollower>().trackingPaused ? "false" : "true");

                ImGui::Separator();

                /*const auto& ents = getEntities();
                for (auto ent : ents)
                {
                    const auto& cam = ent.getComponent<CameraFollower>();
                    ImGui::Text("%s, state %s", CamNames[cam.id].c_str(), StateNames[cam.state].c_str());
                }*/
            }
            ImGui::End();        
        },true);
#endif
}

//public
void CameraFollowSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::GolfMessage)
    {
        const auto& data = msg.getData<GolfEvent>();
        if (data.type == GolfEvent::SetNewPlayer)
        {
            m_closestCamera = CameraID::Player; //reset to player camera
        }
    }
    else if (msg.id == MessageID::CollisionMessage)
    {
        const auto& data = msg.getData<CollisionEvent>();
        if (data.type == CollisionEvent::Begin
            && (data.terrain == TerrainID::Bunker
                || data.terrain == TerrainID::Rough
                || (cro::Util::Random::value(0, 3) == 0)))
        {
            auto ent = getScene()->createEntity();
            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().setUserData<float>(0.5f);
            ent.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;
                if (currTime < 0.f)
                {
                    auto& ents = getEntities();
                    for (auto ent : ents)
                    {
                        //a bit kludgy but we don't want this on the green cam
                        if (/*m_closestCamera == CameraID::Sky &&*/
                            ent.getComponent<CameraFollower>().id == CameraID::Sky)
                        {
                            ent.getComponent<CameraFollower>().state = CameraFollower::Zoom;
                        }
                    }

                    e.getComponent<cro::Callback>().active = false;
                    getScene()->destroyEntity(e);
                }
            };
        }
    }
}

void CameraFollowSystem::process(float dt)
{
    float currDist = std::numeric_limits<float>::max();
    auto lastCam = m_closestCamera;
    cro::Entity currentCam;

    auto& currentFollower = m_currentCamera.getComponent<CameraFollower>();
    currentFollower.currentFollowTime -= dt;


    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& follower = entity.getComponent<CameraFollower>();

        if (!follower.target.isValid())
        {
            continue;
        }

        switch (follower.state)
        {
        default: break;
        case CameraFollower::Track:
        {
            auto& tx = entity.getComponent<cro::Transform>();

            auto ballPos = follower.target.getComponent<cro::Transform>().getWorldPosition();
            auto target = ballPos + TargetOffset;
            CRO_ASSERT(!std::isnan(target.x), "Target pos is NaN");

            follower.currentTarget = cro::Util::Maths::smoothDamp(follower.currentTarget, target, follower.velocity, CameraTrackTime + ((CameraTrackTime / 2.f) * follower.zoom.progress), dt);

            auto worldPos = tx.getWorldPosition();
            tx.setRotation(lookRotation(worldPos, follower.currentTarget));

            //check the distance to the ball, and store it if closer than previous dist
            //and if we fall within the camera's radius
            //and if the player isn't standing too close
            const auto& collider = follower.target.getComponent<ClientCollider>();

            if ((currentFollower.currentFollowTime < 0)
                &&
                (collider.state == static_cast<std::uint8_t>(Ball::State::Flight) || collider.terrain == TerrainID::Green))
            {
                float positionMultiplier = 1.f;
                if (follower.id == CameraID::Green)
                {
                    positionMultiplier = 0.79f;
                }

                auto dist = glm::length2(worldPos - target);
                auto dist2 = glm::length2((worldPos - follower.playerPosition) * positionMultiplier);
                if (dist < currDist
                    && dist < follower.radius
                    && dist2 > follower.radius
                    && follower.id > m_closestCamera) //once we reach green don't go back until explicitly reset
                {
                    currDist = dist;

                    //special case to stop the overhead cam when on the green
                    if (collider.terrain != TerrainID::Green ||
                        follower.id == CameraID::Green)
                    {
                        m_closestCamera = follower.id;
                        currentCam = entity;
                    }
                }
            }

            //zoom if ball goes near the hole
            auto zoomDir = follower.holePosition - ballPos;
            auto dist = glm::length2(zoomDir);
            if (dist < follower.zoomRadius)
            {
                follower.state = CameraFollower::Zoom;
            }

            auto& targetInfo = entity.getComponent<TargetInfo>();
            if (targetInfo.waterPlane.isValid())
            {
                targetInfo.waterPlane.getComponent<cro::Callback>().setUserData<glm::vec3>(target.x, WaterLevel, target.z);
            }
        }
            break;
        case CameraFollower::Zoom:
            if (follower.zoom.done)
            {
                follower.state = CameraFollower::Track;
            }
            else
            {
                follower.currentFollowTime = 20.f;
                follower.zoom.progress = std::min(1.f, follower.zoom.progress + (dt *  follower.zoom.speed));
                follower.zoom.fov = glm::mix(1.f, follower.zoom.target, cro::Util::Easing::easeOutExpo(cro::Util::Easing::easeInQuad(follower.zoom.progress)));
                entity.getComponent<cro::Camera>().resizeCallback(entity.getComponent<cro::Camera>());

                if (follower.zoom.progress == 1)
                {
                    follower.zoom.done = true;
                    follower.state = CameraFollower::Track;
                }

                auto& tx = entity.getComponent<cro::Transform>();
                auto target = follower.target.getComponent<cro::Transform>().getWorldPosition() + TargetOffset;
                follower.currentTarget = cro::Util::Maths::smoothDamp(follower.currentTarget, target, follower.velocity, CameraTrackTime + ((CameraTrackTime / 2.f) * follower.zoom.progress), dt);


                tx.setRotation(lookRotation(tx.getWorldPosition(), follower.currentTarget));
            }
            break;
        }
    }

    //send a message with desired camera ID
    if (m_closestCamera != lastCam)
    {
        auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
        msg->type = SceneEvent::RequestSwitchCamera;
        msg->data = m_closestCamera;

        m_currentCamera.getComponent<CameraFollower>().currentFollowTime = -1.f;
        m_currentCamera = currentCam;
    }

    if (m_closestCamera == CameraID::Player)
    {
        m_currentCamera.getComponent<CameraFollower>().currentFollowTime = -1.f;
    }
}

void CameraFollowSystem::resetCamera()
{
    if (m_closestCamera != CameraID::Player)
    {
        m_closestCamera = CameraID::Player;

        auto* msg = postMessage<SceneEvent>(MessageID::SceneMessage);
        msg->type = SceneEvent::RequestSwitchCamera;
        msg->data = m_closestCamera;
    }

    for (auto entity : getEntities())
    {
        entity.getComponent<CameraFollower>().reset(entity);
        entity.getComponent<CameraFollower>().currentFollowTime = -1.f;
    }
    m_currentCamera.getComponent<CameraFollower>().currentFollowTime = -1.f;
}

//private
void CameraFollowSystem::onEntityAdded(cro::Entity e)
{
    //we need at least one valid value
    m_currentCamera = e;
}