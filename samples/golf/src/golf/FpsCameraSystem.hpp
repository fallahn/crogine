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

#include <crogine/core/GameController.hpp>
#include <crogine/ecs/System.hpp>

/*
First person camera controller. Probably works as 3rd person too with some adjustments
*/

struct FpsCamera final
{
    float cameraPitch = 0.f; //used to clamp camera
    float cameraYaw = 0.f; //used to calc forward vector

    float moveSpeed = 5.f; //units per second

    std::int32_t controllerIndex = 0; //which controller to accept input from. Keyboard input is always sent to controller 0

    bool flyMode = true;
};


class FpsCameraSystem final : public cro::System
{
public:
    explicit FpsCameraSystem(cro::MessageBus&);

    void handleEvent(const cro::Event&);

    void process(float) override;

    //TODO allow adding keybinds to controller index mapping

private:

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