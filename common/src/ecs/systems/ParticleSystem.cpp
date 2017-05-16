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
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>

#include "../../detail/GLCheck.hpp"

#include <glm/gtc/type_ptr.hpp>

//why do I have to hack this? there has to be a catch...
#ifdef PLATFORM_DESKTOP
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 34370
#define GL_POINT_SPRITE 34913
#define ENABLE_POINT_SPRITES  glCheck(glEnable(GL_PROGRAM_POINT_SIZE)); glCheck(glEnable(GL_POINT_SPRITE))
#define DISABLE_POINT_SPRITES glCheck(glDisable(GL_PROGRAM_POINT_SIZE)); glCheck(glDisable(GL_POINT_SPRITE))
#endif //GL_PROGRAM_POINT_SIZE
#else
#define ENABLE_POINT_SPRITES
#define DISABLE_POINT_SPRITES
#endif // PLATFORM_DESKTOP

using namespace cro;

namespace
{
    const std::string vertex = R"(
        attribute vec4 a_position;
        attribute LOW vec4 a_colour;
        attribute MED vec3 a_normal; //this actually stores rotation and size

        uniform mat4 u_projection;
        uniform mat4 u_viewProjection;
        uniform LOW float u_viewportHeight;
        uniform LOW float u_particleSize;

        varying LOW vec4 v_colour;

        void main()
        {
            v_colour = a_colour;

            gl_Position = u_viewProjection * a_position;
            gl_PointSize = u_viewportHeight * u_projection[1][1] / gl_Position.w * u_particleSize;
        }
    )";

    const std::string fragment = R"(
        uniform sampler2D u_texture;

        varying LOW vec4 v_colour;

        void main()
        {
            gl_FragColor = v_colour * texture2D(u_texture, gl_PointCoord) * v_colour.a;
        }
    )";

    constexpr std::size_t MaxVertData = ParticleEmitter::MaxParticles * (3 + 4 + 3); //pos, colour, rotation/scale vert attribs
    const std::size_t MaxParticleSystems = 64; //max number of VBOs - must be divisible by min count
    const std::size_t MinParticleSystems = 4; //min amount before resizing - this many added on resize (so don't make too large!!)
    const std::size_t VertexSize = 10 * sizeof(float); //pos, colour, rotation/scale vert attribs
}

