/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/core/App.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/GameController.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

UISystem::UISystem(MessageBus& mb)
    : System            (mb, typeid(UISystem)),
    m_activeControllerID(ActiveControllerAll),
    m_controllerMask    (0),
    m_prevControllerMask(0),
    m_scrollNavigation  (true),
    m_columnCount       (1),
    m_selectedIndex     (0),
    m_groups            (1),
    m_activeGroup       (0)
{
    requireComponent<UIInput>();
    requireComponent<Transform>();

    //default callback for components which don't have one assigned
    m_buttonCallbacks.push_back([](Entity, ButtonEvent) {});
    m_movementCallbacks.push_back([](Entity, glm::vec2, MotionEvent) {});
    m_selectionCallbacks.push_back([](Entity) {});

    m_windowSize = App::getWindow().getSize();
}

void UISystem::handleEvent(const Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEWHEEL:
        if (m_scrollNavigation)
        {
            if (evt.wheel.x > 0)
            {
                selectNext(1);
            }
            else if (evt.wheel.x < 0)
            {
                selectPrev(1);
            }
            else if (evt.wheel.y > 0)
            {
                selectPrev(m_columnCount);
            }
            else if (evt.wheel.y < 0)
            {
                selectNext(m_columnCount);
            }
        }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        //check if this is the active controller and update
        //if necessary to a connected controller
        if (m_activeControllerID != ActiveControllerAll &&
            evt.cdevice.which == cro::GameController::deviceID(m_activeControllerID))
        {
            //controller IDs automatically shift down
            //so drop to the next lowest available
            m_activeControllerID = (std::max(0, m_activeControllerID - 1));
        }
        break;
    case SDL_MOUSEMOTION:
        m_eventPosition = toWorldCoords(evt.motion.x, evt.motion.y);
        m_movementDelta = m_eventPosition - m_prevMousePosition;
        m_prevMousePosition = m_eventPosition;
        {
            auto& motionEvent = m_motionEvents.emplace_back();
            motionEvent.type = evt.type;
            motionEvent.motion = evt.motion;
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        m_previousEventPosition = m_eventPosition;

        m_eventPosition = toWorldCoords(evt.button.x, evt.button.y);
        {
            auto& buttonEvent = m_mouseDownEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.button = evt.button;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        m_eventPosition = toWorldCoords(evt.button.x, evt.button.y);
        {
            auto& buttonEvent = m_mouseUpEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.button = evt.button;
        }
        break;
        /*
        NOTE!!!
        SDL registers both touch AND mouse events for a single touch
        on Android. Be warned this will execute the same callback twice!!
        */

    case SDL_FINGERMOTION:
        m_eventPosition = toWorldCoords(evt.tfinger.x, evt.tfinger.y);
        //TODO check finger IDs for gestures etc
        {
            auto& motionEvent = m_motionEvents.emplace_back();
            motionEvent.type = evt.type;
            motionEvent.mgesture = evt.mgesture;
        }

        break;
    case SDL_FINGERDOWN:
        m_eventPosition = toWorldCoords(evt.tfinger.x, evt.tfinger.y);
        m_previousEventPosition = m_eventPosition;
        {
            auto& buttonEvent = m_mouseDownEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.tfinger = evt.tfinger;
        }
        break;
    case SDL_FINGERUP:
        m_eventPosition = toWorldCoords(evt.tfinger.x, evt.tfinger.y);
        {
            auto& buttonEvent = m_mouseUpEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.tfinger = evt.tfinger;
        }
        break;
    case SDL_KEYDOWN:
    {
        auto& buttonEvent = m_buttonDownEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.key = evt.key;
    }
        break;
    case SDL_KEYUP:
    {
        switch (evt.key.keysym.sym)
        {
        default:
        {
            auto& buttonEvent = m_buttonUpEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.key = evt.key;
        }
            break;
        case SDLK_LEFT:
            selectPrev(1, UIInput::Index::Left);
            break;
        case SDLK_RIGHT:
            selectNext(1, UIInput::Index::Right);
            break;
        case SDLK_UP:
            selectPrev(m_columnCount, UIInput::Index::Up);
            break;
        case SDLK_DOWN:
            selectNext(m_columnCount, UIInput::Index::Down);
            break;
        }
    }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        if(m_activeControllerID == ActiveControllerAll ||
            evt.cbutton.which == cro::GameController::deviceID(m_activeControllerID))
        {
            switch (evt.cbutton.button)
            {
            default:
            {
                auto& buttonEvent = m_buttonDownEvents.emplace_back();
                buttonEvent.type = evt.type;
                buttonEvent.cbutton = evt.cbutton;
            }
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                selectPrev(m_columnCount, UIInput::Index::Up);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                selectNext(m_columnCount, UIInput::Index::Down);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                selectPrev(1, UIInput::Index::Left);
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                selectNext(1, UIInput::Index::Right);
                break;
            }
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (m_activeControllerID == ActiveControllerAll ||
            evt.cbutton.which == cro::GameController::deviceID(m_activeControllerID))
        {
            switch (evt.cbutton.button)
            {
            default:
            {
                auto& buttonEvent = m_buttonUpEvents.emplace_back();
                buttonEvent.type = evt.type;
                buttonEvent.cbutton = evt.cbutton;
            }
                break;
            }
        }
        break;
    case SDL_JOYBUTTONDOWN:
    {
        auto& buttonEvent = m_buttonDownEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.jbutton = evt.jbutton;
    }
        break;
    case SDL_JOYBUTTONUP:
    {
        auto& buttonEvent = m_buttonUpEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.jbutton = evt.jbutton;
    }
        break;

        //joystick and controller move events
    case SDL_CONTROLLERAXISMOTION:
        if (m_activeControllerID == ActiveControllerAll ||
            evt.caxis.which == cro::GameController::deviceID(m_activeControllerID))
        {
            static constexpr std::int16_t Threshold = cro::GameController::LeftThumbDeadZone;// 15000;
            switch (evt.caxis.axis)
            {
            default: break;
            case SDL_CONTROLLER_AXIS_LEFTX:
                if (evt.caxis.value > Threshold)
                {
                    //right
                    m_controllerMask |= ControllerBits::Right;
                    m_controllerMask &= ~ControllerBits::Left;
                }
                else if (evt.caxis.value < -Threshold)
                {
                    //left
                    m_controllerMask |= ControllerBits::Left;
                    m_controllerMask &= ~ControllerBits::Right;
                }
                else
                {
                    m_controllerMask &= ~(ControllerBits::Left | ControllerBits::Right);
                }
                break;
            case SDL_CONTROLLER_AXIS_LEFTY:
                if (evt.caxis.value > Threshold)
                {
                    //down
                    m_controllerMask |= ControllerBits::Down;
                    m_controllerMask &= ~ControllerBits::Up;
                }
                else if (evt.caxis.value < -Threshold)
                {
                    //up
                    m_controllerMask |= ControllerBits::Up;
                    m_controllerMask &= ~ControllerBits::Down;
                }
                else
                {
                    m_controllerMask &= ~(ControllerBits::Up | ControllerBits::Down);
                }
                break;
            }
        }
        break;
    //case SDL_JOYAXISMOTION:

    //    break;
    }
}

void UISystem::process(float)
{    
    //parse conrtoller inputs first
    auto diff = m_prevControllerMask ^ m_controllerMask;
    for (auto i = 0; i < 4; ++i)
    {
        auto flag = (1 << i);
        if (diff & flag)
        {
            //something changed
            if (m_controllerMask & flag)
            {
                //axis was pressed
                switch (flag)
                {
                default: break;
                case ControllerBits::Left:
                    selectPrev(1, UIInput::Index::Left);
                    break;
                case ControllerBits::Up:
                    selectPrev(m_columnCount, UIInput::Index::Up);
                    break;
                case ControllerBits::Right:
                    selectNext(1, UIInput::Index::Right);
                    break;
                case ControllerBits::Down:
                    selectNext(m_columnCount, UIInput::Index::Down);
                    break;
                }
            }
        }
    }
    m_prevControllerMask = m_controllerMask;
    //m_controllerMask = 0;

    updateGroupAssignments();

    //track this group, if a callback changes it we want to quit the
    //current loop else new groups have the existing events applied to them...
    auto currentGroup = m_activeGroup;

    std::size_t currentIndex = 0;
    for (auto& e : m_groups[m_activeGroup])
    {
        auto& input = e.getComponent<UIInput>();

        auto area = input.m_worldArea;
        bool contains = false;
        if (contains = area.contains(m_eventPosition) && !cro::App::getWindow().getMouseCaptured(); contains && input.enabled)
        {
            if (!input.active)
            {
                //mouse has entered
                input.active = true;
                MotionEvent m;
                m.type = MotionEvent::CursorEnter;
                m_movementCallbacks[input.callbacks[UIInput::Enter]](e, m_movementDelta, m);
            }

            //only fire these events if the selection actually changed.
            if (m_selectedIndex != currentIndex
                && glm::length2(m_movementDelta) != 0) //stops the hovering cursor modifying the selection
            {
                unselect(m_selectedIndex);
                m_selectedIndex = currentIndex;
                select(m_selectedIndex);
            }

            for (const auto& m : m_motionEvents)
            {
                m_movementCallbacks[input.callbacks[UIInput::Motion]](e, m_movementDelta, m);
            }
        }
        else
        {
            if (input.active)
            {
                //mouse left
                input.active = false;

                MotionEvent m;
                m.type = MotionEvent::CursorExit;
                m_movementCallbacks[input.callbacks[UIInput::Exit]](e, m_movementDelta, m);
            }
        }


        //only do mouse/touch events if they're within the bounds of an input
        if (contains && input.enabled)
        {
            for (const auto& f : m_mouseDownEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonDown]](e, f);
                if (currentGroup != m_activeGroup) goto updateEnd;
            }
            for (const auto& f : m_mouseUpEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonUp]](e, f);
                if (currentGroup != m_activeGroup) goto updateEnd;
            }
        }

        else if (currentIndex == m_selectedIndex && input.enabled)
        {
            for (const auto& f : m_buttonDownEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonDown]](e, f);
                if (currentGroup != m_activeGroup) goto updateEnd;
            }
            for (const auto& f : m_buttonUpEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonUp]](e, f);
                if (currentGroup != m_activeGroup) goto updateEnd;
            }
        }

        currentIndex++;
    }

    //DPRINT("Window Pos", std::to_string(m_eventPosition.x) + ", " + std::to_string(m_eventPosition.y));

