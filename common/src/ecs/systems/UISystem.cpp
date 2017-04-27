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

using namespace cro;

UISystem::UISystem(MessageBus& mb)
    : System(mb, this)
{
    requireComponent<UIInput>();
    requireComponent<Transform>();

    m_callbacks.push_back([](Entity, MessageBus&) {}); //default callback for components which don't have one assigned
}

void UISystem::handleEvent(const Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        //TODO flag these so we can group as many events as possible in a single frame
        //that way looping over entities only needs be done once
        break;
    case SDL_MOUSEBUTTONDOWN:

        break;
    case SDL_MOUSEBUTTONUP:

        break;
    case SDL_FINGERMOTION:

        break;
    case SDL_FINGERDOWN:

        break;
    case SDL_FINGERUP:

        break;
    }
}

void UISystem::process(Time dt)
{
    //TODO we probably want some partitioning? Checking every entity for a collision could be a bit pants
}

uint32 UISystem::addCallback(const Callback& cb)
{
    m_callbacks.push_back(cb);
    return m_callbacks.size() - 1;
}