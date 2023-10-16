/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/graphics/TextureResource.hpp>
#include <crogine/core/ConfigFile.hpp>

using namespace cro;

ParticleEmitter::ParticleEmitter()
    : m_vbo                 (0),
    m_vao                   (0),
    m_particles             (MaxParticles),
    m_nextFreeParticle      (0),
    m_running               (false),
    m_emissionTime          (0.f),
    m_previousPosition      (0.f),
    m_prevTimestamp         (0.f),
    m_currentTimestamp      (0.f),
    m_emissionTimestamp     (0.f),
    m_pendingUpdate         (true),
    m_renderFlags           (std::numeric_limits<std::uint64_t>::max()),
    m_releaseCount          (-1)
{

}

//void ParticleEmitter::applySettings(const EmitterSettings& es)
//{
//    CRO_ASSERT(es.emitRate > 0, "Emit rate must be grater than 0");
//    CRO_ASSERT(es.lifetime > 0, "Lifetime must be greater than 0");
//    
//    m_emitterSettings = es;
//}

void ParticleEmitter::start()
{
    m_running = true;
    m_emissionTimestamp = m_prevTimestamp;
    m_emissionTime = 0.f;

    if (settings.releaseCount)
    {
        m_releaseCount = settings.releaseCount;
    }
}

void ParticleEmitter::stop()
{
    m_running = false;
    m_releaseCount = -1;
}

bool EmitterSettings::loadFromFile(const std::string& path, cro::TextureResource& textures)
{
    ConfigFile cfg;
    if (!cfg.loadFromFile(path)) return false;

    if (cfg.getName() == "particle_system")
    {
        const auto& properties = cfg.getProperties();
        for (const auto& p : properties)
        {
            auto name = p.getName();
            if (name == "src")
            {
                auto texPath = p.getValue<std::string>();
                if (!texPath.empty())
                {
                    std::replace(texPath.begin(), texPath.end(), '\\', '/');

                    if (texPath[0] == '/')
                    {
                        texPath = texPath.substr(1);
                    }
                    texturePath = texPath;
                    textureID = textures.get(texPath).getGLHandle();
                    textureSize = textures.get(texPath).getSize();  
                    textures.get(texturePath).setSmooth(textureSmoothing);
                    textures.get(texturePath).setRepeated(true);
                }
            }
            else if (name == "texture_smoothing")
            {
                textureSmoothing = p.getValue<bool>();

                if (!texturePath.empty())
                {
                    textures.get(texturePath).setSmooth(textureSmoothing);
                }
            }
            else if (name == "blendmode")
            {
                auto mode = p.getValue<std::string>();
                if (mode == "add")
                {
                    blendmode = EmitterSettings::Add;
                }
                else if (mode == "multiply")
                {
                    blendmode = EmitterSettings::Multiply;
                }
                else
                {
                    blendmode = EmitterSettings::Alpha;
                }
            }
            else if (name == "acceleration")
            {
                acceleration = p.getValue<float>();
            }
            else if (name == "gravity")
            {
                gravity = p.getValue<glm::vec3>();
            }
            else if (name == "velocity")
            {
                initialVelocity = p.getValue<glm::vec3>();
            }
            else if (name == "spread")
            {
                spread = p.getValue<float>() / 2.f; //because the initial rotation is +- this
            }
            else if (name == "lifetime")
            {
                lifetime = p.getValue<float>();
            }
            else if (name == "lifetime_variance")
            {
                lifetimeVariance = p.getValue<float>();
            }
            else if (name == "colour")
            {
                glm::vec4 c = p.getValue<glm::vec4>() / 255.f;
                colour = cro::Colour(c.r, c.g, c.b, c.a);
            }
            else if (name == "random_initial_rotation")
            {
                randomInitialRotation = p.getValue<bool>();
            }
            else if (name == "rotation_speed")
            {
                rotationSpeed = p.getValue<float>();
            }
            else if (name == "scale_affector")
            {
                scaleModifier = p.getValue<float>();
            }
            else if (name == "size")
            {
                size = p.getValue<float>();
            }
            else if (name == "emit_rate")
            {
                emitRate = p.getValue<float>();
            }
            else if (name == "emit_count")
            {
                emitCount = p.getValue<std::uint32_t>();
            }
            else if (name == "spawn_radius")
            {
                spawnRadius = p.getValue<float>();
            }
            else if (name == "spawn_offset")
            {
                spawnOffset = glm::vec3(p.getValue<glm::vec2>(), 0.f);
            }
            else if (name == "release_count")
            {
                releaseCount = p.getValue<std::int32_t>();
            }
            else if (name == "inherit_rotation")
            {
                inheritRotation = p.getValue<bool>();
            }
            else if (name == "frame_count")
            {
                frameCount = std::max(1u, p.getValue<std::uint32_t>());
            }
            else if (name == "loop_count")
            {
                loopCount = std::min(20u, p.getValue<std::uint32_t>());
            }
            else if (name == "animate")
            {
                animate = p.getValue<bool>();
            }
            else if (name == "random_frame")
            {
                useRandomFrame = p.getValue<bool>();
            }
            else if (name == "framerate")
            {
                framerate = p.getValue<float>();
                if (framerate <= 0)
                {
                    framerate = 1.f;
                }
            }
        }

        const auto& objects = cfg.getObjects();
        for (const auto& o : objects)
        {
            //load force array
            if (o.getName() == "forces")
            {
                const auto& props = o.getProperties();
                std::size_t currentForce = 0;
                for (const auto& force : props)
                {
                    if (force.getName() == "force")
                    {
                        forces[currentForce++] = force.getValue<glm::vec3>();
                        if (currentForce == forces.size())
                        {
                            break;
                        }
                    }
                }
            }
        }

#ifdef CRO_DEBUG_
        if (textureID == 0)
        {
            Logger::log(path + ": no texture property found", Logger::Type::Warning);
        }
#endif
        return true;
    }

    return false;
}

