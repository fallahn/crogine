/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>

#include <SDL_gamecontroller.h>

namespace cro
{
    /*!
    \brief Class for querying realtime status of connected controllers
    */
    class CRO_EXPORT_API GameController final
    {
    public:

        enum
        {
            AxisRightX = SDL_CONTROLLER_AXIS_RIGHTX,
            AxisRightY = SDL_CONTROLLER_AXIS_RIGHTY,
            AxisLeftX = SDL_CONTROLLER_AXIS_LEFTX,
            AxisLeftY = SDL_CONTROLLER_AXIS_LEFTY,
            TriggerLeft = SDL_CONTROLLER_AXIS_TRIGGERLEFT,
            TriggerRight = SDL_CONTROLLER_AXIS_TRIGGERRIGHT
        };

        enum
        {
            ButtonA = SDL_CONTROLLER_BUTTON_A,
            ButtonB = SDL_CONTROLLER_BUTTON_B,
            ButtonX = SDL_CONTROLLER_BUTTON_X,
            ButtonY = SDL_CONTROLLER_BUTTON_Y,
            ButtonBack = SDL_CONTROLLER_BUTTON_BACK,
            ButtonGuide = SDL_CONTROLLER_BUTTON_GUIDE,
            ButtonStart = SDL_CONTROLLER_BUTTON_START,
            ButtonLeftStick = SDL_CONTROLLER_BUTTON_LEFTSTICK,
            ButtonRightStick = SDL_CONTROLLER_BUTTON_RIGHTSTICK,
            ButtonLeftShoulder= SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            ButtonRightShoulder = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
            DPadUp = SDL_CONTROLLER_BUTTON_DPAD_UP,
            DPadDown = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
            DPadLeft = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            DPadRight = SDL_CONTROLLER_BUTTON_DPAD_RIGHT
        };

        /*!
        \brief Returns the current value of the requested axis on the request
        controller index, if it exists, else returns zero.
        \param controllerIndex Index of the controller to query
        \param controllerAxis ID of the controller axis to query, for example 
        GameController::AxisLeftX or GameController::TriggerRight.
        \returns value in range -32768 to 32767 or 0 to 32767 for triggers
        */
        static int16 getAxis(int32 controllerIndex, int32 controllerAxis);

        /*!
        \brief Returns whether or not given button at the given controller
        index is currently pressed.
        \param controllerIndex Index of the connected controller to query
        \param button ID of the button to query, for example GameController::ButtonA
        or GameController::DPadUp
        \returns true if the button is pressed or false if the button is not pressed
        or the given controllerIndex or button ID does not exist.
        */
        static bool isButtonPressed(int32 controllerIndex, int32 button);

    private:

    };
}