ParticleSystem::ParticleSystem(MessageBus& mb)
    : System            (mb, typeid(ParticleSystem)),
    m_dataBuffer        (MaxVertData),
    m_vboIDs            (MaxParticleSystems),
    m_nextBuffer        (0),
    m_bufferCount       (0),
    m_projectionUniform (-1),
    m_textureUniform    (-1),
    m_viewProjUniform   (-1),
    m_viewportUniform   (-1),
    m_sizeUniform       (-1)
{
    for (auto& vbo : m_vboIDs) vbo = 0;

    requireComponent<Transform>();
    requireComponent<ParticleEmitter>();

    if (!m_shader.loadFromString(vertex, fragment))
    {
        Logger::log("Failed to compile Particle shader", Logger::Type::Error);
    }
    else
    {
        //fetch uniforms.
        const auto& uniforms = m_shader.getUniformMap();
        m_projectionUniform = uniforms.find("u_projection")->second;
        m_textureUniform = uniforms.find("u_texture")->second;
        m_viewProjUniform = uniforms.find("u_viewProjection")->second;
        m_viewportUniform = uniforms.find("u_viewportHeight")->second;
        m_sizeUniform = uniforms.find("u_particleSize")->second;

        //map attributes
        const auto& attribMap = m_shader.getAttribMap();
        m_attribData[0].index = attribMap[Mesh::Position];
        m_attribData[0].attribSize = 3;
        m_attribData[0].offset = 0;

        m_attribData[1].index = attribMap[Mesh::Colour];
        m_attribData[1].attribSize = 4;
        m_attribData[1].offset = 3 * sizeof(float);

        m_attribData[2].index = attribMap[Mesh::Normal]; //actually rotation/scale just using the existing naming convention
        m_attribData[2].attribSize = 3;
        m_attribData[2].offset = (3 + 4) * sizeof(float);
    }
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
    for (auto& e : entities) //TODO frustum cull!
    {
        //check each emitter to see if it should spawn a new particle
        auto& emitter = e.getComponent<ParticleEmitter>();
        if (emitter.m_running &&
            emitter.m_emissionClock.elapsed().asSeconds() > (1.f / emitter.m_emitterSettings.emitRate))
        {
            emitter.m_emissionClock.restart();
            if (emitter.m_nextFreeParticle < emitter.m_particles.size() - 1)
            {
                auto& tx = e.getComponent<Transform>();
                /*auto worldMat = tx.getLocalTransform();
                glm::vec3 forward = glm::vec3(-worldMat[2][0], -worldMat[2][1], -worldMat[2][2]);
                */
                const auto& settings = emitter.m_emitterSettings;
                auto& p = emitter.m_particles[emitter.m_nextFreeParticle];
                p.colour = settings.colour;
                p.forces = settings.forces;
                p.gravity = settings.gravity;
                p.lifetime = p.maxLifeTime = settings.lifetime;
                //p.rotation; //TODO random initial rotation
                //TODO transform initial velocity with parent
                p.velocity = settings.initialVelocity;// glm::vec3(worldMat * glm::vec4(settings.initialVelocity, 1.f));
                
                //spawn particle in world position
                p.position = tx.getWorldPosition();
                p.rotation = Util::Random::value(-Util::Const::TAU, Util::Const::TAU);

                //add random radius placement - TODO how to do with a position table? CAN'T HAVE +- 0!!
                /*p.position.x += Util::Random::value(-settings.spawnRadius.x, settings.spawnRadius.x);
                p.position.y += Util::Random::value(-settings.spawnRadius.y, settings.spawnRadius.y);
                p.position.z += Util::Random::value(-settings.spawnRadius.z, settings.spawnRadius.z);*/

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

            p.rotation += emitter.m_emitterSettings.rotationSpeed * dtSec;
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
        //DPRINT("Next free Particle", std::to_string(emitter.m_nextFreeParticle));

        //TODO sort by depth? should be drawing back to front for transparency really.

        //update VBO
        std::size_t idx = 0;
        for (auto i = 0u; i < emitter.m_nextFreeParticle; ++i)
        {
            const auto& p = emitter.m_particles[i];

            //position
            m_dataBuffer[idx++] = p.position.x;
            m_dataBuffer[idx++] = p.position.y;
            m_dataBuffer[idx++] = p.position.z;

            //colour
            m_dataBuffer[idx++] = p.colour.getRed();
            m_dataBuffer[idx++] = p.colour.getGreen();
            m_dataBuffer[idx++] = p.colour.getBlue();
            m_dataBuffer[idx++] = p.colour.getAlpha();

            //rotation/size
            m_dataBuffer[idx++] = 0.f;
            m_dataBuffer[idx++] = 0.f;
            m_dataBuffer[idx++] = 0.f;
        }
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, idx * sizeof(float), m_dataBuffer.data()));        
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void ParticleSystem::render(Entity camera)
{
    glCheck(glEnable(GL_CULL_FACE));
    glCheck(glEnable(GL_BLEND));
    ENABLE_POINT_SPRITES;
        
    //particles are already in world space so just need viewProj
    const auto& tx = camera.getComponent<Transform>();
    const auto cam = camera.getComponent<Camera>();
    glm::mat4 viewProj = cam.projection * glm::inverse(tx.getWorldTransform());
    glm::vec3 camWorldPos = tx.getWorldPosition();

    auto vp = applyViewport(cam.viewport);

    //bind shader
    glCheck(glUseProgram(m_shader.getGLHandle()));

    //set shader uniforms (texture/projection)
    glCheck(glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, glm::value_ptr(cam.projection)));
    glCheck(glUniformMatrix4fv(m_viewProjUniform, 1, GL_FALSE, glm::value_ptr(viewProj)));
    glCheck(glUniform1f(m_viewportUniform, vp.height));
    glCheck(glUniform1i(m_textureUniform, 0));
    glCheck(glActiveTexture(GL_TEXTURE0));
    

    //foreach entity
    auto& entities = getEntities();
    for (auto& e : entities)
    {
        const auto& emitter = e.getComponent<ParticleEmitter>();
        //bind emitter texture
        glCheck(glBindTexture(GL_TEXTURE_2D, emitter.m_emitterSettings.textureID));
        glCheck(glUniform1f(m_sizeUniform, emitter.m_emitterSettings.size));
        
        //bind emitter vbo
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, emitter.m_vbo));

        //bind vertex attribs
        for (auto i = 0u; i < /*m_attribData.size()*/2; ++i)
        {
            glCheck(glEnableVertexAttribArray(m_attribData[i].index));
            glCheck(glVertexAttribPointer(m_attribData[i].index, m_attribData[i].attribSize,
                GL_FLOAT, GL_FALSE, VertexSize,
                reinterpret_cast<void*>(static_cast<intptr_t>(m_attribData[i].offset))));
        }

        //apply blend mode
        applyBlendMode(emitter.m_emitterSettings.blendmode);

        //draw
        glCheck(glDrawArrays(GL_POINTS, 0, emitter.m_nextFreeParticle));

        //unbind attribs
        for (auto i = 0u; i < /*m_attribData.size()*/2; ++i)
        {
            glCheck(glDisableVertexAttribArray(m_attribData[i].index));
        }
    }

    glCheck(glUseProgram(0));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(glBindTexture(GL_TEXTURE_2D, 0));

    restorePreviousViewport();
    glCheck(glDisable(GL_CULL_FACE));
    glCheck(glDisable(GL_BLEND));
    glCheck(glDisable(GL_DEPTH_TEST));
    glCheck(glDepthMask(GL_TRUE));
    DISABLE_POINT_SPRITES;
}

