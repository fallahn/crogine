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

#include "NameScrollSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/util/Wavetable.hpp>

NameScrollSystem::NameScrollSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(NameScrollSystem))
{
    requireComponent<NameScroller>();
    requireComponent<cro::Transform>();
    requireComponent<cro::Drawable2D>();

    m_waveTable = cro::Util::Wavetable::sine(0.25f);

    for (auto& f : m_waveTable)
    {
        f += 1.f;
        f /= 2.f;
    }
}

//public
void NameScrollSystem::process(float)
{
    static constexpr std::int32_t UpdateRate = 4;
    static std::int32_t updateCounter = 0;

    updateCounter++;

    if (updateCounter % UpdateRate == 0)
    {
        auto entities = getEntities();
        for (auto entity : entities)
        {
            auto& scroller = entity.getComponent<NameScroller>();
            if (scroller.maxDistance > 0
                && scroller.active)
            {
                scroller.tableIndex = (scroller.tableIndex + 1) % m_waveTable.size();

                float distance = std::round(scroller.maxDistance * m_waveTable[scroller.tableIndex]);

                auto bounds = entity.getComponent<cro::Drawable2D>().getCroppingArea();
                bounds.left = distance;
                entity.getComponent<cro::Drawable2D>().setCroppingArea(bounds);

                auto pos = entity.getComponent<cro::Transform>().getPosition();
                pos.x = scroller.basePosition - distance;
                entity.getComponent<cro::Transform>().setPosition(pos);
            }
            else
            {
                scroller.tableIndex = 0;
            }
        }
    }
}

//private
void NameScrollSystem::onEntityAdded(cro::Entity e)
{
    e.getComponent<NameScroller>().basePosition = 
        e.getComponent<cro::Transform>().getPosition().x + (e.getComponent<NameScroller>().maxDistance / 2.f);
}