updateEnd:
    m_previousEventPosition = m_eventPosition;
    m_mouseUpEvents.clear();
    m_buttonUpEvents.clear();
    m_mouseDownEvents.clear();
    m_buttonDownEvents.clear();
    m_movementDelta = {};
}

void UISystem::handleMessage(const Message& msg)
{
    if (msg.id == Message::WindowMessage)
    {
        const auto& data = msg.getData<Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            m_windowSize.x = data.data0;
            m_windowSize.y = data.data1;
        }
    }
}

std::uint32_t UISystem::addCallback(const ButtonCallback& cb)
{
    m_buttonCallbacks.push_back(cb);
    return static_cast<std::uint32_t>(m_buttonCallbacks.size() - 1);
}

std::uint32_t UISystem::addCallback(const MovementCallback& cb)
{
    m_movementCallbacks.push_back(cb);
    return static_cast<std::uint32_t>(m_movementCallbacks.size() - 1);
}

std::uint32_t UISystem::addCallback(const SelectionChangedCallback& cb)
{
    m_selectionCallbacks.push_back(cb);
    return static_cast<std::uint32_t>(m_selectionCallbacks.size() - 1);
}

void UISystem::setActiveGroup(std::size_t group)
{
    updateGroupAssignments();

    CRO_ASSERT(m_groups.count(group) != 0, "Group doesn't exist");
    CRO_ASSERT(!m_groups[group].empty(), "Group is empty");

    unselect(m_selectedIndex);

    m_activeGroup = group;
    m_selectedIndex = 0;

    select(m_selectedIndex);

    if (!m_groups[m_activeGroup][m_selectedIndex].getComponent<UIInput>().enabled)
    {
        selectNext(1);
    }

    //refresh transforms in case we came out of some sort of animation
    for (auto e : m_groups[m_activeGroup])
    {
        auto& input = e.getComponent<UIInput>();
        input.m_worldArea = input.area.transform(e.getComponent<Transform>().getWorldTransform());
    }
}

