/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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
//#define CAMERA_TRACK

#include "GameConsts.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/gui/GuiClient.hpp>

//updates specator cameras to follow the active ball

struct CameraID final
{
    enum
    {
        Player, Bystander, Sky, Green,
        Transition, Idle, Drone,
        Count
    };
};

struct PostProcess final
{
    cro::Shader* shader = nullptr;
    std::vector<std::pair<std::string, cro::TextureID>> uniforms;
};

//used to set the camera target
struct TargetInfo final
{
    float targetHeight = CameraStrokeHeight;
    float targetOffset = CameraStrokeOffset;
    float startHeight = 0.f;
    float startOffset = 0.f;

    glm::vec3 targetLookAt = glm::vec3(0.f);
    glm::vec3 currentLookAt = glm::vec3(0.f);
    glm::vec3 prevLookAt = glm::vec3(0.f);
    glm::vec3 finalLookAt = glm::vec3(0.f); //final lookat point with any height offset added (see updateCameraHeight())
    glm::vec3 finalLookAtOffset = glm::vec3(0.f); //allow moving back by any offset

    cro::Entity waterPlane;
    PostProcess* postProcess = nullptr;
};

struct CameraFollower final
{
    enum State
    {
        Track,
        Zoom
    }state = Track;

    cro::Entity target;
    glm::vec3 currentTarget = glm::vec3(0.f); //used to interpolate
    glm::vec3 velocity = glm::vec3(0.f); //used by the interpolation function
    glm::vec3 holePosition = glm::vec3(0.f); //used to tell if we should zoom
    glm::vec3 playerPosition = glm::vec3(0.f); //used to tell if we can be active (not if too close to player)
    float radius = 0.f; //camera becomes active when ball within this (should be ^2)
    float zoomRadius = 25.f; //dist^2 from hole when zoom becomes active

    static constexpr float MinFollowTime = 4.f;
    float currentFollowTime = 0.f;

    glm::vec3 targetOffset = glm::vec3(0.f); //aim is offset by this from target position

    std::int32_t id = -1;

    struct ZoomData final
    {
        float target = 0.25f;
        float fov = 1.f;
        float progress = 0.f;
        float speed = 1.f;
        bool done = false;
    }zoom;

    void reset(cro::Entity parent)
    {
        zoom.fov = 1.f;
        zoom.progress = 0.f;
        zoom.done = false;
        parent.getComponent<cro::Camera>().resizeCallback(parent.getComponent<cro::Camera>());

        state = CameraFollower::Track;
    }

    cro::Entity debugEntity;
    bool hadUpdate = false;
};

class CameraFollowSystem final : public cro::System, public cro::GuiClient
{
public:
    explicit CameraFollowSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(float) override;

    void setTargetGroup(std::uint8_t group) { m_targetGroup = group; }
    std::uint8_t getTargetGroup() const { return m_targetGroup; }

    //resets back to player cam and raises a message
    void resetCamera();

private:
    std::int32_t m_closestCamera;
    cro::Entity m_currentCamera;

    std::uint8_t m_targetGroup;

    void onEntityAdded(cro::Entity) override;
};