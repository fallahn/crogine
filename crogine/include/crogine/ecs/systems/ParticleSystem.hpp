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

#include <vector>

namespace cro
{
    /*!
    \brief Particle system.
    Updates and renders all particle emitters in the scene
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

        void process(Time) override;

        void render(Entity) override;

    private:
        
        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

        std::vector<float> m_dataBuffer;
        std::vector<uint32> m_vboIDs;
        std::size_t m_nextBuffer;
        std::size_t m_bufferCount;

        std::size_t m_visibleCount;
        std::vector<Entity> m_visibleSystems;
        void allocateBuffer();

        Shader m_shader;
        int32 m_projectionUniform;
        int32 m_textureUniform;
        int32 m_viewProjUniform;
        int32 m_viewportUniform;
        int32 m_sizeUniform;

        struct AttribData final
        {
            int32 index = 0;
            int32 attribSize = 0;
            int32 offset = 0;
        };
        std::array<AttribData, 3u> m_attribData;
    };
}