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

#ifndef CRO_TEXT_RENDERER_HPP_
#define CRO_TEXT_RENDERER_HPP_

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/SDLResource.hpp>

#include <glm/mat4x4.hpp>

#include <vector>

namespace cro
{
    class Text;

    /*!
    \brief Text renderer class.
    Responsible for batching and rendering sprite components.
    */
    class CRO_EXPORT_API TextRenderer final : public System, public Renderable
    {
    public:
        explicit TextRenderer(MessageBus&);
        ~TextRenderer();

        TextRenderer(const TextRenderer&) = delete;
        TextRenderer(TextRenderer&&) = delete;
        const TextRenderer& operator = (const TextRenderer&) = delete;
        TextRenderer& operator = (TextRenderer&&) = delete;

        /*!
        \brief Message handler
        */
        void handleMessage(const Message&) override;
        
        /*!
        \brief Processes the text data into renderable batches
        */
        void process(Time) override;

        /*!
        \brief Draws all the text components in this system's parent Scene
        */
        void render(Entity) override;

    private:

        struct Batch final
        {
            int32 texture = 0; //font texture atlas
            uint32 start = 0; //first vert of this batch in the VBO
            uint32 count = 0; //number of verts in the batch
            Material::BlendMode blendMode = Material::BlendMode::Alpha;
        };
        //maps VBO id to a batch
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
        
        struct ShaderData final
        {
            Shader shader;
            int32 xformUniformIndex = 0;
            int32 textureUniformIndex = 0;
            int32 projectionUniformIndex = 0;
            std::array<AttribData, AttribLocation::Count> attribMap;
        };
        std::array<ShaderData, 2u> m_shaders;

        void fetchShaderData(ShaderData&);
        
        bool m_pendingRebuild;
        bool m_pendingSorting;
        void rebuildBatch();
        void updateVerts(Text&);
        void updateScissor(Text&);

        void applyBlendMode(Material::BlendMode);

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;
    };
}

#endif //CRO_TEXT_RENDERER_HPP_