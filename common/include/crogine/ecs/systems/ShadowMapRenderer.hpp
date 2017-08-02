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

#ifndef CRO_SHADOWMAP_RENDERER_HPP_
#define CRO_SHADOWMAP_RENDERER_HPP_

#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>

namespace cro
{
    class RenderTexture;

    /*!
    \brief Shadow map renderer.
    Any entities with a shadow caster component and
    appropriate shadow map material assigned to the Model
    component will be rendered by this system into a given
    render target. This system should be added to the scene
    before the ModelRenderer system (or any system which
    employs the depth map rendered by this system) in order
    that the depth map data be up to date.
    */
    class CRO_EXPORT_API ShadowMapRenderer final : public cro::System, public cro::Renderable
    {
    public:
        /*!
        \brief Constructor.
        \param mb Message bus instance
        \param target Reference to a render texture into which the depth map is rendered
        */
        ShadowMapRenderer(MessageBus& mb, RenderTexture& target);

        void process(cro::Time) override;

        void render(Entity) override;

        /*!
        \brief Sets the offset of the Scene's sunlight object relative to the camera.
        Use this to best align the shadow map with the visible scene. This value is added
        to the camera's current position.
        */
        void setProjectionOffset(glm::vec3);


    private:
        RenderTexture& m_target;
        std::vector<Entity> m_visibleEntities;
        glm::vec3 m_projectionOffset;
    };
}

#endif //CRO_SHADOWMAP_RENDERER_HPP_