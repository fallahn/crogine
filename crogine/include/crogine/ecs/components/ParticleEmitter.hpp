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
        Colour colour;
        std::uint32_t frameID = 0;
        std::uint32_t loopCount = 0;
        
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 velocity = glm::vec3(0.f);
        glm::vec3 gravity = glm::vec3(0.f);
        float lifetime = 0.f;
        float maxLifeTime = 1.f;
        float frameTime = 0.f;
        float rotation = 0.f;
        float scale = 1.f;
        float acceleration = 1.f;
    };

    /*!
    \brief Encapsulates settings used by an emitter to
    initialise particles it creates
    */
    struct CRO_EXPORT_API EmitterSettings final
    {
        std::string texturePath;
        bool textureSmoothing = false;

        enum BlendMode
        {
            Alpha,
            Add,
            Multiply
        } blendmode = Alpha;
        glm::vec3 gravity = glm::vec3(0.f);
        glm::vec3 initialVelocity = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 spawnOffset = glm::vec3(0.f);

        std::array<glm::vec3, 4> forces{ glm::vec3(0.f) };
        Colour colour = Colour::White;

        std::uint32_t emitCount = 1; //!< amount released at once
        std::int32_t releaseCount = 0; //!< number of particles released before stopping (0 for infinite)
        std::uint32_t frameCount = 1; //!< must be at least one. Texture width is divided by this
        std::uint32_t loopCount = 0;

        float lifetime = 1.f;
        float lifetimeVariance = 0.f;
        float spread = 0.f;
        float scaleModifier = 0.f;
        float acceleration = 1.f;
        float size = 0.05f; //!< diameter of particle
        float emitRate = 10.f; //!< particles per second
        float rotationSpeed = 0.f;
        float spawnRadius = 0.f;
        float framerate = 1.f; //!< must be greater than zero

        std::uint32_t textureID = 0; //!< animated particles should have all frames in a row

        bool randomInitialRotation = true;
        bool inheritRotation = true;

        bool animate = false; //!< If true the particle will attempt to play through all animation frames once.
        bool useRandomFrame = false; //!< If true the particle will pick a frame at random when spawning

        glm::vec2 textureSize = glm::vec2(0.f);

        bool loadFromFile(const std::string&, TextureResource&);
        bool saveToFile(const std::string&); //!< saves the current settings to a config file
    };

    /*!
    \brief Particle Emitter.
    */
    class CRO_EXPORT_API ParticleEmitter final : public Detail::SDLResource
    {
    public:
        ParticleEmitter();

        /*!
        \brief Starts emitting particles with the current settings
        Note that applying custom settings to the emitter will not
        take effect until the next time this is called.
        */
        void start();

        /*!
        \brief Stops the emitter from creating particles
        */
        void stop();

        /*!
        \brief Returns true id stop() has been called on the emitter, or
        if the emitter is currently not running, else returns false.
        */
        const bool stopped() const { return (!m_running && m_nextFreeParticle == 0); }

        /*!
        \brief Returns the current bounding sphere of the emitter
        */
        const Sphere& getBounds() const { return m_bounds; }

        /*!
        \brief Sets the render flags for this emitter.
        If the render flags, when AND'd with the current render flags of the active camera,
        are non-zero then the emitter is drawn, else the emitter is skipped by rendering.
        Defaults to std::numeric_limits<std::uint64_t>::max() (all flags set)
        \see Camera::renderFlags
        */
        void setRenderFlags(std::uint64_t flags) { m_renderFlags = flags; }

        /*!
        \brief Returns the current render flags of this model.
        */
        std::uint64_t getRenderFlags() const { return m_renderFlags; }

        static const std::uint32_t MaxParticles = 10000u;
        EmitterSettings settings;

    private:
        std::uint32_t m_vbo;
        std::uint32_t m_vao; //< used on desktop
        
        //std::array<Particle, MaxParticles> m_particles;
        std::vector<Particle> m_particles;
        std::size_t m_nextFreeParticle;

        bool m_running;
        float m_emissionTime;
        Sphere m_bounds;
        glm::vec3 m_previousPosition; //used to interpolate spawn position if emit rate is > frame rate
        float m_prevTimestamp;
        float m_currentTimestamp;
        float m_emissionTimestamp;

        bool m_pendingUpdate;
        std::uint64_t m_renderFlags;

        std::int32_t m_releaseCount;

        friend class ParticleSystem;
    };
}