/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "AvatarScaleSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>

#include <crogine/util/Easings.hpp>

AvatarScaleSystem::AvatarScaleSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(AvatarScaleSystem))
{
    requireComponent<AvatarScale>();
    requireComponent<cro::Transform>();
}

//public
void AvatarScaleSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& as = entity.getComponent<AvatarScale>();
        if (as.active)
        {
            if (as.current < as.target)
            {
                as.current = (std::min(as.target, as.current + (dt * as.speed)));
            }
            else
            {
                as.current = std::max(as.target, as.current - (dt * as.speed));
            }

            float scale = cro::Util::Easing::easeInBounce(as.current / as.target);
            entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

            if (as.current == as.target)
            {
                as.active = false;
            }
        }
    }
}