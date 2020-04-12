/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <crogine/detail/glm/vec3.hpp>

#include <array>

namespace cro
{
    class TextureResource;

    /*!
    \brief Data struct for a single particle
    */
    struct CRO_EXPORT_API Particle final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 velocity = glm::vec3(0.f);
        glm::vec3 gravity = glm::vec3(0.f);
        float lifetime = 0.f;
        float maxLifeTime = 1.f;
        Colour colour;
        float rotation = 0.f;
        float scale = 1.f;
    };

    /*!
    \brief Encapsulates settings used by an emitter to
    initialise particles it creates
    */
    struct CRO_EXPORT_API EmitterSettings final
    {
        enum
        {
            Alpha,
            Add,
            Multiply
        } blendmode = Alpha;
        glm::vec3 gravity = glm::vec3(0.f);
        glm::vec3 initialVelocity = glm::vec3(0.f);
        std::array<glm::vec3, 4> forces{ glm::vec3(0.f) };
        float lifetime = 1.f;
        float lifetimeVariance = 0.f;
        Colour colour;
        float rotationSpeed = 0.f;
        float scaleModifier = 0.f;
        float size = 1.f; //diameter of particle
        float emitRate = 1.f; //< particles per second
        float spawnRadius = 0.f;
        uint32 textureID = 0;
        bool loadFromFile(const std::string&, TextureResource&);
    };

    /*!
    \brief Particle Emitter.
    */
    class CRO_EXPORT_API ParticleEmitter final : public Detail::SDLResource
    {
    public:
        ParticleEmitter();
        /*~ParticleEmitter();*/

        //void applySettings(const EmitterSettings&);

        void start();
        void stop();

        static const uint32 MaxParticles = 1000u;
        EmitterSettings emitterSettings;

    private:
        uint32 m_vbo;
        uint32 m_vao; //< used on desktop
        
        std::array<Particle, MaxParticles> m_particles;
        std::size_t m_nextFreeParticle;

        bool m_running;
        Clock m_emissionClock;
        Sphere m_bounds;

        friend class ParticleSystem;
    };
}