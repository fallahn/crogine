/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "TrophyDisplaySystem.hpp"

#include <crogine/ecs/Scene.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Wavetable.hpp>

TrophyDisplaySystem::TrophyDisplaySystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(TrophyDisplaySystem))
{
    requireComponent<TrophyDisplay>();
    requireComponent<cro::Transform>();
    requireComponent<cro::ParticleEmitter>();
    requireComponent<cro::AudioEmitter>();

    auto wave = cro::Util::Wavetable::sine(0.5f, 0.25f);
    m_wavetable.insert(m_wavetable.end(), wave.begin(), wave.begin() + (wave.size() / 2));
}

//public
void TrophyDisplaySystem::process(float dt)
{
    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& trophy = entity.getComponent<TrophyDisplay>();
        auto& tx = entity.getComponent<cro::Transform>();
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
                trophy.label.getComponent<cro::Callback>().active = true;

                float scale = cro::Util::Easing::easeOutBack(trophy.scale);
                tx.setScale(glm::vec3(scale));

                if (trophy.scale == 1)
                {
                    trophy.state = TrophyDisplay::Active;
                    trophy.particleTime = TrophyDisplay::ParticleInterval + cro::Util::Random::value(-0.4f, 0.4f);
                    entity.getComponent<cro::ParticleEmitter>().start();
                    entity.getComponent<cro::AudioEmitter>().play();

                    //have to make sure the camera is active to add the system
                    //to its draw list...
                    getScene()->getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            }
            [[fallthrough]];
        case TrophyDisplay::Active:
            if (trophy.delay < 0)
            {
                trophy.tableIndex = std::min(m_wavetable.size() - 1, trophy.tableIndex + 1);
                {
                    auto position = tx.getPosition();
                    position.y = trophy.startHeight + m_wavetable[trophy.tableIndex];
                    tx.setPosition(position);
                }

                trophy.particleTime -= dt;
                if (trophy.particleCount && trophy.particleTime < 0.f)
                {
                    trophy.particleCount--;
                    trophy.particleTime += TrophyDisplay::ParticleInterval + cro::Util::Random::value(-0.4f, 0.4f);

                    entity.getComponent<cro::ParticleEmitter>().start();
                    entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
                    entity.getComponent<cro::AudioEmitter>().play();
                }
            }
            tx.rotate(cro::Transform::Y_AXIS, dt);
            break;
        }
    }
}

void TrophyDisplaySystem::onEntityAdded(cro::Entity entity)
{
    entity.getComponent<TrophyDisplay>().startHeight = entity.getComponent<cro::Transform>().getPosition().y;
}