/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "ParticleDirector.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/Scene.hpp>

#include <cstddef>

namespace
{
    const std::size_t MinEmitters = 8;

    //this is a fallback in case a loaded particle
    //settings file either doesn't stop or fails to
    //load. I know this might not always be the case
    //but in the particular instance of golf we don't
    //want any never-ending particle streams
    struct TimeoutCallback final
    {
        void operator()(cro::Entity e, float dt)
        {
            timeout -= dt;
            if (timeout < 0)
            {
                e.getComponent<cro::ParticleEmitter>().stop();
                e.getComponent<cro::Callback>().active = false;
            }
        }
        float timeout = 3.f;
    };
}

ParticleDirector::ParticleDirector()
    : m_nextFreeEmitter   (0)
{

}

//public
void ParticleDirector::process(float)
{
    //check for finished systems then free up by swapping
    for (auto i = 0u; i < m_nextFreeEmitter; ++i)
    {
        if (m_emitters[i].getComponent<cro::ParticleEmitter>().stopped())
        {
            auto entity = m_emitters[i];
            entity.getComponent<cro::Callback>().active = false;

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
        m_emitters[i].addComponent<cro::Callback>();
    }
}

cro::Entity ParticleDirector::getNextEntity()
{
    if (m_nextFreeEmitter == m_emitters.size())
    {
        resizeEmitters();
    }
    auto ent = m_emitters[m_nextFreeEmitter++];
    ent.getComponent<cro::Callback>().function = TimeoutCallback();
    ent.getComponent<cro::Callback>().active = true;
    return ent;
}
