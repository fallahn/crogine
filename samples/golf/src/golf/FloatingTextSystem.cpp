/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "FloatingTextSystem.hpp"
#include "MenuConsts.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/util/Easings.hpp>

FloatingTextSystem::FloatingTextSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(FloatingTextSystem))
{
    requireComponent<FloatingText>();
    requireComponent<cro::Transform>();
    requireComponent<cro::Text>();
}

//public
void FloatingTextSystem::process(float dt)
{
    const auto& entities = getEntities();
    for (auto e : entities)
    {
        auto& floatingText = e.getComponent<FloatingText>();

        floatingText.currTime = std::min(FloatingText::MaxTime, floatingText.currTime + dt);

        float progress = floatingText.currTime / FloatingText::MaxTime;
        float move = std::floor(cro::Util::Easing::easeOutQuint(progress) * FloatingText::MaxMove);

        auto pos = floatingText.basePos;
        pos.y += move;
        e.getComponent<cro::Transform>().setPosition(pos);

        if (floatingText.currTime == FloatingText::MaxTime)
        {
            getScene()->destroyEntity(e);
        }

        float alpha = cro::Util::Easing::easeInCubic(progress);
        auto c = floatingText.colour;
        c.setAlpha(1.f - alpha);
        e.getComponent<cro::Text>().setFillColour(c);

        break; //only update the front entity
    }
}

//public
void FloatingTextSystem::onEntityAdded(cro::Entity e)
{
    e.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
}