void UISystem::setColumnCount(std::size_t count)
{
    m_columnCount = std::max(std::size_t(1), count);
}

void UISystem::setActiveControllerID(std::int32_t id)
{
    id = std::max(0, id);

    if (GameController::isConnected(id))
    {
        m_activeControllerID = id;
    }
    m_controllerMask = 0;
}

void UISystem::selectAt(std::size_t index)
{
    if (index == m_selectedIndex)
    {
        return;
    }

    const auto& entities = m_groups[m_activeGroup];
    auto old = m_selectedIndex;

    do
    {
        m_selectedIndex = (m_selectedIndex + 1) % entities.size();
    } while (m_selectedIndex != index && m_selectedIndex != old);

    //and do selected callback
    if (m_selectedIndex != old
        && entities[m_selectedIndex].getComponent<UIInput>().enabled)
    {
        unselect(old);
        select(m_selectedIndex);
    }
}

void UISystem::selectByIndex(std::size_t index)
{
    const auto& entities = m_groups[m_activeGroup];
    auto old = m_selectedIndex;

    do
    {
        m_selectedIndex = (m_selectedIndex + 1) % entities.size();
    } while (entities[m_selectedIndex].getComponent<cro::UIInput>().getSelectionIndex() != index && m_selectedIndex != old);

    //and do selected callback
    if (m_selectedIndex != old
        && entities[m_selectedIndex].getComponent<UIInput>().enabled)
    {
        unselect(old);
        select(m_selectedIndex);
    }
}

