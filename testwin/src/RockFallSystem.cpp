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

#include "RockFallSystem.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

namespace
{
    const float gravity = 7.95f;
    const float spawnTime = 1.6f;
    const float shakeTime = 1.8f;
    const float newRockTime = spawnTime + shakeTime;

    std::array<float, 4u> textureOffsets = { 0.f, 0.25f, 0.5f, 0.75f };
}

RockFallSystem::RockFallSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(RockFallSystem)),
    m_running       (false),
    m_wavetable     (cro::Wavetable::Waveform::Sine, 10.f, 0.02f)
{
    requireComponent<cro::Transform>();
    requireComponent<RockFall>();
}

//public
void RockFallSystem::process(cro::Time dt)
{
    auto& entities = getEntities();
    const float dtSec = dt.asSeconds();
    if (m_running)
    {
        //check clock and spawn new if available
        if (m_clock.elapsed().asSeconds() > newRockTime)
        {
            for (auto& ent : entities)
            {
                auto& rockfall = ent.getComponent<RockFall>();
                if (rockfall.state == RockFall::Idle)
                {
                    m_clock.restart();
                    rockfall.state = RockFall::Spawning;
                    rockfall.stateTime = spawnTime;

                    auto& tx = ent.getComponent<cro::Transform>();
                    tx.setPosition({ cro::Util::Random::value(-3.f, 3.f), 2.9f, tx.getPosition().z });

                    auto& model = ent.getComponent<cro::Model>();
                    model.setMaterialProperty(0, "u_subrect", glm::vec4(textureOffsets[cro::Util::Random::value(0, 3)], 0.f, 0.25f, 1.f));

                    break;
                }
            }
        }
    }

    //update all active
    for (auto& ent : entities)
    {
        auto& rockfall = ent.getComponent<RockFall>();
        auto& tx = ent.getComponent<cro::Transform>();
        switch (rockfall.state)
        {
        default: break;

        case RockFall::Spawning:
            rockfall.stateTime -= dtSec;
            tx.move({ 0.f, -0.5f * dtSec, 0.f });
            if (rockfall.stateTime <= 0)
            {
                rockfall.stateTime = shakeTime + cro::Util::Random::value(-0.8f, 0.8f);
                rockfall.state = RockFall::Shaking;
            }
            break;
        case RockFall::Shaking:
            rockfall.stateTime -= dtSec;
            tx.move({ m_wavetable.fetch(dt), 0.f, 0.f });
            if (rockfall.stateTime <= 0)
            {
                rockfall.state = RockFall::Falling;
            }
            break;
        case RockFall::Falling:
            rockfall.velocity += gravity * dtSec;
            tx.move({ 0.f, -rockfall.velocity * dtSec, 0.f });

            if (tx.getPosition().y < -4.f)
            {
                rockfall.state = RockFall::Idle;
                rockfall.velocity = 0.f;
            }
            break;
        }
    }
}

void RockFallSystem::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::BackgroundSystem)
    {
        const auto& data = msg.getData<BackgroundEvent>();
        if (data.type == BackgroundEvent::ModeChanged)
        {
            setRunning((data.value != 0));
        }
    }
}

void RockFallSystem::setRunning(bool r)
{
    m_running = r;

    if (!r) //drop everything!
    {
        auto& entities = getEntities();
        for (auto& e : entities)
        {
            auto& rf = e.getComponent<RockFall>();
            if (rf.state != RockFall::Idle)
            {
                rf.state = RockFall::Falling;
            }
        }
    }
}