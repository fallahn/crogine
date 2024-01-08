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

#pragma once

#include <crogine/core/GameController.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/components/Transform.hpp>

/*
First/third person camera controller.
Attach this directly to an entity which has a Camera component for first
person view, or attach a Camera to a child entity of the camera controller
and place it appropriately to create a third person view.
*/

struct FpsCamera final
{
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
class FpsCameraSystem final : public cro::System
{
public:
    FpsCameraSystem(cro::MessageBus&, const CollisionMesh&);

    void handleEvent(const cro::Event&);

    void process(float) override;

    //TODO allow adding keybinds to controller index mapping

private:

    const CollisionMesh& m_collisionMesh;

    struct Input final
    {
        enum Flags
        {
            Forward = 0x1,
            Backward = 0x2,
            Left = 0x4,
            Right = 0x8,
            LeftMouse = 0x10,
            RightMouse = 0x20,
            Crouch = 0x40,
            Jump = 0x80
        };

        std::uint32_t timeStamp = 0;
        std::uint16_t buttonFlags = 0;
        std::int8_t xMove = 0;
        std::int8_t yMove = 0;
    };
    std::array<Input, cro::GameController::MaxControllers> m_inputs;

    void onEntityAdded(cro::Entity) override;
};