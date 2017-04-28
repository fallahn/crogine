/*-----------------------------------------------------------------------

Matt Marchant 2017
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
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include <glm/vec2.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace cro;

UISystem::UISystem(MessageBus& mb)
    : System    (mb, this)
{
    requireComponent<UIInput>();
    requireComponent<Transform>();

    m_callbacks.push_back([](Entity, uint64) {}); //default callback for components which don't have one assigned

    m_windowSize = App::getWindow().getSize();
    setViewPort(m_windowSize.x, m_windowSize.y);

    //setup projection
    m_projectionMatrix = glm::ortho(0.f, static_cast<float>(DefaultSceneSize.x), 0.f,
        static_cast<float>(DefaultSceneSize.y), -0.1f, 100.f);
}

void UISystem::handleEvent(const Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        m_eventPosition = toScreenCoords(evt.motion.x, evt.motion.y);
        break;
    case SDL_MOUSEBUTTONDOWN:
        m_eventPosition = toScreenCoords(evt.button.x, evt.button.y);
        m_previousEventPosition = m_eventPosition;
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_downEvents.push_back(LeftMouse);
            break;
        case SDL_BUTTON_RIGHT:
            m_downEvents.push_back(RightMouse);
            break;
        case SDL_BUTTON_MIDDLE:
            m_downEvents.push_back(MiddleMouse);
            break;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        m_eventPosition = toScreenCoords(evt.button.x, evt.button.y);
        switch (evt.button.button)
        {
        default: break;
        case SDL_BUTTON_LEFT:
            m_upEvents.push_back(Flags::LeftMouse);
            break;
        case SDL_BUTTON_RIGHT:
            m_upEvents.push_back(Flags::RightMouse);
            break;
        case SDL_BUTTON_MIDDLE:
            m_upEvents.push_back(Flags::MiddleMouse);
            break;
        }
        break;
    case SDL_FINGERMOTION:
        m_eventPosition = toScreenCoords(evt.tfinger.dx, evt.tfinger.dy);
        //TODO check finger IDs for gestures etc
        break;
    case SDL_FINGERDOWN:
        m_eventPosition = toScreenCoords(evt.tfinger.dx, evt.tfinger.dy);
        m_previousEventPosition = m_eventPosition;
        //TODO check finger IDs for gestures etc
        m_downEvents.push_back(Finger);
        break;
    case SDL_FINGERUP:
        m_eventPosition = toScreenCoords(evt.tfinger.dx, evt.tfinger.dy);
        m_upEvents.push_back(Finger);
        break;
    }
}

void UISystem::process(Time dt)
{    
    //TODO we probably want some partitioning? Checking every entity for a collision could be a bit pants
    auto& entities = getEntities();
    for (auto& e : entities)
    {
        //TODO probably want to cache these and only update if control moved
        auto tx = e.getComponent<Transform>().getWorldTransform();
        auto& input = e.getComponent<UIInput>();

        auto area = input.area.transform(tx);
        if (area.contains(m_eventPosition))
        {
            if (!input.active)
            {
                //mouse has entered
                input.active = true;
                m_callbacks[input.callbacks[UIInput::MouseEnter]](e, 0);
            }
            for (auto f : m_downEvents)
            {
                m_callbacks[input.callbacks[UIInput::MouseDown]](e, f);
            }
            for (auto f : m_upEvents)
            {
                m_callbacks[input.callbacks[UIInput::MouseUp]](e, f);
            }
        }
        else
        {
            if (input.active)
            {
                //mouse left
                input.active = false;
                m_callbacks[input.callbacks[UIInput::MouseExit]](e, 0);
            }
        }
    }

    //DPRINT("Window Pos", std::to_string(m_eventPosition.x) + ", " + std::to_string(m_eventPosition.y));

    m_previousEventPosition = m_eventPosition;
    m_upEvents.clear();
    m_downEvents.clear();
}

void UISystem::handleMessage(const Message& msg)
{
    if (msg.id == Message::WindowMessage)
    {
        const auto& data = msg.getData<Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            setViewPort(data.data0, data.data1);
            m_windowSize.x = data.data0;
            m_windowSize.y = data.data1;
        }
    }
}

uint32 UISystem::addCallback(const Callback& cb)
{
    m_callbacks.push_back(cb);
    return m_callbacks.size() - 1;
}

//private
void UISystem::setViewPort(int32 x, int32 y)
{
    //assumes width is always widest    
    m_viewPort.width = x;
    m_viewPort.height = (x / 16) * 9;
    m_viewPort.bottom = (y - m_viewPort.height) / 2;
}

glm::vec2 UISystem::toScreenCoords(int32 x, int32 y)
{
    y = m_windowSize.y - y; //invert vertically
    //convert to viewport space
    auto vpX = static_cast<float>(x - m_viewPort.left) / m_viewPort.width;
    auto vpY = static_cast<float>(y - m_viewPort.bottom) / m_viewPort.height;

    //convert to NDC
    vpX *= 2.f;
    vpX -= 1.f;

    vpY *= 2.f;
    vpY -= 1.f;

    //convert to world
    auto worldPos = glm::inverse(m_projectionMatrix) * glm::vec4(vpX, vpY, 0.f, 1.f);

    return { worldPos };
}

glm::vec2 UISystem::toScreenCoords(float x, float y)
{
    //touch input is alread NDC - TODO check Y inversion
    float ratio = static_cast<float>(m_windowSize.y) / m_viewPort.height;
    y *= ratio;

    auto screenPos = glm::inverse(m_projectionMatrix) * glm::vec4(x, y, 0.f, 1.f);
    return { screenPos };
}