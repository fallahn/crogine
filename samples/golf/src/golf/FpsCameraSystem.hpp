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

#include "Thumbsticks.hpp"

#include <crogine/core/GameController.hpp>
#include <crogine/core/String.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/GuiClient.hpp>

struct FpsCamera final
{
    static constexpr float MaxZoom = 0.25f; //multiplier of shared data FOV
    static constexpr float MinZoom = 1.f;

    float fov = MinZoom;
    float zoomProgress = 0.f; //0 - 1

    float cameraPitch = 0.f; //used to clamp camera
    float cameraYaw = 0.f; //used to calc forward vector

    float moveSpeed = 5.f; //units per second


    //if setting a transform manually on an entity which uses this component
    //call this once with the entity to reset the orientation to the new transform.
    void resetOrientation(cro::Entity entity)
    {
        auto rotation = glm::eulerAngles(entity.getComponent<cro::Transform>().getRotation());
        //cam.cameraPitch = rotation.x;
        cameraYaw = rotation.y;
    }
};

class CollisionMesh;
struct SharedStateData;
class FpsCameraSystem final : public cro::System, public cro::GuiClient
{
public:
    FpsCameraSystem(cro::MessageBus&, const CollisionMesh&, const SharedStateData&);

    void handleEvent(const cro::Event&);

    void process(float) override;

    void setHumanCount(std::int32_t c) { m_humanCount = c; }
    void setControllerID(std::int32_t p);
    void setLocation(const cro::String& l) { m_screenshotLocation = l; }

private:

    const CollisionMesh& m_collisionMesh;
    const SharedStateData& m_sharedData;

    std::int32_t m_humanCount;
    std::int32_t m_controllerID;

    struct Input final
    {
        enum Flags
        {
            Forward    = 0x1,
            Backward   = 0x2,
            Left       = 0x4,
            Right      = 0x8,
            LeftMouse  = 0x10,
            RightMouse = 0x20,
            Down       = 0x40,
            Up         = 0x80,
            ZoomIn     = 0x100,
            ZoomOut    = 0x200,
            Sprint     = 0x400,
            Walk       = 0x800,
        };

        std::int32_t wheel = 0;
        std::uint16_t buttonFlags = 0;
        std::uint16_t prevStick = 0; //previous flags used by left thumb stick
        std::int8_t xMove = 0;
        std::int8_t yMove = 0;
    }m_input;
    
    Thumbsticks m_thumbsticks;
    std::int16_t m_triggerAmount;

    float m_analogueMultiplier;
    float m_inputAcceleration;

    float m_forwardAmount;
    float m_sideAmount;
    float m_upAmount;

    cro::String m_screenshotLocation;

    void checkControllerInput(float);
    void onEntityAdded(cro::Entity) override;
};