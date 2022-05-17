/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "VatAnimationSystem.hpp"

#include <crogine/ecs/components/Model.hpp>

VatAnimationSystem::VatAnimationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(VatAnimationSystem))
{
    requireComponent<VatAnimation>();
    requireComponent<cro::Model>();
}

//public
void VatAnimationSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& anim = entity.getComponent<VatAnimation>();
        anim.currentTime += anim.deltaTime;

        if (anim.currentTime > 1.f)
        {
            anim.currentTime -= 1.f;
        }

        //would prefer not to do the string lookup, but perf hit is negligable...
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_time", anim.currentTime);
        entity.getComponent<cro::Model>().setShadowMaterialProperty(0, "u_time", anim.currentTime);
    }
}