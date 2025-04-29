/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/gui/GuiClient.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <functional>
#include <unordered_map>

namespace cro
{
    /*!
    \brief Button event data
    Contains the SDL button event data for button down and up events.
    This is passed to any ButtonCallbacks so that the user may
    handle the event based on its type as they wish.
    */
    struct ButtonEvent
    {
        std::uint32_t type = 0;                       //!< SDL_Event type
        union
        {
            SDL_KeyboardEvent key;             //!< SDL_KeyboardEvent data if this is a keyboard event
            SDL_MouseButtonEvent button;       //!< SDL_MouseButtonEvent data if this is a mouse button event
            SDL_JoyButtonEvent jbutton;        //!< SDL_JoyButtonEvent data if this is a joystick button event
            SDL_ControllerButtonEvent cbutton; //!< SDL_ControllerButtonEvent data if this is a controller button event
            SDL_TouchFingerEvent tfinger;      //!< SDL_TouchFingerEvent data if this is a touch event
        };
    };

    /*!
    \brief Motion event data
    Contains the SDL motion event data for any motion supported devices
    including the mouse, joystick or controller.
    This is passed to any MovementCallbacks so that the user may
    handle the event based on its type as they wish.
    */
    struct MotionEvent
    {
        std::uint32_t type = 0;                    //!< SDL_Event type
        union
        {
            SDL_MouseMotionEvent motion;    //!< Mouse motion event data
            SDL_JoyAxisEvent jaxis;         //!< Joystick axis event data
            SDL_JoyBallEvent jball;         //!< Joystick ball event data
            SDL_JoyHatEvent jhat;           //!< Joystick hat event data
            SDL_ControllerAxisEvent caxis;  //!< Game Controller axis event data
            SDL_MultiGestureEvent mgesture; //!< Gesture event data
            SDL_DollarGestureEvent dgesture;//!< Gesture event data
        };

        static constexpr std::uint32_t CursorEnter = std::numeric_limits<std::uint32_t>::max(); //!< event type when cursor deactivates an input
        static constexpr std::uint32_t CursorExit = CursorEnter - 1; //!< event type when cursor deactivates an input
    };

    class CRO_EXPORT_API UISystem final : public System, GuiClient
    {
    public:
        //passes in the entity for whom the callback was triggered and a ButtonEvent
        //containing the SDL event data for the device which triggered it
        using ButtonCallback = std::function<void(Entity, const ButtonEvent&)>;
        using MovementCallback = std::function<void(Entity, glm::vec2, const MotionEvent&)>;
        using SelectionChangedCallback = std::function<void(Entity)>;
        static constexpr std::int32_t ActiveControllerAll = -1;

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
        component.callbacks[UIInput::ButtonDown] = id;
        \endcode
        */
        std::uint32_t addCallback(const ButtonCallback&);

        /*!
        \brief Adds a mouse or touch input movement callback.
        This is similar to button event callbacks, only the movement delta is
        passed in as a parameter instead of a button ID. These are also used for
        mouse enter/exit events
        */
        std::uint32_t addCallback(const MovementCallback&);

        /*!
        \brief Adds a selection changed callback.
        These callbacks are executed when the currently selected UI input
        is unselected, or a new UI input is selected. These should be assigned
        to component.callbacks[UIInout::Selected] and component.callbacks[UIInput::Unselected]
        accordingly.
        */
        std::uint32_t addCallback(const SelectionChangedCallback&);

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

        /*!
        \brief Returns the number of UIInputs in the active group
        */
        std::size_t getGroupSize() const { return m_groups.at(m_activeGroup).size(); }

        /*
        \brief Set the number of columns displayed in the active group.
        By default the 'up' and 'down' controls move the selected control index
        in the same direction as the 'left' and 'right' controls - that is, +1
        or -1 from the currently selected index. In layouts where multiple
        columns of UI controls exists this feels counter-intuitive as the selected
        index does not visually move up and down the screen. By setting this value
        to the number of columns in the display the 'up' and 'down' controls will
        jump by +/- this number of indices, effectively moving an entire row at a
        time. This value is not saved per group so when setting a new group active
        that has a different number of columns, this function should be used to
        set the new active value.

        Note that this is ignored for any UIInput components which has a specific
        previous or next index set on them.

        \param count The number of columns in the display (or number of indices to
        skip when moving up/down). Must be greater than zero.
        */
        void setColumnCount(std::size_t count);

