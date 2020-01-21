/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <map>
#include <set>

namespace cro
{
    class MessageBus;
    class Sprite;
    class Transform;

    /*!
    \brief Batches and renders the scene Sprite components
    */
    class CRO_EXPORT_API SpriteRenderer final : public System, public Renderable
    {
    public:
        /*!
        \brief Constructor.
        \param mb Reference to system message bus
        */
        explicit SpriteRenderer(MessageBus& mb);
        ~SpriteRenderer();
        SpriteRenderer(const SpriteRenderer&) = delete;
        SpriteRenderer(SpriteRenderer&&) = delete;
        const SpriteRenderer& operator = (const SpriteRenderer&) = delete;
        SpriteRenderer& operator = (SpriteRenderer&&) = delete;

        enum class DepthAxis
        {
            Y, Z
        };
        /*!
        \brief Sets the depth sorting axis.
        When the depth axis is set to Y then sprites lower down on the
        screen will appear nearer the front. When set to Z they are sorted
        by the Z depth value of the entity transform. Defaults to Z
        */
        void setDepthAxis(DepthAxis d) { m_depthAxis = d; }

        /*!
        \brief Returns the current DepthAxis setting
        */
        DepthAxis getDepthAxis() const { return m_depthAxis; }

        /*!
        \brief Implements message handling
        */
        void handleMessage(const Message&) override;

        /*!
        \brief Implements the process which performs batching
        */
        void process(Time) override;

        /*!
        \brief Renders the Sprite components
        */
        void render(Entity) override;

    private:
        
        //maps VBO to textures
        struct Batch final
        {
            int32 texture = 0;
            uint32 start = 0;
            uint32 count = 0; //this is the COUNT not the final index
            Material::BlendMode blendMode = Material::BlendMode::Alpha;
        };
        std::vector<std::pair<uint32, std::vector<Batch>>> m_buffers;
        std::vector<std::vector<glm::mat4>> m_bufferTransforms;

        enum AttribLocation
        {
            Position, Colour, UV0, UV1, Count
        };
        struct AttribData final
        {
            uint32 size = 0;
            uint32 location = 0;
            uint32 offset = 0;
        };
        std::array<AttribData, AttribLocation::Count> m_attribMap;

        Shader m_shader;
        int32 m_matrixIndex;
        int32 m_textureIndex;
        int32 m_projectionIndex;

        DepthAxis m_depthAxis;

        bool m_pendingRebuild;
        bool m_pendingSorting;
        void rebuildBatch();

        void updateGlobalBounds(Sprite&, const glm::mat4&);
        void applyBlendMode(Material::BlendMode);

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;
    };
}