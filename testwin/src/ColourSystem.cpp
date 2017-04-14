/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "ColourSystem.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/util/Maths.hpp>

#include <array>

using namespace cro;

ColourSystem::ColourSystem()
    : cro::System(this)
{
    requireComponent<cro::Sprite>();
    requireComponent<ColourChanger>();
}

void ColourSystem::process(cro::Time dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& sprite = entity.getComponent<Sprite>();
        auto& changer = entity.getComponent<ColourChanger>();

        auto time = dt.asSeconds() / 3.f;
        if (changer.up)
        {
            changer.colour[changer.currentChannel] = Util::Maths::clamp(changer.colour[changer.currentChannel] + time, 0.f, 1.f);
            if (changer.colour[changer.currentChannel] == 1)
            {
                changer.currentChannel = (changer.currentChannel + 1) % 3;
                if (changer.currentChannel == 0) changer.up = false;
            }
        }
        else
        {
            changer.colour[changer.currentChannel] = Util::Maths::clamp(changer.colour[changer.currentChannel] - time, 0.f, 1.f);
            if (changer.colour[changer.currentChannel] < 0.1f)
            {
                changer.currentChannel = (changer.currentChannel + 1) % 3;
                if (changer.currentChannel == 0) changer.up = true;
            }
        }

        sprite.setColour(changer.colour);
    }
}

//private
void ColourSystem::onEntityAdded(Entity entity)
{
    auto colour = entity.getComponent<Sprite>().getColour();
    entity.getComponent<ColourChanger>().colour = { colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha() };
}