        /*!
        \brief Sets the controller ID allowed to send input to the system.
        Only controller events with the controller ID will be forwarded to
        the UI callbacks, or all events if this is set to ActiveControllerAll
        \param id The ID of the controller to set active. Should be 0 - 3 or
        ActiveControllerAll (default)
        */
        void setActiveControllerID(std::int32_t id);

        /*!
        \brief Enables or disables navigation with mouse scroll.
        By default scrolling the mouse wheel will navigate the selected
        UIInput to the next available active UIInput. Setting this to false
        will disabled this behaviour.
        */
        void setMouseScrollNavigationEnabled(bool enabled) { m_scrollNavigation = enabled; }

        /*!
        \brief Returns whether or not mouse scroll navigation is currently enabled.
        Set to true by default.
        \see setMouseScrollNavigationEnabled()
        */
        bool getMouseScrollNavigationEnabled() const { return m_scrollNavigation; }

        /*!
        \brief Selects the input at the given index, or the last
        input if the index exceeds the number of active inputs in the current group
        \param index The index of the input to select
        Note that this may not give expected results if multiple inputs
        have been manually assigned the same selection index with setSelectionIndex()
        or the index is not in range of the group size - in which case prefer
        selectByIndex()
        */
        void selectAt(std::size_t index);

        /*!
        \brief Selects at a specific index if a UIInput component has been provided
        with one via setSelectionIndex() - otherwise prefer selectAt()
        */
        void selectByIndex(std::size_t index);

        /*!
        \brief Initialise the debug window for this instance if #DEBUG_UI is currently defined
        \param label unique ID for this instance to identify the debug window (sets the title)
        */
        void initDebug(const std::string& label) const;

    private:

        std::vector<ButtonCallback> m_buttonCallbacks;
        std::vector<MovementCallback> m_movementCallbacks;
        std::vector<SelectionChangedCallback> m_selectionCallbacks;

        glm::vec2 m_prevMousePosition = glm::vec2(0.f);
        glm::vec2 m_previousEventPosition = glm::vec2(0.f); //in screen coords
        glm::vec2 m_eventPosition = glm::vec2(0.f);
        glm::vec2 m_movementDelta = glm::vec2(0.f); //in world coords

        std::vector<ButtonEvent> m_buttonDownEvents;
        std::vector<ButtonEvent> m_mouseDownEvents;
        std::vector<ButtonEvent> m_buttonUpEvents;
        std::vector<ButtonEvent> m_mouseUpEvents;
        std::vector<MotionEvent> m_motionEvents;

        struct HoldEvent final
        {
            static constexpr float HoldTime = 0.25f;
            static constexpr float MinHoldTime = 0.05f;

            float currentHoldTime = HoldTime;
            float timer = HoldTime;
            bool active = false;

            void start()
            {
                active = true;
                currentHoldTime = HoldTime;
                timer = HoldTime;
            }
        };
        std::array<HoldEvent, 4u> m_keyHoldEvents = {};
        std::array<HoldEvent, 4u> m_buttonHoldEvents = {};

        glm::uvec2 m_windowSize;

        //void setViewPort(std::int32_t, std::int32_t);
        glm::vec2 toWorldCoords(std::int32_t x, std::int32_t y); //converts screen coords
        glm::vec2 toWorldCoords(float, float); //converts normalised coords

        std::int32_t m_activeControllerID;
        std::uint8_t m_controllerMask;
        std::uint8_t m_prevControllerMask;
        enum ControllerBits
        {
            Up = 0x1, Down = 0x2, Left = 0x4, Right = 0x8
        };

        bool m_scrollNavigation;
        std::size_t m_columnCount;
        std::size_t m_selectedIndex;

        std::int32_t m_prevDirection;
        std::size_t m_previousIndex;

        void selectNext(std::size_t, std::int32_t = UIInput::Index::Right);
        void selectPrev(std::size_t, std::int32_t = UIInput::Index::Left);

        void unselect(std::size_t, bool wasMouseEvent = false);
        void select(std::size_t, bool wasMouseEvent = false);


        std::unordered_map<std::size_t, std::vector<Entity>> m_groups;
        std::size_t m_activeGroup;

        void updateGroupAssignments();

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

        struct DebugContext final
        {
            std::size_t selectedIndex = 0;
            std::size_t upIndex = 0;
            std::size_t downIndex = 0;
            std::size_t leftIndex = 0;
            std::size_t rightIndex = 0;

            std::size_t backIndex = 0;
        }m_debugContext;
    };
}