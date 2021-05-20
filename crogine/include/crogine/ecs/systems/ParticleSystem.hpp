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

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>

#include <vector>

namespace cro
{
    /*!
    \brief Particle system.
    Updates and renders all particle emitters in the scene.
    Particle systems are renderable, but do write to the depth buffer.
    They do, however, read from the the depth buffer, and so should be
    added to a Scene after any other render systems such as the
    ModelRenderer. This allows for correct blending of alpha transparent
    particle systems.
    */
    class CRO_EXPORT_API ParticleSystem final : public Renderable, public System
    {
    public:
        explicit ParticleSystem(MessageBus&);
        ~ParticleSystem();

        ParticleSystem(const ParticleSystem&) = delete;
        ParticleSystem(ParticleSystem&&) = delete;
        const ParticleSystem operator = (const ParticleSystem&) = delete;
        ParticleSystem& operator = (ParticleSystem&&) = delete;

        void updateDrawList(Entity) override;

        void process(float) override;

        void render(Entity, const RenderTarget&) override;

    private:
        
        std::array<std::vector<Entity>, 2u> m_visibleEntities;

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

        std::vector<float> m_dataBuffer;
        std::vector<std::uint32_t> m_vboIDs;
        std::vector<std::uint32_t> m_vaoIDs; //< used on desktop
        std::size_t m_nextBuffer;
        std::size_t m_bufferCount;

        //this is a fallback texture for untextured systems.
        //probably not less optimal than switching between
        //textured and untextured shaders.
        cro::Texture m_fallbackTexture;

        void allocateBuffer();

        Shader m_shader;

        enum UniformID
        {
            ClipPlane,
            ViewProjection,
            Projection,
            Texture,
            TextureSize,
            Viewport,
            ParticleSize,
            FrameCount,
            DepthMap,

            Count
        };
        std::array<std::int32_t, UniformID::Count> m_uniformIDs = {};

        struct AttribData final
        {
            std::int32_t index = 0;
            std::int32_t attribSize = 0;
            std::int32_t offset = 0;
        };
        std::array<AttribData, 3u> m_attribData;
    };
}