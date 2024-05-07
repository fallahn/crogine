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
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>


struct FpsCamera final
{
    float fov = 1.5f;

    float cameraPitch = 0.f; //used to clamp camera
    float cameraYaw = 0.f; //used to calc forward vector

    float moveSpeed = 3.f; //units per second
    float lookSensitivity = 0.5f; //0 - 1
    float yInvert = 1.f; //-1 to invert axis

    std::int32_t controllerIndex = 0; //which controller to accept input from. Keyboard input is always sent to controller 0

    bool flyMode = true;

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
struct InputBinding;
class FpsCameraSystem final : public cro::System
{
public:
    FpsCameraSystem(cro::MessageBus&, const CollisionMesh&, const InputBinding&);

    void handleEvent(const cro::Event&);

    void process(float) override;

    void setHumanCount(std::int32_t c) { m_humanCount = c; }
    void setControllerID(std::int32_t p);

private:

    const CollisionMesh& m_collisionMesh;
    const InputBinding& m_inputBinding;

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

        std::uint32_t timeStamp = 0;
        std::uint16_t buttonFlags = 0;
        std::uint16_t prevStick = 0; //previous flags used by left thumb stick
        std::int8_t xMove = 0;
        std::int8_t yMove = 0;
    }m_input;
    
    Thumbsticks m_thumbsticks;
    float m_analogueMultiplier;
    float m_inputAcceleration;

    void checkControllerInput(float);
    void onEntityAdded(cro::Entity) override;
};