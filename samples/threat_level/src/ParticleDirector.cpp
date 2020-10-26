/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "ParticleDirector.hpp"
#include "Messages.hpp"
#include "PlayerSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/Scene.hpp>

#include <crogine/detail/Types.hpp>
#include <crogine/graphics/TextureResource.hpp>

#include <cstddef>

namespace
{
    const std::size_t MinEmitters = 4;
    const std::size_t MaxEmitters = 48;
}

ParticleDirector::ParticleDirector(cro::TextureResource& tr)
    : m_nextFreeEmitter (0)
{
    //load particle presets
    m_settings[SettingsID::Explosion].loadFromFile("assets/particles/explosion.cps", tr);
}

//public
void ParticleDirector::handleMessage(const cro::Message& msg)
{
    if (m_nextFreeEmitter < MaxEmitters)
    {
        //fire a particle system based on event
        if (msg.id == MessageID::NpcMessage)
        {
            const auto& data = msg.getData<NpcEvent>();
            if (data.type == NpcEvent::Died
                /*|| data.type == NpcEvent::HealthChanged*/)
            {
                //explosion
                auto particleEnt = getNextEntity();
                auto& emitter = particleEnt.getComponent<cro::ParticleEmitter>();
                emitter.settings = m_settings[SettingsID::Explosion];
                emitter.start();
                particleEnt.getComponent<cro::Transform>().setPosition(data.position);
            }
        }
        else if (msg.id == MessageID::PlayerMessage)
        {
            const auto& data = msg.getData<PlayerEvent>();
            if (data.type == PlayerEvent::Died)
            {
                auto particleEnt = getNextEntity();
                auto& emitter = particleEnt.getComponent<cro::ParticleEmitter>();
                emitter.settings = m_settings[SettingsID::Explosion];
                emitter.start();
                particleEnt.getComponent<cro::Transform>().setPosition(data.position);
            }
        }
    }
}

void ParticleDirector::handleEvent(const cro::Event&)
{

}

void ParticleDirector::process(float)
{
    //check for finished systems then free up by swapping
    for (auto i = 0u; i < m_nextFreeEmitter; ++i)
    {
        if (m_emitters[i].getComponent<cro::ParticleEmitter>().stopped())
        {
            auto entity = m_emitters[i];
            m_nextFreeEmitter--;
            m_emitters[i] = m_emitters[m_nextFreeEmitter];
            m_emitters[m_nextFreeEmitter] = entity;
            i--;
        }
    }
}

//private
void ParticleDirector::resizeEmitters()
{
    m_emitters.resize(m_emitters.size() + MinEmitters);
    for (auto i = m_emitters.size() - MinEmitters; i < m_emitters.size(); ++i)
    {
        m_emitters[i] = getScene().createEntity();
        m_emitters[i].addComponent<cro::ParticleEmitter>();
        m_emitters[i].addComponent<cro::Transform>();
    }
}

cro::Entity ParticleDirector::getNextEntity()
{
    if (m_nextFreeEmitter == m_emitters.size())
    {
        resizeEmitters();
    }
    auto ent = m_emitters[m_nextFreeEmitter++];
    return ent;
}