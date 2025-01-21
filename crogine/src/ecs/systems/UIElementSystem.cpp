/*-----------------------------------------------------------------------

Matt Marchant 2025
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
#include <crogine/core/Message.hpp>

#include <crogine/ecs/systems/UIElementSystem.hpp>

#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/components/Transform.hpp>

using namespace cro;

UIElementSystem::UIElementSystem(MessageBus& mb)
    : System(mb, typeid(UIElementSystem))
{
    requireComponent<UIElement>();
    requireComponent<Transform>();
}

//public
void UIElementSystem::handleMessage(const Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            const auto size = glm::vec2(data.data0, data.data1);
            auto& entities = getEntities();

            for (auto entity : entities)
            {
                updateEntity(entity, size);
            }
        }
    }
}


//private
void UIElementSystem::onEntityAdded(Entity entity)
{
    updateEntity(entity, cro::App::getWindow().getSize());
}

void UIElementSystem::updateEntity(Entity entity, glm::vec2 size)
{
    auto& element = entity.getComponent<UIElement>();
    if (element.resizeCallback)
    {
        element.resizeCallback(entity);
    }
    
    auto finalPos = (element.relativePosition * size) + element.absolutePosition;
    finalPos.x = std::round(finalPos.x);
    finalPos.y = std::round(finalPos.y);
    entity.getComponent<cro::Transform>().setPosition(glm::vec3(finalPos, element.depth));
}