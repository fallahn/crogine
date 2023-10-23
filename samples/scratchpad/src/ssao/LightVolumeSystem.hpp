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

namespace cro
{
    struct LightVolume final
    {
        float radius = 1.f;
        Colour colour = Colour::White;
    };

    class LightVolumeSystem final : public System, public Renderable
#ifdef CRO_DEBUG_
        , public GuiClient
#endif
    {
    public:
        struct BufferID final
        {
            enum
            {
                //hmm actually we don't need the colour buffer
                Colour, Position, Normal,
                Count
            };
        };

        explicit LightVolumeSystem(MessageBus&);

        LightVolumeSystem(const LightVolumeSystem&) = delete;
        LightVolumeSystem(LightVolumeSystem&&) = delete;

        LightVolumeSystem& operator = (const LightVolumeSystem&) = delete;
        LightVolumeSystem& operator = (LightVolumeSystem&&) = delete;

        void handleMessage(const Message&) override;
        void process(float) override;

        void updateDrawList(Entity camera) override;

        void render(Entity camera, const RenderTarget& target) override {} //we'll be invoking our own function

        void updateBuffer(Entity camera);


        void setSourceBuffer(TextureID, std::int32_t index);

        void setTargetSize(glm::uvec2 size, std::uint32_t scale);
        const Texture& getBuffer() const { return m_renderTexture.getTexture(); }

    private:
        cro::RenderTexture m_renderTexture;
        glm::uvec2 m_bufferSize;
        std::uint32_t m_bufferScale; //texture size is divided by this

        Shader m_shader;
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
        std::array<TextureID, BufferID::Count> m_bufferIDs = {};


        std::vector<Entity> m_visibleEntities;


        void resizeBuffer();
    };
}