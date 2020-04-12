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
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/matrix.hpp>

namespace cro
{
    /*!
    \brief Used to decide by which criteria 2D drawables are sorted.
    Drawables are sorted by the given axis of their transform in the
    following order:
    DepthAxis::Y - Drawables are rendered from top to bottom order,
    so that the lower on the Y axis they are positioned the later they
    are drawn (ie Drawables nearer the bottom will be drawn over the
    top of Drawables nearer the top of the view). Useful for top-down
    tile map style rendering
    DepthAxis::Z - Drawables are drawn in the order in which their Z
    values appear - smaller Z values are drawn first with larget Z values
    drawn over the top. Z values can be set in the entities Transform component.
    */
    enum class DepthAxis
    {
        Y, Z
    };

    /*!
    \brief Renders any entities with a Drawable2D component and Transform component.
    This includes entities with Sprite or Text components, although these require
    their respective systems in the Scene to update their geometry correctly. Usuaully
    these items would be drawn in a 2D world with an orthographic projection, rather
    than mixing items with a 3D world.
    \see Drawable2D
    \see Sprite
    \see Text
    */
    class CRO_EXPORT_API RenderSystem2D final : public cro::System, public cro::Renderable
    {
    public:
        /*!
        \brief Constructor.
        \param mb Reference to the system message bus
        */
        explicit RenderSystem2D(MessageBus & mb);
        ~RenderSystem2D();

        RenderSystem2D(const RenderSystem2D&) = delete;
        RenderSystem2D(RenderSystem2D&&) = default;

        const RenderSystem2D& operator = (const RenderSystem2D&) = delete;
        RenderSystem2D& operator = (RenderSystem2D&&) = default;

        /*!
        \brief Performs frustum culling and Material sorting by depth and blend mode
        */
        void updateDrawList(Camera&) override;

        void process(float) override;

        /*!
        \brief Attempts to render the scene based on the current entity lists
        */
        void render(Entity, const RenderTarget&) override;

        /*!
        \brief Sets whether Drawable components should be sorted by the Y or
        Z value of their Transform.
        \see DepthAxis
        */
        void setSortOrder(DepthAxis order);

    private:

        Shader m_colouredShader;
        Shader m_texturedShader;

        DepthAxis m_sortOrder;

        void applyBlendMode(Material::BlendMode);
        glm::ivec2 mapCoordsToPixel(glm::vec2, const glm::mat4& viewProjMat, IntRect) const;

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;

        void resetDrawable(Entity);
    };
}
