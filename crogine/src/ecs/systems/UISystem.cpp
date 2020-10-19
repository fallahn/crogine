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

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

using namespace cro;

UISystem::UISystem(MessageBus& mb)
    : System        (mb, typeid(UISystem)),
    m_selectedIndex (0),
    m_groups        (1),
    m_activeGroup   (0)
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
            auto& buttonEvent = m_downEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.button = evt.button;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        m_eventPosition = toWorldCoords(evt.button.x, evt.button.y);
        {
            auto& buttonEvent = m_upEvents.emplace_back();
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
            auto& buttonEvent = m_downEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.tfinger = evt.tfinger;
        }
        break;
    case SDL_FINGERUP:
        m_eventPosition = toWorldCoords(evt.tfinger.x, evt.tfinger.y);
        {
            auto& buttonEvent = m_upEvents.emplace_back();
            buttonEvent.type = evt.type;
            buttonEvent.tfinger = evt.tfinger;
        }
        break;
    case SDL_KEYDOWN:
    {
        auto& buttonEvent = m_downEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.key = evt.key;
    }
        break;
    case SDL_KEYUP:
    {
        auto& buttonEvent = m_upEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.key = evt.key;
    }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    {
        auto& buttonEvent = m_downEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.cbutton = evt.cbutton;
    }
        break;
    case SDL_CONTROLLERBUTTONUP:
    {
        auto& buttonEvent = m_upEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.cbutton = evt.cbutton;
    }
        break;
    case SDL_JOYBUTTONDOWN:
    {
        auto& buttonEvent = m_downEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.jbutton = evt.jbutton;
    }
        break;
    case SDL_JOYBUTTONUP:
    {
        auto& buttonEvent = m_upEvents.emplace_back();
        buttonEvent.type = evt.type;
        buttonEvent.jbutton = evt.jbutton;
    }
        break;
    }
}

void UISystem::process(float)
{    
    updateGroupAssignments();

    std::size_t currentIndex = 0;
    for (auto& e : m_groups[m_activeGroup])
    {
        //TODO probably want to cache these and only update if control moved
        auto tx = e.getComponent<Transform>().getWorldTransform();
        auto& input = e.getComponent<UIInput>();

        auto area = input.area.transform(tx);
        bool contains = false;
        if (contains = area.contains(m_eventPosition); contains)
        {
            if (!input.active)
            {
                //mouse has entered
                input.active = true;
                MotionEvent m;
                m.type = MotionEvent::CursorEnter;
                m_movementCallbacks[input.callbacks[UIInput::Enter]](e, m_movementDelta, m);
            }

            unselect(m_selectedIndex);
            m_selectedIndex = currentIndex;
            select(m_selectedIndex);

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

        if (contains || currentIndex == m_selectedIndex)
        {
            for (const auto& f : m_downEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonDown]](e, f);
            }
            for (const auto& f : m_upEvents)
            {
                m_buttonCallbacks[input.callbacks[UIInput::ButtonUp]](e, f);
            }
        }

        currentIndex++;
    }

    //DPRINT("Window Pos", std::to_string(m_eventPosition.x) + ", " + std::to_string(m_eventPosition.y));

    //TODO selecting previous and next input in active group with the keyboard or controller

    m_previousEventPosition = m_eventPosition;
    m_upEvents.clear();
    m_downEvents.clear();
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

uint32 UISystem::addCallback(const ButtonCallback& cb)
{
    m_buttonCallbacks.push_back(cb);
    return static_cast<uint32>(m_buttonCallbacks.size() - 1);
}

uint32 UISystem::addCallback(const MovementCallback& cb)
{
    m_movementCallbacks.push_back(cb);
    return static_cast<uint32>(m_movementCallbacks.size() - 1);
}

uint32 UISystem::addCallback(const SelectionChangedCallback& cb)
{
    m_selectionCallbacks.push_back(cb);
    return static_cast<uint32>(m_selectionCallbacks.size() - 1);
}

void UISystem::setActiveGroup(std::size_t group)
{
    updateGroupAssignments();

    CRO_ASSERT(m_groups.count(group) != 0, "Group doesn't exist");
    CRO_ASSERT(!m_groups[group].empty(), "Group is empty");

    //unselect(m_selectedIndex);

    m_activeGroup = group;
    //m_selectedIndex = 0;

    //select(m_selectedIndex);
}

//private
glm::vec2 UISystem::toWorldCoords(int32 x, int32 y)
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
    y -= vp.bottom;
    y /= vp.height;

    //convert to NDC
    x *= 2.f; x -= 1.f;
    y *= 2.f; y -= 1.f;

    //and unproject
    auto worldPos = glm::inverse(getScene()->getActiveCamera().getComponent<Camera>().viewProjectionMatrix) * glm::vec4(x, y, 0.f, 1.f);
    return { worldPos };
}

void UISystem::selectNext(std::size_t stride)
{
    //call unselected on prev ent
    auto& entities = m_groups[m_activeGroup];
    unselect(m_selectedIndex);

    //get new index
    m_selectedIndex = (m_selectedIndex + stride) % entities.size();

    //and do selected callback
    select(m_selectedIndex);
}

void UISystem::selectPrev(std::size_t stride)
{
    //call unselected on prev ent
    auto& entities = m_groups[m_activeGroup];
    unselect(m_selectedIndex);

    //get new index
    m_selectedIndex = (m_selectedIndex + (entities.size() - stride)) % entities.size();

    //and do selected callback
    select(m_selectedIndex);
}

void UISystem::unselect(std::size_t entIdx)
{
    auto& entities = m_groups[m_activeGroup];
    auto idx = entities[entIdx].getComponent<UIInput>().callbacks[UIInput::Unselected];
    m_selectionCallbacks[idx](entities[entIdx]);
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
}

void UISystem::onEntityRemoved(Entity entity)
{
    //remove the entity from its group
    auto group = entity.getComponent<UIInput>().m_group;

    if (m_activeGroup == group)
    {
        selectPrev(1);
    }

    m_groups[group].erase(std::remove_if(m_groups[group].begin(), m_groups[group].end(),
        [entity](Entity e)
        {
            return e == entity;
        }), m_groups[group].end());
}