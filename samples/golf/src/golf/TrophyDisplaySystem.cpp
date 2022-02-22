/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "TrophyDisplaySystem.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/util/Easings.hpp>

TrophyDisplaySystem::TrophyDisplaySystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(TrophyDisplaySystem))
{
    requireComponent<TrophyDisplay>();
    requireComponent<cro::Transform>();
    requireComponent<cro::ParticleEmitter>();
}

//public
void TrophyDisplaySystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& trophy = entity.getComponent<TrophyDisplay>();
        switch (trophy.state)
        {
        default:
        case TrophyDisplay::Idle:
            break;
        case TrophyDisplay::In:
            trophy.delay -= dt;
            if (trophy.delay < 0)
            {
                trophy.scale = std::min(1.f, trophy.scale + (dt * 4.f));

                float scale = cro::Util::Easing::easeOutBack(trophy.scale);
                entity.getComponent<cro::Transform>().setScale(glm::vec3(scale));

                if (trophy.scale == 1)
                {
                    trophy.state = TrophyDisplay::Active;
                    entity.getComponent<cro::ParticleEmitter>().start();

                    //have to make sure the camera is active to add the system
                    //to its draw list...
                    getScene()->getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            }
            [[fallthrough]];
        case TrophyDisplay::Active:
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
            break;
        }
    }
}