//private
glm::vec2 UISystem::toWorldCoords(std::int32_t x, std::int32_t y)
{
    auto vpX = static_cast<float>(x) / m_windowSize.x;
    auto vpY = static_cast<float>(y) / m_windowSize.y;

    return toWorldCoords(vpX, vpY);
}

glm::vec2 UISystem::toWorldCoords(float x, float y)
{
    //invert Y
    y = 1.f - y;

    //scale to vp
    auto vp = getScene()->getActiveCamera().getComponent<Camera>().viewport;
    x -= vp.left;
    x /= vp.width;

    y -= vp.bottom;
    y /= vp.height;

    //convert to NDC
    x *= 2.f; x -= 1.f;
    y *= 2.f; y -= 1.f;

    //and unproject
    auto worldPos = glm::inverse(getScene()->getActiveCamera().getComponent<Camera>().getActivePass().viewProjectionMatrix) * glm::vec4(x, y, 0.f, 1.f);
    return { worldPos };
}

void UISystem::selectNext(std::size_t stride, std::int32_t direction)
{
    const auto& entities = m_groups[m_activeGroup];
    auto old = m_selectedIndex;

    auto targetSelection = entities[m_selectedIndex].getComponent<UIInput>().m_neighbourIndices[direction];
    if (targetSelection != std::numeric_limits<std::size_t>::max())
    {
        //check if any of the entities in this groupd have the requested index
        auto result = std::find_if(entities.cbegin(), entities.cend(),
            [targetSelection](const Entity& e)
            {
                return e.getComponent<UIInput>().getSelectionIndex() == targetSelection;
            });

        if (result != entities.cend()
            /*&& result->getComponent<cro::UIInput>().enabled*/)
        {
            m_selectedIndex = std::distance(entities.cbegin(), result);
        }
    }


    //if it's the same we didn't find anything
    //or some bright spark tried to set the target component as itself...
    if (old == m_selectedIndex)
    {
        do
        {
            m_selectedIndex = (m_selectedIndex + stride) % entities.size();
        } while (m_selectedIndex != old && !entities[m_selectedIndex].getComponent<UIInput>().enabled);
    }

    //and do selected callback
    if (m_selectedIndex != old)
    {
        unselect(old);
        select(m_selectedIndex);
    }
}

void UISystem::selectPrev(std::size_t stride, std::int32_t direction)
{
    const auto& entities = m_groups[m_activeGroup];

    auto old = m_selectedIndex;

    auto targetSelection = entities[m_selectedIndex].getComponent<UIInput>().m_neighbourIndices[direction];
    if (targetSelection != std::numeric_limits<std::size_t>::max())
    {
        //check if any of the entities in this groupd have the requested index
        auto result = std::find_if(entities.cbegin(), entities.cend(),
            [targetSelection](const Entity& e)
            {
                return e.getComponent<UIInput>().getSelectionIndex() == targetSelection;
            });

        if (result != entities.cend()
            /*&& result->getComponent<cro::UIInput>().enabled*/)
        {
            m_selectedIndex = std::distance(entities.cbegin(), result);
        }
    }

    //if it's the same we didn't find anything
    //or some bright spark tried to set the target component as itself...
    if (old == m_selectedIndex)
    {
        //get new index
        do
        {
            m_selectedIndex = (m_selectedIndex + (entities.size() - stride)) % entities.size();
        } while (m_selectedIndex != old && !entities[m_selectedIndex].getComponent<UIInput>().enabled);
    }

    //and do selected callback
    if (m_selectedIndex != old)
    {
        unselect(old);
        select(m_selectedIndex);
    }
}

