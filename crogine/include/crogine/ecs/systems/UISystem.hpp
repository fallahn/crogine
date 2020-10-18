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

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <functional>

namespace cro
{
    /*!
    \brief Button event data
    Contains the SDL button event data for button down and up events.
    This is passed to any ButtonCallbacks so that the user may
    handle the event based on its type as they wish.
    */
    union ButtonEvent
    {
        uint32 type = 0;                   //!< SDL_Event type
        SDL_KeyboardEvent key;             //!< SDL_KeyboardEvent data if this is a keyboard event
        SDL_MouseButtonEvent button;       //!< SDL_MouseButtonEvent data if this is a mouse button event
        SDL_JoyButtonEvent jbutton;        //!< SDL_JoyButtonEvent data if this is a joystick button event
        SDL_ControllerButtonEvent cbutton; //!< SDL_ControllerButtonEvent data if this is a controller button event
        SDL_TouchFingerEvent tfinger;      //!< SDL_TouchFingerEvent data if this is a touch event
    };

    /*!
    \brief Motion event data
    Contains the SDL motion event data for any motion supported devices
    including the mouse, joystick or controller.
    This is passed to any MovementCallbacks so that the user may
    handle the event based on its type as they wish.
    */
    union MotionEvent
    {
        uint32 type = 0;                //!< SDL_Event type
        SDL_MouseMotionEvent motion;    //!< Mouse motion event data
        SDL_JoyAxisEvent jaxis;         //!< Joystick axis event data
        SDL_JoyBallEvent jball;         //!< Joystick ball event data
        SDL_JoyHatEvent jhat;           //!< Joystick hat event data
        SDL_ControllerAxisEvent caxis;  //!< Game Controller axis event data
        SDL_MultiGestureEvent mgesture; //!< Gesture event data
        SDL_DollarGestureEvent dgesture;//!< Gesture event data

        static constexpr uint32 CursorExit = std::numeric_limits<uint32>::max(); //!< event type when cursor deactivates an input
    };

    class CRO_EXPORT_API UISystem final : public System
    {
    public:
        //passes in the entity for whom the callback was triggered and a ButtonEvent
        //containing the SDL event data for the device which triggered it
        using ButtonCallback = std::function<void(Entity, ButtonEvent event)>;
        using MovementCallback = std::function<void(Entity, glm::vec2, MotionEvent event)>;

        explicit UISystem(MessageBus&);

        /*!
        \brief Used to pass in any events the system may be interested in
        This must be called from a State's even handler for the UI to work
        */
        void handleEvent(const Event&);

        /*!
        \brief Performs processing
        */
        void process(float) override;

        /*!
        \brief Message handler
        */
        void handleMessage(const Message&) override;

        /*!
        \brief Adds a button event callback.
        \returns ID of the callback. This should be used to assigned the callback
        to the relative callback slot of a UIInput component. eg:
        \begincode
        auto id = system.addCallback(cb);
        component.callbacks[UIInput::MouseDown] = id;
        \endcode
        */
        uint32 addCallback(const ButtonCallback&);

        /*!
        \brief Adds a mouse or touch input movement callback.
        This is similar to button event callbacks, only the movement delta is
        passed in as a parameter instead of a button ID. These are also used for
        mouse enter/exit events
        */
        uint32 addCallback(const MovementCallback&);

        /*!
        \brief Sets the active group of UIInput components.
        By default all components are added to group 0. If a single
        UISystem handles multiple menus, for example, UIInput components
        can be grouped by a given index, one for each menu. This function
        will set the active group of UIInput components to receive events.
        \see cro::UIInput::setGroup()
        */
        void setActiveGroup(std::size_t);

        /*!
        \brief Returns the current active group
        */
        std::size_t getActiveGroup() const { return m_activeGroup; }


    private:

        std::vector<ButtonCallback> m_buttonCallbacks;
        std::vector<MovementCallback> m_movementCallbacks;

        glm::vec2 m_prevMousePosition = glm::vec2(0.f);
        glm::vec2 m_previousEventPosition = glm::vec2(0.f); //in screen coords
        glm::vec2 m_eventPosition = glm::vec2(0.f);
        glm::vec2 m_movementDelta = glm::vec2(0.f); //in world coords

        std::vector<ButtonEvent> m_downEvents;
        std::vector<ButtonEvent> m_upEvents;
        std::vector<MotionEvent> m_motionEvents;

        glm::uvec2 m_windowSize;

        //void setViewPort(int32, int32);
        glm::vec2 toWorldCoords(int32 x, int32 y); //converts screen coords
        glm::vec2 toWorldCoords(float, float); //converts normalised coords



        std::unordered_map<std::size_t, std::vector<Entity>> m_groups;
        std::size_t m_activeGroup;

        void updateGroupAssignments();

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;
    };
}