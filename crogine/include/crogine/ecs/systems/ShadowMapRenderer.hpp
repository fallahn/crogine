/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/GuiClient.hpp>
#endif

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

    The quality of shadow mapping can vary wildly depending on
    the scene being drawn. Two Camera properties can and
    should be tweaked on a per-use case:
    Camera::maxShadowDistance() - this reduces the distance
    shadows are rendered from the camera, but effectively
    increases the resolution of the shadow map by covering a
    smaller area. If shadows are particularly blocky then
    reducing this could improve quality without the need 
    for creating a higher resolution DepthTexture on the camera.
    However:
    Reducing the draw distance of shadows can have the effect
    of 'lifting' the frustum created by the light as it is
    shrunk, so objects may start to fall outside of the
    shadow casting frustum. In which case increasing the
    length of the frustum can be done with 
    Camera::setShadowExpansion(), which pushes the light
    frustum's far plane back by the given amount.
    This means it is skewed slightly towards the near plane
    (where the depth buffer has higher resolution) so that
    it can also improve the quality of shadows, particularly
    self shadows, where the depth difference is very small.

    Note that the Camera's depthBuffer must be explicitly created:
    any camera without a valid depthBuffer will be skipped.
    \see Camera
    */
    class CRO_EXPORT_API ShadowMapRenderer final : public System, public Renderable
#ifdef CRO_DEBUG_
        , public GuiClient
#endif
    {
    public:
        /*!
        \brief Constructor.
        \param mb Message bus instance
        */
        explicit ShadowMapRenderer(MessageBus& mb);

        /*!
        \brief DEPRECATED This function does nothing.
        Use Camera::setMaxShadowDistance() instead.
        */
        [[deprecated("Use Camera::setMaxShadowDistance()")]]
        void setMaxDistance(float distance);

        /*!
        \brief DEPRECATED This function does nothing.
        The number of cascades is dictated by the number
        of frustum splits created with Camera::setPerspective()
        or Camera::setOrthographic()
        */
        [[deprecated("Use numSplits with Camera::setPerspective()")]]
        void setNumCascades(std::int32_t count);

        /*!
        \brief Sets the render interval
        Sets N where the shadow maps are rendered every Nth frame.
        Must be greater than 0
        */
        void setRenderInterval(std::uint32_t interval) { m_interval = std::max(interval, 1u); }

        void process(float) override;

        void updateDrawList(Entity) override;
        void render(Entity, const RenderTarget&) override {};

    private:
        std::uint32_t m_interval;
        
        std::vector<Entity> m_activeCameras;

        struct Drawable final
        {
            Drawable(Entity e, float d)
                : entity(e), distance(d) {}
            Entity entity;
            float distance = 0.f;
        };
        //for each camera, for each camera cascade, a vector of entities
        std::vector<std::vector<std::vector<Drawable>>> m_drawLists;

        //buffer to render first pass blur if soft shadowing
        DepthTexture m_blurBuffer;
        SimpleQuad m_inputQuad;
        SimpleQuad m_outputQuad;

        cro::Shader m_blurShaderA;
        cro::Shader m_blurShaderB;

        void render();

        void onEntityAdded(Entity) override;
        void onEntityRemoved(Entity) override;
        void flushEntity(Entity) override;
    };
}