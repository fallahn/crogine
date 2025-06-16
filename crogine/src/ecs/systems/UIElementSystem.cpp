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
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

using namespace cro;

namespace
{
    auto roundPos(glm::vec2 p)
    {
        p.x = std::round(p.x);
        p.y = std::round(p.y);
        return p;
    }
}

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

float UIElementSystem::getViewScale(glm::vec2 size)
{
    const float ratio = size.x / size.y;

    if (ratio < 1.7f)
    {
        //4:3
        return std::min(8.f, std::floor(size.x / 512.f));
    }

    if (ratio < 2.37f)
    {
        //widescreen - clamp at 6x for 4k
        return std::min(6.f, std::floor(size.x / 540.f));
    }

    //ultrawide
    return std::min(6.f, std::floor(size.y / 360.f));
}

//private
void UIElementSystem::onEntityAdded(Entity entity)
{
    //hmm this is a bit of a supposition, but we want to be consistent
    auto& element = entity.getComponent<UIElement>();
    element.absolutePosition = roundPos(element.absolutePosition);

    updateEntity(entity, cro::App::getWindow().getSize());
}

void UIElementSystem::updateEntity(Entity entity, glm::vec2 size)
{
    const auto& element = entity.getComponent<UIElement>();
    if (element.resizeCallback)
    {
        element.resizeCallback(entity);
    }

    
    switch (element.type)
    {
    default:
    {
        auto finalPos = (element.relativePosition * size) + element.absolutePosition;
        entity.getComponent<cro::Transform>().setPosition(glm::vec3(roundPos(finalPos), element.depth));
    }
        break;
    case UIElement::Position:
    {
        auto finalPos = (element.relativePosition * size);
        if (element.screenScale)
        {
            const auto screenScale = getViewScale(size);
            finalPos += (screenScale * element.absolutePosition);
        }
        else
        {
            finalPos += element.absolutePosition;
        }

        entity.getComponent<cro::Transform>().setPosition(glm::vec3(roundPos(finalPos), element.depth));
    }
        break;
    case UIElement::Sprite:
    {
        if (element.screenScale)
        {
            const auto screenScale = getViewScale(size);
            entity.getComponent<cro::Transform>().setPosition(glm::vec3(roundPos(element.absolutePosition * screenScale), element.depth));
            entity.getComponent<cro::Transform>().setScale(glm::vec2(screenScale));
        }
        else
        {
            entity.getComponent<cro::Transform>().setPosition(glm::vec3(element.absolutePosition, element.depth));
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
    }
        break;
    case UIElement::Text:
    {
        CRO_ASSERT(element.characterSize != 0, "");
        if (element.screenScale)
        {
            const auto screenScale = getViewScale(size);
            entity.getComponent<cro::Transform>().setPosition(glm::vec3(roundPos(element.absolutePosition * screenScale), element.depth));
            entity.getComponent<cro::Text>().setCharacterSize(element.characterSize * screenScale);
            entity.getComponent<cro::Text>().setVerticalSpacing(element.verticalSpacing * screenScale);
        }
        else
        {
            entity.getComponent<cro::Transform>().setPosition(glm::vec3(element.absolutePosition, element.depth));
            entity.getComponent<cro::Text>().setCharacterSize(element.characterSize);
            entity.getComponent<cro::Text>().setVerticalSpacing(element.verticalSpacing);
        }
    }
        break;
    }
}