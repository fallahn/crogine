/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/util/Random.hpp>

#include "../../detail/GLCheck.hpp"

using namespace cro;

namespace
{
    constexpr std::size_t MaxVertData = ParticleEmitter::MaxParticles * (3 + 4 + 3); //pos, colour, rotation vert attribs
    const std::size_t MaxParticleSystems = 30; //max number of VBOs
    const std::size_t MinParticleSystems = 4; //min amount before resizing - this many added on resize (so don't make too large!!)
}

ParticleSystem::ParticleSystem(MessageBus& mb)
    : System        (mb, typeid(ParticleSystem)),
    m_dataBuffer    (MaxVertData),
    m_vboIDs        (MaxParticleSystems),
    m_nextBuffer    (0),
    m_bufferCount   (0)
{
    for (auto& vbo : m_vboIDs) vbo = 0;

    for (auto i = 0; i < MinParticleSystems; ++i)
    {
        allocateBuffer();
    }

    requireComponent<Transform>();
    requireComponent<ParticleEmitter>();
}

ParticleSystem::~ParticleSystem()
{
    //delete VBOs
    for (auto vbo : m_vboIDs)
    {
        if (vbo)
        {
            glCheck(glDeleteBuffers(1, &vbo));
        }
    }
}

//public
void ParticleSystem::process(Time dt)
{
    auto& entities = getEntities();
    for (auto& e : entities)
    {
        //check each emitter to see if it should spawn a new particle
        auto& emitter = e.getComponent<ParticleEmitter>();
        if (emitter.m_running &&
            emitter.m_emissionClock.elapsed().asSeconds() > emitter.m_emitterSettings.emitRate)
        {
            if (emitter.m_nextFreeParticle < emitter.m_particles.size())
            {
                const auto& settings = emitter.m_emitterSettings;
                auto& p = emitter.m_particles[emitter.m_nextFreeParticle];
                p.colour = settings.colour;
                p.forces = settings.forces;
                p.gravity = settings.gravity;
                p.lifetime = p.maxLifeTime = settings.lifetime;
                //p.rotation; //TODO random initial rotation
                p.velocity = settings.initialVelocity;
                
                //spawn particle in world position
                auto& tx = e.getComponent<Transform>();
                p.position = tx.getWorldPosition();

                //add random radius placement - TODO how to do with a position table?
                p.position.x += Util::Random::value(-settings.spawnRadius.x, settings.spawnRadius.x);
                p.position.y += Util::Random::value(-settings.spawnRadius.y, settings.spawnRadius.y);
                p.position.z += Util::Random::value(-settings.spawnRadius.z, settings.spawnRadius.z);

                emitter.m_nextFreeParticle++;
            }
        }      

        //update each particle
        const float dtSec = dt.asSeconds();
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            auto& p = emitter.m_particles[i];

            p.velocity += p.gravity * dtSec;
            for (auto f : p.forces) p.velocity += f * dtSec;
            p.position += p.velocity * dtSec;            
           
            p.lifetime -= dtSec;
            p.colour.setAlpha(std::max(p.lifetime / p.maxLifeTime, 0.f));

            //TODO rotation
        }

        //go over again and remove dead particles with pop/swap
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            if (emitter.m_particles[i].lifetime < 0)
            {
                std::swap(emitter.m_particles[i], emitter.m_particles[emitter.m_nextFreeParticle]);
                emitter.m_nextFreeParticle--;
            }
        }

        //update VBOs allocating new if emitter has 0 VBO
    }
}

void ParticleSystem::render(Entity camera)
{
    //particles are already in world space so just need viewProj
}

//private
void ParticleSystem::onEntityAdded(Entity)
{
    //TODO check VBO count and increase if needed
}

void ParticleSystem::onEntityRemoved(Entity)
{
    //TODO update available VBOs
}

void ParticleSystem::allocateBuffer()
{
    CRO_ASSERT(m_bufferCount < m_vboIDs.size(), "Max Buffers Reached!");
    glCheck(glGenBuffers(1, &m_vboIDs[m_bufferCount]));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[m_bufferCount]));
    glCheck(glBufferData(GL_ARRAY_BUFFER, MaxVertData, nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    m_bufferCount++;
}