bool EmitterSettings::saveToFile(const std::string& path)
{
    auto emitterName = FileSystem::getFileName(path);
    emitterName = emitterName.substr(0, emitterName.size() - 4);

    ConfigFile cfg("particle_system", emitterName);

    auto texPath = texturePath;
    std::replace(texPath.begin(), texPath.end(), '\\', '/');
    if (!texPath.empty())
    {
        if (texPath[0] == '/')
        {
            texPath = texPath.substr(1);
        }
        cfg.addProperty("src", texPath);
    }
    //hmm shame we don't have access to the texture here to read the property
    cfg.addProperty("texture_smoothing").setValue(textureSmoothing);

    if (blendmode == Add)
    {
        cfg.addProperty("blendmode", "add");
    }
    else if (blendmode == Multiply)
    {
        cfg.addProperty("blendmode", "multiply");
    }
    else
    {
        cfg.addProperty("blendmode", "alpha");
    }

    cfg.addProperty("acceleration").setValue(acceleration);
    cfg.addProperty("gravity").setValue(gravity);
    cfg.addProperty("velocity").setValue(initialVelocity);
    cfg.addProperty("spread").setValue(spread * 2.f);
    cfg.addProperty("lifetime").setValue(lifetime);
    cfg.addProperty("lifetime_variance").setValue(lifetimeVariance);
    cfg.addProperty("colour").setValue(glm::vec4(colour.getRedByte(), colour.getGreenByte(), colour.getBlueByte(), colour.getAlphaByte()));
    cfg.addProperty("random_initial_rotation").setValue(randomInitialRotation);
    cfg.addProperty("rotation_speed").setValue(rotationSpeed);
    cfg.addProperty("scale_affector").setValue(scaleModifier);
    cfg.addProperty("size").setValue(size);
    cfg.addProperty("emit_rate").setValue(emitRate);
    cfg.addProperty("emit_count").setValue(emitCount);
    cfg.addProperty("spawn_radius").setValue(spawnRadius);
    cfg.addProperty("spawn_offset").setValue(spawnOffset);
    cfg.addProperty("release_count").setValue(releaseCount);
    cfg.addProperty("inherit_rotation").setValue(inheritRotation);
    cfg.addProperty("frame_count").setValue(frameCount);
    cfg.addProperty("loop_count").setValue(loopCount);
    cfg.addProperty("animate").setValue(animate);
    cfg.addProperty("random_frame").setValue(useRandomFrame);
    cfg.addProperty("framerate").setValue(framerate);

    auto forceObj = cfg.addObject("forces");
    for (const auto& f : forces)
    {
        forceObj->addProperty("force").setValue(f);
    }

    return cfg.save(path);
}