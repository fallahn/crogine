/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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

#include <crogine/graphics/MaterialData.hpp>
#include <crogine/ecs/System.hpp>
#include <crogine/ecs/Renderable.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/DepthTexture.hpp>

namespace cro
{
    class Texture;

    /*!
    \brief Shadow map renderer.
    Any entities with a shadow caster component and
    appropriate shadow map material assigned to the Model
    component will be rendered by this system into a
    render target. This system should be added to the scene
    before the ModelRenderer system (or any system which
    employs the depth map rendered by this system) in order
    that the depth map data be up to date, and AFTER the 
    CameraSystem so that camera transforms are up to date.

    This system will render into each Camera component's
    depthBuffer target, for each Camera in the current Scene.
    The ShadowMapRenderer uses the Scene's active Sunlight
    property to create a frustum centred to the Camera's
    view frustum and stores it in the depthViewProjection
    property of the camera as a matrix. This matrix, in
    conjunction with the depthBuffer, is used to render a
    directional shadow map for each camera in the Scene with
    a valid depthBuffer.

    Note that the Camera's depthBuffer must be explicitly created:
    any camera without a valid depthBuffer will be skipped.
    \see Camera
    */
    class CRO_EXPORT_API ShadowMapRenderer final : public cro::System, public cro::Renderable
    {
    public:
        /*!
        \brief Constructor.
        \param mb Message bus instance
        */
        explicit ShadowMapRenderer(MessageBus& mb);

        /*!
        \brief Sets the max distance which shadows are drawn
        Note that this is clamped to the far plane distance of
        the active camera. The default is 100 units
        \param distance Maximum world distance to draw shadows.
        Must be greater than 0
        */
        void setMaxDistance(float distance);

        /*!
        \brief Sets the number of cascades to use when rendering
        the shadow map. Currently only the first cascade is implemented
        however this value will affect how much of the MaxDistance is
        covered. For instance setting this to 3 will only cover the
        first third of the MaxDistance value.
        \param count Number of cascades to use. Must be greater than 0
        */
        void setNumCascades(std::int32_t count);

        void process(float) override;

        void updateDrawList(Entity) override;
        void render(Entity, const RenderTarget&) override {};

    private:
        float m_maxDistance;
        std::int32_t m_cascadeCount;
        float m_frustumExtension; //< how much to expand the light frustae by (multiple)
        
        std::vector<Entity> m_activeCameras;
        std::vector<std::vector<std::pair<Entity, float>>> m_drawLists;

        void render();

        void onEntityAdded(cro::Entity) override;
    };
}