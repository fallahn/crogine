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

#pragma once

#include "GameConsts.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/detail/glm/vec3.hpp>

//updates specator cameras to follow the active ball

struct CameraID final
{
    enum
    {
        Player, Sky, Green, Transition,
        Count
    };
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

    cro::Entity waterPlane;
};

struct CameraFollower final
{
    enum State
    {
        Track,
        Zoom,
        Reset
    }state = Track;

    cro::Entity target;
    glm::vec3 currentTarget = glm::vec3(0.f); //used to interpolate
    glm::vec3 holePosition = glm::vec3(0.f);
    glm::vec3 playerPosition = glm::vec3(0.f);
    float radius = 0.f; //camera becomes active when ball within this (should be ^2)

    std::int32_t id = -1;

    struct ZoomData final
    {
        float target = 0.25f;
        float fov = 1.f;
        float progress = 0.f;
        float speed = 1.f;
        bool done = false;
    }zoom;
};

class CameraFollowSystem final : public cro::System
{
public:
    explicit CameraFollowSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;
    void process(float) override;

private:
    std::int32_t m_closestCamera;
};