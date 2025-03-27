/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include <crogine/graphics/Colour.hpp>
#include <crogine/detail/Types.hpp>

#include <SDL_gamecontroller.h>
#include <SDL_haptic.h>

#include <string>

namespace cro
{
    /*!
    \brief Utility function for creating SDL_HapticEffect initialised to safe values
    \see SDL_HapticEffect
    */
    static inline SDL_HapticEffect createHapticEffect()
    {
        SDL_HapticEffect effect;
        SDL_memset(&effect, 0, sizeof(SDL_HapticEffect));

        effect.type = SDL_HAPTIC_SINE;
        effect.periodic.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
        effect.periodic.direction.dir[0] = 18000; // Force comes from south
        effect.periodic.period = 1000; // 1000 ms
        effect.periodic.magnitude = 20000; // 20000/32767 strength
        effect.periodic.length = 5000; // 5 seconds long
        effect.periodic.attack_length = 1000; // Takes 1 second to get max strength
        effect.periodic.fade_length = 1000; // Takes 1 second to fade away

        return effect;
    }

    /*!
    \brief Contains a haptic effect ID and the controller index with which it was registered.
    This struct is returned from registerHapticEffect() and can be used with startHapticEffect()
    getHapticStatus() and stopHapticEffect()
    */
    struct HapticEffect final
    {
        std::int32_t controllerIndex = -1;
        std::int32_t effectID = -1;
    };

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
            DPadRight = SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
            ButtonTrackpad = SDL_CONTROLLER_BUTTON_TOUCHPAD,
            PaddleL4 = SDL_CONTROLLER_BUTTON_PADDLE2,
            PaddleL5 = SDL_CONTROLLER_BUTTON_PADDLE4,
            PaddleR4 = SDL_CONTROLLER_BUTTON_PADDLE1,
            PaddleR5 = SDL_CONTROLLER_BUTTON_PADDLE3,
        };

        enum
        {
            DSTriggerLeft  = 0x1,
            DSTriggerRight = 0x2,
            DSTriggerBoth  = DSTriggerLeft | DSTriggerRight
        };

        enum
        {
            DSModeOff            = 0x05,
            DSModeFeedback       = 0x21,
            DSModeWeapon         = 0x25,
            DSModeVibrate        = 0x26,

            DSModeFeedbackSimple = 0x01,
            DSModeWeaponSimple   = 0x02,
            DSModeVibrateSimple  = 0x06
        };

        /*!
        \brief Used to supply effect parameters to DualSense trigger effects
        */
        struct CRO_EXPORT_API DSEffect final
        {
            //DSMode enum
            std::uint8_t mode = DSModeOff;

            //params vary based on the above mode
            //so factory functions are provided for
            //creating a variety of presets (below)
            std::array<std::uint8_t, 10> params = {};

            DSEffect() { std::fill(params.begin(), params.end(), 0); }
            explicit DSEffect(std::uint8_t dsMode) : mode(dsMode) { std::fill(params.begin(), params.end(), 0); }

            /*!
            \brief Feedback effects resist movement atfer a given position
            \param position Value between 0-9 at which to start resisting movement
            \param strength The amount of resistance to apply. 0-8
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createFeedback(std::uint8_t position, std::uint8_t strength);

            /*!
            \brief Weapon effects resist movement between the given start and end position.
            This is usually used to emulate the trigger of a gun for example.
            \param startPosition The position at which the effect starts. Must be between 2-7
            \param endPosition The position at which the effect ends. Must be between startPosition +1
            and 8
            \param strength The strength of the resistance to apply. 0-8
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createWeapon(std::uint8_t startPosition, std::uint8_t endPosition, std::uint8_t strength);

            /*!
            \brief Creates a vibration effect when the trigger is pressed beyond the given position
            \param position The starting point for the effect. 0-9
            \param strength The strength or amplitude of the vibration. 0-8
            \param frequency The frequency of the vibration in hertz
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createVibration(std::uint8_t position, std::uint8_t strength, std::uint8_t frequency);

            /*!
            \brief Creates 10 regions of resistance when pressing the trigger.
            \param values An array of 10 values between 0-8 representing the strength of each region
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createMultiFeedback(const std::array<std::uint8_t, 10u>& values);

            /*!
            \brief Creates a linear range of resistance from the given start position to the given
            end position interpolated between the given start strength and the given end strength
            \param startPosition The starting position of the effect 0-8
            \param endPosition The end point of the effect (startPosition+1) - 9
            \param startStrength The amount of resistance at the start point 1-8
            \param endStrength The amount of resistance at the endPoint 1-8
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createSlopeFeedback(std::uint8_t startPosition, std::uint8_t endPosition, std::uint8_t startStrength, std::uint8_t endStrength);

            /*!
            \brief Creates a vibration effect of 10 varying strengths at the same frequency
            \param strengths An array of 10 values representing the 10 ampltude/strengths. Values must be 0-8
            \param frquency The frequency of the effect in hertz
            \returns DSEffect object to use with appyDSTriggerEffect()
            */
            static DSEffect createMultiVibration(const std::array<std::uint8_t, 10u>& strengths, std::uint8_t frequency);
        };

        static constexpr std::int16_t AxisMax = 32767;
        static constexpr std::int16_t AxisMin = -32768;

        template <std::int16_t Size>
        struct DeadZone final
        {
            static constexpr std::int16_t size = Size;
            
            void setOffset(std::int16_t o)
            {
                offset = std::clamp(o, std::int16_t(-Size), std::int16_t((AxisMax - 100) - Size));
            }

            std::int16_t getOffset() const { return offset; }
            operator std::int32_t() const { return Size + offset; }

            //delete these so that the type conversion operator is the only one available
            DeadZone& operator = (const DeadZone&) = delete;
            DeadZone& operator = (DeadZone&&) = delete;
            
        private:
            std::int16_t offset = 0;
        };

        //these are the values in the DX SDK for XInput
        static constexpr std::int16_t LeftThumbDeadZoneV = 8700;// 7849;
        static constexpr std::int16_t RightThumbDeadZoneV = 8689;
        static constexpr std::int16_t TriggerDeadZoneV = 30;
        
        static DeadZone<LeftThumbDeadZoneV> LeftThumbDeadZone;
        static DeadZone<RightThumbDeadZoneV> RightThumbDeadZone;
        static DeadZone<TriggerDeadZoneV> TriggerDeadZone;

        /*!
        \brief Maximum number of game controllers which may be connected
        Note that this does not include joystick devices.
        */
        static constexpr std::int32_t MaxControllers = 12;

        /*!
        \brief Returns the event ID associated with the controller at the given index
        Events such as SDL_CONTROLLERBUTTONDOWN do not contain the ControllerID in the
        button.which field, rather the underlying ID of the device. This function returns
        that ID currently mapped to the given controller index (which may be -1 if the
        controller is currently disconnected) and can be compared the the event data
        to see which controller raise it.
        \begincode
            if(evt.button.which == cro::GameController::deviceID(controllerID))
            {
                //handle the event
            }
        \endcode
        \param controllerID The ID of the controller, usually  0 - 3
        \returns deviceID The ID of the device which corresponds to the given controller
        */
        static std::int32_t deviceID(std::int32_t controllerID);

        /*!
        \brief Returns the ControllerID (0-3) from the given joystick index, if it is connected.
        If the joystick ID is not valid this function returns -1. Use it to find the ControllerID
        of a joystick event such as SDL_CONTROLLERBUTTONDOWN
        \begincode
        auto id = controllerID(evt.cbutton.which);
        \endcode
        \param joystickID The ID from which to retrieve the controller ID
        \returns 0-3 on success or -1 if the joystick is not a GameController
        */
        static std::int32_t controllerID(std::int32_t joystickID);

        /*!
        \brief Returns the current value of the requested axis on the requested
        controller index, if it exists, else returns zero.
        \param controllerIndex Index of the controller to query
        \param controllerAxis ID of the controller axis to query, for example 
        GameController::AxisLeftX or GameController::TriggerRight.
        \returns value in range -32768 to 32767 or 0 to 32767 for triggers
        */
        static std::int16_t getAxisPosition(std::int32_t controllerIndex, std::int32_t controllerAxis);

        /*!
        \brief Returns whether or not given button at the given controller
        index is currently pressed.
        \param controllerIndex Index of the connected controller to query
        \param button ID of the button to query, for example GameController::ButtonA
        or GameController::DPadUp
        \returns true if the button is pressed or false if the button is not pressed
        or the given controllerIndex or button ID does not exist.
        */
        static bool isButtonPressed(std::int32_t controllerIndex, std::int32_t button);

        /*!
        \brief Returns whether or not the controller with the given ID is currently
        connected
        \param controllerIndex The controller ID to query
        \returns bool True if connected else false
        */
        static bool isConnected(std::int32_t controllerIndex);

        /*!
        \brief Returns true if the controller with the given ID supports haptic
        or force feedback
        \param controllerIndex The controller ID to query
        \returns bool true if haptic supported, else false
        */
        static bool hasHapticSupport(std::int32_t controllerIndex);

        /*!
        \brief Registers an SDL_HapticEffect with the game controller at the given index
        \param controllerIndex The ID of the controller to which to assign the effect
        \param effect An SDL_HapticEffect struct populated with valid values. Use
        createHapticEffect() to get an instance with reasonable defaults.
        \returns A HapticEffect instance which contains the ID of the effect if successful
        and the associated controller index. If haptics are not available or registering failed
        for some reason then the effectID is -1.
        The returned ID is only valid for the controller ID with which it was registered.
        Register the SDL effect with each controller that the effect should be used with to
        first determine if the controller at that index supports haptic effects.
        */
        static HapticEffect registerHapticEffect(std::int32_t controllerIndex, SDL_HapticEffect& effect);

        /*!
        \brief Starts a haptic effect on the given controller using the given effect ID
        \param effect A HapticEffect instance returned by previously calling registerHapticEffect()
        \param repeat The number of times to repeat the effect before stopping, or SDL_HAPTIC_INFINITY
        */
        static void startHapticEffect(HapticEffect effect, std::uint32_t repeat = 1);

        /*!
        \brief Stops a haptic effect if it is currently active.
        \param effect The HapticEffect to stop, which had previously been used with startHapticEffect()
        */
        static void stopHapticEffect(HapticEffect effect);

        /*!
        \brief Returns true if the given effect is currently active, else returns false
        \param effect HapticEffect to query
        */
        static bool isHapticActive(HapticEffect effect);

        /*!
        \brief Starts a rumble effect on the controller with the given index, if it is supported
        \param controllerIndex The index of the controller to start rumbling
        \param strengthLow Strength of the low frequency motor from 0 - 65335
        \param strengthHigh Strength of the high frequency motor from 0 - 65335
        \param duration The duration in milliseconds to rumble.
        Currently not working correctly in SDL 2.0.16, see https://github.com/libsdl-org/SDL/issues/4435
        */
        static void rumbleStart(std::int32_t controllerIndex, std::uint16_t strengthLow, std::uint16_t strengthHigh, std::uint32_t duration);

        /*!
        \brief Stops the rumble effect on the given controller, if it is active
        \param controllerIndex The index of the controller to stop
        */
        static void rumbleStop(std::int32_t controllerIndex);

        /*!
        \brief Returns a string containing the identifier associated with the given controller ID
        */
        static std::string getName(std::int32_t controllerIndex);

        /*!
        \brief returns the printable (short) name of the controller at the given index
        */
        static const cro::String& getPrintableName(std::int32_t controllerIndex);

        /*!
        \brief Returns the number of currently connected GameControllers.
        Note that this does not include any Joystick devices.
        */
        static std::int32_t getControllerCount();

        /*!
        \brief Returns true if the controller at the given index was detected
        to have a playstation style layout.
        This may not always be accurate as detection is based on the controller
        name string which was read at the time of the controller connection
        */
        static bool hasPSLayout(std::int32_t controllerIndex);

        /*!
        \brief Sets the LED colour of the controller to the given colour
        Only works on supported controllers, else has no effect
        \param controllerIndex Controller ID to apply the colour to
        \param colour A cro::Colour containing the values with which to set the LED
        */
        static void setLEDColour(std::int32_t controllerIndex, cro::Colour colour);

        /*!
        \brief Applies a trigger effect to the given controller if it is a DualSense
        compatible controller else does nothing.
        \param controllerIndex Which controller to apply the effect to
        \param triggers DSTrigger flags for which triggers to affect
        \param settings DSEffect struct containing the parameters for the desired effect.
        Note that a default constructed DSEEffect will reset the trigger state to none
        \returns true if the effect was applied else false if the given controller index
        was not valid or the controller is not a DualSense controller
        */
        static bool applyDSTriggerEffect(std::int32_t controllerIndex, std::int32_t triggers, const DSEffect& settings);

        /*!
        \brief Returns the player index of the last controller to recieve an event
        */
        static std::int32_t getLastControllerID() { return m_lastControllerIndex; }

        /*!
        \brief Returns thet Steam input ID of this controller if we're on a Steam deck
        else returns zero
        */
        static  std::uint64_t getSteamHandle(std::int32_t controllerID);

    private:
        friend class App;
        static std::int32_t m_lastControllerIndex; //tracks which controller last had input
    };
}