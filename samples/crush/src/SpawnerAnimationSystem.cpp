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

#include "SpawnerAnimationSystem.hpp"
#include "SpawnAreaSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/util/Wavetable.hpp>

namespace
{
    constexpr float SpinSpeed = 100.f;
    const cro::Colour FieldColour = cro::Colour(0.380531f, 0.f, 1.f, 0.141176f);
}

void SpawnerAnimation::start()
{
    tableIndex = 0;
    active = true;
    spinnerEnt.getComponent<cro::ParticleEmitter>().start();
}

SpawnerAnimationSystem::SpawnerAnimationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(SpawnerAnimationSystem))
{
    requireComponent<SpawnerAnimation>();
    requireComponent<cro::Transform>();

    //should last the same duration as the spawn shield
    m_wavetable = cro::Util::Wavetable::sine(1.f / (SpawnArea::ActiveTime * 2.f));
    std::vector<float> temp(m_wavetable.begin(), m_wavetable.begin() + (m_wavetable.size() / 2));

    m_wavetable.swap(temp);
}

//public
void SpawnerAnimationSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& anim = entity.getComponent<SpawnerAnimation>();
        if (anim.active)
        {
            const float tableValue = std::min(1.f, std::max(0.f, m_wavetable[anim.activeIndex]));

            auto spinSpeed = SpinSpeed * tableValue;
            anim.spinnerEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, spinSpeed * dt);
            anim.spinnerEnt.getComponent<cro::ParticleEmitter>().settings.colour.setAlpha(tableValue);

            auto colour = FieldColour;
            colour.setAlpha(FieldColour.getAlpha() * tableValue);
            entity.getComponent<cro::Model>().setMaterialProperty(1, "u_colour", colour);

            anim.tableIndex = (anim.tableIndex + 1) % m_wavetable.size();
            if (anim.tableIndex == 0)
            {
                anim.activeIndex = 0;
                anim.active = false;
                anim.spinnerEnt.getComponent<cro::ParticleEmitter>().stop();
                entity.getComponent<cro::Model>().setMaterialProperty(1, "u_colour", cro::Colour::Transparent);
            }
            else if (anim.tableIndex > anim.activeIndex)
            {
                anim.activeIndex = anim.tableIndex;
            }
        }
    }
}

//private
void SpawnerAnimationSystem::onEntityAdded(cro::Entity entity)
{
    CRO_ASSERT(entity.getComponent<SpawnerAnimation>().spinnerEnt.isValid(), "missing entity");
}