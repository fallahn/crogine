/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ScrubParticleDirector.hpp"
#include "ScrubConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>

ScrubParticleDirector::ScrubParticleDirector(cro::TextureResource& tr)
{
    m_emitterSettings[sc::ParticleID::Bubbles].loadFromFile("assets/arcade/scrub/particles/bubble.cps", tr);
    m_emitterSettings[sc::ParticleID::Splash].loadFromFile("assets/arcade/scrub/particles/splash.cps", tr);
}

//public
void ScrubParticleDirector::handleMessage(const cro::Message& msg)
{
    const auto getEnt = [&](std::int32_t id, glm::vec3 position)
        {
            auto entity = getNextEntity();
            entity.getComponent<cro::Transform>().setPosition(position);
            entity.getComponent<cro::ParticleEmitter>().settings = m_emitterSettings[id];
            entity.getComponent<cro::ParticleEmitter>().start();
            return entity;
        };

    if (msg.id == sc::MessageID::ParticleMessage)
    {
        const auto& data = msg.getData<sc::ParticleEvent>();
        switch (data.particleID)
        {
        default: break;
        case sc::ParticleID::Bubbles:
        {
            //check soap level and vary emission
            auto ent = getEnt(data.particleID, data.position);
            auto& emitter = ent.getComponent<cro::ParticleEmitter>();
            emitter.settings.size *= data.soapLevel;
            emitter.settings.emitCount = static_cast<std::uint32_t>(static_cast<float>(emitter.settings.emitCount) * data.soapLevel);
        }
            break;
        case sc::ParticleID::Splash:
            getEnt(data.particleID, data.position);
            break;
        }
    }
}