//private
void ParticleSystem::onEntityAdded(Entity entity)
{
    //check VBO count and increase if needed
    if (m_nextBuffer == m_bufferCount)
    {
        for (auto i = 0u; i < MinParticleSystems; ++i)
        {
            allocateBuffer();
        }
    }

    entity.getComponent<ParticleEmitter>().m_vbo = m_vboIDs[m_nextBuffer];
    m_nextBuffer++;
}

void ParticleSystem::onEntityRemoved(Entity entity)
{
    auto vboID = entity.getComponent<ParticleEmitter>().m_vbo;
    
    //update available VBOs
    std::size_t idx = 0;
    while (m_vboIDs[idx] != vboID) { idx++; }

    std::swap(m_vboIDs[idx], m_vboIDs[m_nextBuffer]);

    m_nextBuffer--;
}

void ParticleSystem::allocateBuffer()
{
    CRO_ASSERT(m_bufferCount < m_vboIDs.size(), "Max Buffers Reached!");
    glCheck(glGenBuffers(1, &m_vboIDs[m_bufferCount]));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[m_bufferCount]));
    glCheck(glBufferData(GL_ARRAY_BUFFER, MaxVertData * sizeof(float), nullptr, GL_DYNAMIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    m_bufferCount++;
}

void ParticleSystem::applyBlendMode(int32 mode)
{
    switch (mode)
    {
    default: break;
    case EmitterSettings::Alpha:
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        break;
    case EmitterSettings::Multiply:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_DST_COLOR, GL_ZERO));
        break;
    case EmitterSettings::Add:
        glCheck(glEnable(GL_DEPTH_TEST));
        glCheck(glDepthMask(GL_FALSE));
        glCheck(glBlendFunc(GL_ONE, GL_ONE));
        break;
    }
}