void UISystem::unselect(std::size_t entIdx)
{
    auto& entities = m_groups[m_activeGroup];
    if (entIdx < entities.size())
    {
        auto idx = entities[entIdx].getComponent<UIInput>().callbacks[UIInput::Unselected];
        m_selectionCallbacks[idx](entities[entIdx]);
    }
}

void UISystem::select(std::size_t entIdx)
{
    auto& entities = m_groups[m_activeGroup];
    auto idx = entities[entIdx].getComponent<UIInput>().callbacks[UIInput::Selected];
    m_selectionCallbacks[idx](entities[entIdx]);
}

void UISystem::updateGroupAssignments()
{
    auto& entities = getEntities();
    for (auto& e : entities)
    {
        //check to see if the hitbox group changed - remove from old first before adding to new
        auto& input = e.getComponent<UIInput>();

        if (input.m_updateGroup)
        {
            //only swap group if we changed - we may have only changed index order
            if (input.m_previousGroup != input.m_group)
            {
                //if we're selected unselect first
                if (m_selectedIndex >= m_groups[m_activeGroup].size())
                {
                    m_selectedIndex = 0;
                }

                //remove from old group first
                m_groups[input.m_previousGroup].erase(std::remove_if(m_groups[input.m_previousGroup].begin(),
                    m_groups[input.m_previousGroup].end(),
                    [e](Entity entity)
                    {
                        return e == entity;
                    }), m_groups[input.m_previousGroup].end());

                //create new group if needed
                if (m_groups.count(input.m_group) == 0)
                {
                    m_groups.insert(std::make_pair(input.m_group, std::vector<Entity>()));
                }

                //add to group
                if (input.m_selectionIndex == 0)
                {
                    //set a default order
                    input.m_selectionIndex = m_groups[input.m_group].size();
                }
                m_groups[input.m_group].push_back(e);
            }


            //sort the group by selection index
            std::sort(m_groups[input.m_group].begin(), m_groups[input.m_group].end(),
                [](Entity a, Entity b)
                {
                    return a.getComponent<UIInput>().m_selectionIndex < b.getComponent<UIInput>().m_selectionIndex;
                });


            input.m_updateGroup = false;
        }
    }
}

void UISystem::onEntityAdded(Entity entity)
{
    //add to group 0 by default, process() will move the entity if needed
    m_groups[0].push_back(entity);

    //add a default sort order to items without a specific
    //position if they're left in the main group.
    auto& input = entity.getComponent<UIInput>();
    if (input.m_group == 0
        && input.m_selectionIndex == 0)
    {
        input.m_selectionIndex = m_groups[0].size();
    }


    //add a transform callback to only update world TX if input moves/scales
    auto* worldArea = &input.m_worldArea;
    *worldArea = input.area.transform(entity.getComponent<Transform>().getWorldTransform()); //remember to set initial value!
    entity.getComponent<Transform>().addCallback(
        [entity, worldArea]() mutable
        {
            *worldArea = entity.getComponent<UIInput>().area.transform(entity.getComponent<Transform>().getWorldTransform());
        });
}

void UISystem::onEntityRemoved(Entity entity)
{
    //remove the entity from its group
    auto group = entity.getComponent<UIInput>().m_group;
    
    //remove from group
    m_groups[group].erase(std::remove_if(m_groups[group].begin(), m_groups[group].end(),
        [entity](Entity e)
        {
            return e == entity;
        }), m_groups[group].end());


    //update selected index if this group is active
    if (m_activeGroup == group)
    {
        if (!m_groups[group].empty())
        {
            if (m_groups[group][m_selectedIndex] == entity)
            {
                //we don't need to call the unselect callback so set the index directly
                m_selectedIndex = std::min(m_selectedIndex - 1, m_groups[group].size() - 1);

                LogI << "Updated selected index to " << m_selectedIndex << std::endl;
            }
            else
            {
                //clamp to new size
                m_selectedIndex = std::min(m_selectedIndex, m_groups[group].size() - 1);
            }
        }
        else
        {
            //TODO reasonably pick the next active group
            //or, if all groups are now empty... IDK
        }
    }
}