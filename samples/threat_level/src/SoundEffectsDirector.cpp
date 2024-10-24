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

#include "SoundEffectsDirector.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Scene.hpp>

#include <crogine/util/Random.hpp>

#include <array>

namespace
{
    //IDs used to index paths below
    enum AudioID
    {
        NPCLaser, Explode,
        EMP, Buddy,

        Count
    };

    //paths to audio files - non streaming only
    //must match with enum above
    const std::array<std::string, AudioID::Count> paths = 
    {
        "assets/audio/effects/laser.wav",
        "assets/audio/effects/explode.wav",   
        "assets/audio/effects/emp.wav",   
        "assets/audio/effects/buddy_arrive.wav"   
    };

    const std::size_t MinEntities = 32;
    const std::size_t MaxEntities = 128;
}

SFXDirector::SFXDirector()
    : m_nextFreeEntity    (0)
{
    //pre-load sounds
    for(auto i = 0; i < AudioID::Count; ++i)
    {
        m_resources.load(i, paths[i]);
    }
}

//public
void SFXDirector::handleMessage(const cro::Message& msg)
{
    if (m_nextFreeEntity < MaxEntities)
    {
        if (msg.id == MessageID::NpcMessage)
        {
            const auto& data = msg.getData<NpcEvent>();
            if (data.type == NpcEvent::Died
                || data.type == NpcEvent::HealthChanged)
            {
                //explosion
                playSound(AudioID::Explode, data.position).setPitch(cro::Util::Random::value(0.9f, 1.1f));
            }
            else if (data.type == NpcEvent::FiredWeapon)
            {
                //pew pew
                playSound(AudioID::NPCLaser, data.position).setPitch(cro::Util::Random::value(0.8f, 1.2f));
            }
        }
        else if (msg.id == MessageID::PlayerMessage)
        {
            const auto& data = msg.getData<PlayerEvent>();
            if (data.type == PlayerEvent::Died)
            {
                playSound(AudioID::Explode, data.position).setPitch(cro::Util::Random::value(0.8f, 1.f));
            }
            else if (data.type == PlayerEvent::FiredLaser)
            {
                playSound(AudioID::NPCLaser, data.position).setPitch(cro::Util::Random::value(0.7f, 1.f));
            }
            else if (data.type == PlayerEvent::FiredEmp)
            {
                playSound(AudioID::EMP, data.position).setPitch(cro::Util::Random::value(0.7f, 1.f));
            }
        }
        else if (msg.id == MessageID::BuddyMessage)
        {
            const auto& data = msg.getData<BuddyEvent>();
            if (data.type == BuddyEvent::FiredWeapon)
            {
                playSound(AudioID::NPCLaser, data.position).setPitch(0.4f);
            }
            else if (data.type == BuddyEvent::Spawned)
            {
                playSound(AudioID::Buddy, data.position).setVolume(2.f);
            }
        }
    }
}

void SFXDirector::process(float dt)
{
    //check our ents and free up finished sounds
    for (auto i = 0u; i < m_nextFreeEntity; ++i)
    {
        if (m_entities[i].getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
        {
            auto entity = m_entities[i];
            m_nextFreeEntity--;
            m_entities[i] = m_entities[m_nextFreeEntity];
            m_entities[m_nextFreeEntity] = entity;
            i--;
        }
    }
}

//private
cro::Entity SFXDirector::getNextFreeEntity()
{
    if (m_nextFreeEntity == m_entities.size())
    {
        resizeEntities(m_entities.size() + MinEntities);
    }
    return m_entities[m_nextFreeEntity++];
}

void SFXDirector::resizeEntities(std::size_t size)
{
    auto currSize = m_entities.size();
    m_entities.resize(size);

    for (auto i = currSize; i < m_entities.size(); ++i)
    {
        m_entities[i] = getScene().createEntity();
        m_entities[i].addComponent<cro::Transform>();
        m_entities[i].addComponent<cro::AudioEmitter>();
    }
}

cro::AudioEmitter& SFXDirector::playSound(std::int32_t audioID, glm::vec3 position)
{
    auto entity = getNextFreeEntity();
    entity.getComponent<cro::Transform>().setPosition(position);
    auto& emitter = entity.getComponent<cro::AudioEmitter>();
    emitter.setSource(m_resources.get(audioID));
    //must reset values here in case they were changed prior to recycling from pool
    emitter.setRolloff(1.f);
    //emitter.setMinDistance(5.f);
    emitter.setVolume(1.f);
    emitter.setPitch(1.f);
    emitter.setMixerChannel(1);
    emitter.play();
    return emitter;
}