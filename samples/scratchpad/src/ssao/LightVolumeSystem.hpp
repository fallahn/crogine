/*-----------------------------------------------------------------------

Matt Marchant 2023
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
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/detail/glm/vec2.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/GuiClient.hpp>
#endif

#include <vector>
#include <cstdint>
#include <array>

namespace test
{
    struct LightVolume final
    {
        float radius = 1.f;
        cro::Colour colour = cro::Colour::White;
    };

    class LightVolumeSystem final : public cro::System, public cro::Renderable
#ifdef CRO_DEBUG_
        , public cro::GuiClient
#endif
    {
    public:
        struct BufferID final
        {
            enum
            {
                Position, Normal,
                Count
            };
        };

        explicit LightVolumeSystem(cro::MessageBus&);

        LightVolumeSystem(const LightVolumeSystem&) = delete;
        LightVolumeSystem(LightVolumeSystem&&) = delete;

        LightVolumeSystem& operator = (const LightVolumeSystem&) = delete;
        LightVolumeSystem& operator = (LightVolumeSystem&&) = delete;

        void handleMessage(const cro::Message&) override;
        void process(float) override;

        void updateDrawList(cro::Entity camera) override;

        void render(cro::Entity camera, const cro::RenderTarget& target) override {} //we'll be invoking our own function

        void updateBuffer(cro::Entity camera);


        void setSourceBuffer(cro::TextureID, std::int32_t index);

        void setTargetSize(glm::uvec2 size, std::uint32_t scale);
        void setMultiSamples(std::uint32_t samples);
        const cro::Texture& getBuffer() const { return m_renderTexture.getTexture(); }

    private:
        cro::RenderTexture m_renderTexture;
        glm::uvec2 m_bufferSize;
        std::uint32_t m_bufferScale; //texture size is divided by this
        std::uint32_t m_multiSamples;

        cro::Shader m_shader;
        struct UniformID final
        {
            enum
            {
                World,
                ViewProjection,

                PositionMap,
                NormalMap,

                TargetSize,

                LightColour,
                LightPosition,
                LightRadiusSqr,

                Count
            };
        };
        std::array<std::int32_t, UniformID::Count> m_uniformIDs = {};
        std::array<cro::TextureID, BufferID::Count> m_bufferIDs = {};


        std::vector<cro::Entity> m_visibleEntities;


        void resizeBuffer();
    };
}