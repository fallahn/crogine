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
    /*!
    \brief System for rendering dynamic light maps via LightVolume geometry.

    Light volumes can be rendered via entities with a Mesh component and LightVolume
    component. The mesh component is used to define the volume in 3D space and should
    be a sphere with greater or equal radius as the LightVolume. The LightVolume
    component is then used to define the radius if the falloff and the light colour.

    The LightVolumeSystem works in screen space and require the input of two scene
    buffers containing the position and normal data of the scene, usually output
    from a MultiRenderTexture. The position and normal data can be in either world
    or view space, but both buffers should be the same. When the LightVolumeSystem
    is constructed pass either LightVolume::WorldSpace or LightVolume::ViewSpace to
    the constructor so that the LightVolumeSystem can create the correct shaders
    needed to render the light map.

    Once the lightmap is rendered it can be blended additively with the appropriate
    scene using a final pass.
    */
    class CRO_EXPORT_API LightVolumeSystem final : public System, public Renderable
#ifdef CRO_DEBUG_
        , public GuiClient
#endif
    {
    public:
        /*!
        \brief Used to index the source buffers
        */
        struct BufferID final
        {
            enum
            {
                Position, Normal,
                Count
            };
        };

        /*!
        \brief Constructor
        \param mb A reference to the applications' active MessageBus
        \param spaceIndex Either LightVolume::WorldSpace or LightVolume::ViewSpace
        depending on the coordinate space of the input buffers.
        */
        LightVolumeSystem(MessageBus&, std::int32_t spaceIndex);

        LightVolumeSystem(const LightVolumeSystem&) = delete;
        LightVolumeSystem(LightVolumeSystem&&) = delete;

        LightVolumeSystem& operator = (const LightVolumeSystem&) = delete;
        LightVolumeSystem& operator = (LightVolumeSystem&&) = delete;

        void process(float) override;

        /*
        Called by the scene to cull the light objects for rendering
        */
        void updateDrawList(Entity camera) override;

        /*
        Unused. See below
        */
        void render(Entity camera, const RenderTarget& target) override {}

        /*!
        \brief Updates the lightmap buffer by rendering the visible lights
        This needs to be called explicitly after Scene has been rendered to
        the source buffers for position and normal data.
        \param camera The entity containing the Camera component used to
        create the active source buffers
        \param dest The destination RenderTexture for the lightmap output
        */
        void updateTarget(Entity camera, RenderTexture& dest);

        /*!
        \brief Sets the TextureID of the given source buffer
        \param id The TextureID of the source buffer
        \param index Either BufferID::Position or BufferID::Normal to describe
        the type of data in the given buffer.
        */
        void setSourceBuffer(TextureID id, std::int32_t index);


    private:
        std::int32_t m_spaceIndex;

        Shader m_shader;
        struct UniformID final
        {
            enum
            {
                World,
                View,
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
        std::vector<std::vector<Entity>> m_drawLists;
    };
}